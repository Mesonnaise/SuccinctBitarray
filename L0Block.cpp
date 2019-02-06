#include<stdexcept>
#include<string>
#include<algorithm>
#include<intrin.h>

#include"Util.h"
#include"L0Block.h"

namespace Succinct{

  //*** L0Block class ***//

  void L0Block::Allocate(){
    uint64_t *cmp=mBitArrayAddr.load(std::memory_order_acquire);
    uint64_t *flag=reinterpret_cast<uint64_t*>(0x8000000000000000ULL);

    if(cmp==nullptr||cmp==flag){
      //Race to see which thread gets to allocate the memory.
      //Loser spin locks until the 0x8000000000000000ULL flag is cleared.

      cmp=nullptr;

      if(mBitArrayAddr.compare_exchange_strong(cmp,flag,std::memory_order_seq_cst)){
        uint32_t size=mBitCountReserve/8;

        uint64_t *tmpPointer=static_cast<uint64_t*>(_mm_malloc(size,32));

        if(tmpPointer){
          mBitArrayAddr.store(tmpPointer,std::memory_order_release);
          mAllocatedSize=static_cast<uint32_t>(_aligned_msize(tmpPointer,32,0));
        } else
          throw std::runtime_error("Succinct::L0Block: Failed to allocate "+std::to_string(size)+" bytes");

      } else{
        uint64_t backOff=2;

        while(flag==mBitArrayAddr.load(std::memory_order_consume)&&mAllocatedSize==0){

          for(uint64_t i=0;i<backOff;i++)
            std::atomic_thread_fence(std::memory_order_relaxed);

          backOff<<=1;
        }
      }
    }
  }

  L0Block::L0Block(const L0Block &rhs):L0BlockBase(rhs.mBitCountReserve){
    if(rhs.AllocatedSize()){
      Allocate();

      std::memcpy(mBitArrayAddr,rhs.mBitArrayAddr,rhs.AllocatedSize());
    }
  }

  L0Block::L0Block(const L0BlockCounter &rhs):L0BlockBase(rhs.mBitCountReserve){
    if(rhs.AllocatedSize()){
      Allocate();

      std::memcpy(mBitArrayAddr,rhs.mBitArrayAddr,AllocatedSize());
    }
  }

  L0Block::~L0Block(){
    if(mBitArrayAddr&&mAllocatedSize)
      _mm_free(mBitArrayAddr);
  }

  bool L0Block::Get(uint64_t pos)const{
    if(mAllocatedSize==0)
      return false;

    auto value=std::atomic_load_explicit(
      GetQuadword(pos),
      std::memory_order_consume);

    return value>>(pos&0x000000000000003FULL)&0x0000000000000001ULL;
  }

  void L0Block::Set(uint64_t pos){
    if(mAllocatedSize==0)
      Allocate();

    std::atomic_fetch_or_explicit(
      GetQuadword(pos),
      0x0000000000000001ULL<<(pos&0x000000000000003FULL),
      std::memory_order_seq_cst);
  }

  void L0Block::Clear(uint64_t pos){
    if(mAllocatedSize==0)
      return;

    std::atomic_fetch_xor_explicit(
      GetQuadword(pos),
      ~(0x0000000000000001ULL<<(pos&0x000000000000003FULL)),
      std::memory_order_seq_cst);
  }

  uint64_t L0Block::GetGroup(uint64_t pos,uint64_t groupSize)const{
    if(mAllocatedSize==0)
      return 0;

    const uint64_t  groupMask=~(0xFFFFFFFFFFFFFFFFULL<<groupSize);

    auto value=std::atomic_load_explicit(
      GetQuadword(pos),
      std::memory_order_consume);

    return value>>(pos&0x000000000000003FULL)&groupMask;
  }

  void L0Block::SetGroup(uint64_t pos,uint64_t val,uint64_t groupSize){
    if(mBitArrayAddr==nullptr)
      Allocate();

    const uint64_t  groupMask=~(0xFFFFFFFFFFFFFFFFULL<<groupSize);

    val&=groupMask;

    std::atomic_fetch_or_explicit(
      GetQuadword(pos),
      val<<(pos&0x000000000000003FULL),
      std::memory_order_seq_cst);
  }

  void L0Block::ClearGroup(uint64_t pos,uint64_t val,uint64_t groupSize){
    if(mAllocatedSize==0)
      return;

    const uint64_t  groupMask=~(0xFFFFFFFFFFFFFFFFULL<<groupSize);

    val&=groupMask;

    std::atomic_fetch_xor_explicit(
      GetQuadword(pos),
      ~val,
      std::memory_order_seq_cst);
  }

  uint64_t L0Block::PopCount()const{
    uint64_t rollCount=0;

    for(uint64_t idx=0;idx<mBitCountReserve/64;idx++)
      rollCount+=__popcnt64(*(mBitArrayAddr+idx));

    return rollCount;
  }


  //*** L0BlockCounter class ***//

  L0BlockCounter::L0BlockCounter(const L0BlockCounter &rhs):L0BlockBase(rhs.mBitCountReserve){
    if(rhs.AllocatedSize()){
      Allocate();

      std::memcpy(mCountersAddr,rhs.mCountersAddr,rhs.AllocatedSize());
    }

    mL0Counter=rhs.BaseCounter();
  }

  L0BlockCounter::L0BlockCounter(const L0Block &rhs):L0BlockBase(rhs.mBitCountReserve){
    if(rhs.AllocatedSize()){
      Allocate();

      std::memcpy(mBitArrayAddr,rhs.mBitArrayAddr,rhs.AllocatedSize());

      mDirtyBitOffset=0;
      RebuildCounters();
    }
  }

  L0BlockCounter::~L0BlockCounter(){
    if(mBitArrayAddr&&mAllocatedSize)
      _mm_free(mCountersAddr);
  }

  void L0BlockCounter::Allocate(){
    uint64_t *cmp=mBitArrayAddr.load(std::memory_order_acquire);
    uint64_t *flag=reinterpret_cast<uint64_t*>(0x8000000000000000ULL);

    if(cmp==nullptr||cmp==flag){
      //Race to see which thread gets to allocate the memory.
      //Loser spin locks until the 0x8000000000000000ULL flag is cleared.

      cmp=nullptr;

      if(mBitArrayAddr.compare_exchange_strong(cmp,flag,std::memory_order_seq_cst)){

        uint32_t L1GroupCount=(mBitCountReserve+mBitsPerL1-1)/mBitsPerL1;

        uint32_t size=L1GroupCount*mBitsPerL1/8;

        //Round up to a multible of 4
        L1GroupCount=(L1GroupCount+3)&0xFFFFFFFFFFFFFFFCULL;

        size+=L1GroupCount*8;

        uint64_t *tmpPointer=static_cast<uint64_t*>(_mm_malloc(size,32));

        if(tmpPointer){

          mCountersAddr.store(tmpPointer,std::memory_order_release);
          mBitArrayAddr.store(tmpPointer+L1GroupCount,std::memory_order_release);

          mAllocatedSize=static_cast<uint32_t>(_aligned_msize(tmpPointer,32,0));

        } else
          throw std::runtime_error("Succinct::L0Block: Failed to allocate memory");

      } else{
        uint64_t backOff=2;

        while(flag==mBitArrayAddr.load(std::memory_order_consume)&&mAllocatedSize==0){

          for(uint64_t i=0;i<backOff;i++)
            std::atomic_thread_fence(std::memory_order_relaxed);

          backOff<<=1;
        }
      }
    }
  }

  void L0BlockCounter::RebuildCounters()const{

    uint64_t *countAddr=mCountersAddr+(mDirtyBitOffset/mBitsPerL1);
    uint64_t *countAddrEnd=mCountersAddr+(mBitCountReserve+(mBitsPerL1-1))/mBitsPerL1;

    uint64_t *bitAddr=mBitArrayAddr+(mDirtyBitOffset/64);

    uint64_t rollCount=*countAddr&0x00000000FFFFFFFFULL;

    uint64_t L2Count=0;
    uint64_t counters=0;

    while(countAddr<countAddrEnd){
      counters=rollCount;

      for(int L2Idx=32;L2Idx<62;L2Idx+=11){
        L2Count=0;

        for(int i=0;i<4;i++)
          L2Count+=__popcnt64(*bitAddr++);

        rollCount+=L2Count;

        counters|=L2Count<<L2Idx;
      }

      for(int i=0;i<4;i++)
        rollCount+=__popcnt64(*bitAddr++);

      *countAddr++=counters;
    }

    mDirtyBitOffset=NOTDIRTY;

    auto nextBlock=Next();

    if(nextBlock){
      int64_t delta=int64_t(rollCount+mL0Counter)-nextBlock->mL0Counter;
      do{
        nextBlock->mL0Counter+=delta;
        nextBlock=nextBlock->Next();
      }while(nextBlock);
    }
  }

  uint64_t L0BlockCounter::Rank(uint64_t pos)const{
    if(mAllocatedSize==0)
      return 0;

    if(mDirtyBitOffset!=NOTDIRTY)
      RebuildCounters();

    uint64_t *counter=mCountersAddr.load(std::memory_order_relaxed);
    uint64_t L1L2Counter=*(counter+(pos/mBitsPerL1));

    uint64_t rollCount=L1L2Counter&0x00000000FFFFFFFFULL;

    L1L2Counter>>=32;

    uint64_t L2Mask=pos/(mBitsPerL1/4)&0x0000000000000003ULL;
    L2Mask=~(0xFFFFFFFFFFFFFFFFULL<<(L2Mask*11));

    //Horizontal sum L2 counters, the result of the sum is in the upper 16bits
    rollCount+=((L1L2Counter&L2Mask)*0x0001000000400801ULL)>>48;

    uint64_t *bitArrayAddr=mBitArrayAddr+((pos/64)&0xFFFFFFFFFFFFFFF8ULL);
    uint64_t *bitArrayAddrEnd=mBitArrayAddr+(pos/64);

    while(bitArrayAddr<bitArrayAddrEnd)
      rollCount+=__popcnt64(*bitArrayAddr++);

    uint64_t mask=0xFFFFFFFFFFFFFFFFULL<<((pos&0x000000000000003FULL)+1);
    rollCount+=__popcnt64(*bitArrayAddr&~mask);

    return mL0Counter+rollCount;
  }

  uint64_t L0BlockCounter::Select(uint64_t count){
    if(mDirtyBitOffset!=NOTDIRTY)
      RebuildCounters();

    uint64_t *L1Addr=mCountersAddr.load(std::memory_order_relaxed);
    uint64_t *L1AddrEnd=L1Addr+(mBitCountReserve+(mBitsPerL1-1))/mBitsPerL1;

    uint64_t rollposition=0;

    count-=mL0Counter;

    while(count>(*L1Addr&0x00000000FFFFFFFFULL)&&L1Addr<L1AddrEnd){
      count-=*L1Addr++&0x00000000FFFFFFFFULL;
      rollposition+=mBitsPerL1;
    }

    uint64_t L2=*L1Addr>>32;
    L2=(L2    &0x00000000000003FFULL)|
      ((L2<<5)&0x0000000003FF0000ULL)|
      ((L2<<10)&0x000003FF00000000ULL);


    uint64_t L2Sums=L2*0x0001000100010001ULL;
    uint64_t L2Comp=count*0x0001000100010001ULL;
    uint64_t ret=(L2Sums|0x0000800080008000ULL)-L2Comp;

    uint64_t L2Idx=3-__popcnt64(ret&0x0000800080008000ULL);

    rollposition+=(mBitsPerL1/4)*L2Idx;
    count-=((L2Sums<<16)>>L2Idx*16)&0x00000000000003FFULL;

    uint64_t *bitAddr=mBitArrayAddr.load(std::memory_order_relaxed);
    uint64_t *bitAddrEnd=bitAddr+(mBitCountReserve+63)/64;
    bitAddr+=(rollposition/64);

    uint64_t popcnt=__popcnt64(*bitAddr);

    while(count>popcnt&&bitAddr<bitAddrEnd){
      count-=popcnt;
      rollposition+=64;
      popcnt=__popcnt64(*++bitAddr);
    }

    rollposition+=BwordSelect(*--bitAddr,count);

    return rollposition;
  }

  bool L0BlockCounter::Get(uint64_t pos)const{
    if(mAllocatedSize==0)
      return false;

    auto value=std::atomic_load_explicit(
      GetQuadword(pos),
      std::memory_order_consume);

    return value>>(pos&0x000000000000003FULL)&0x0000000000000001ULL;
  }

  void L0BlockCounter::Set(uint64_t pos){
    if(mBitArrayAddr==nullptr)
      Allocate();

    uint64_t mask=0x0000000000000001ULL<<(pos&0x000000000000003FULL);

    auto value=std::atomic_fetch_or_explicit(
      GetQuadword(pos),
      mask,
      std::memory_order_seq_cst);

    if((value&mask)==0)
      mDirtyBitOffset=std::min(mDirtyBitOffset,static_cast<uint32_t>(pos));
  }

  void L0BlockCounter::Clear(uint64_t pos){
    if(mAllocatedSize==0)
      return;

    uint64_t mask=0x0000000000000001ULL<<(pos&0x000000000000003FULL);

    auto value=std::atomic_fetch_xor_explicit(
      GetQuadword(pos),
      ~(0x0000000000000001ULL<<(pos&0x000000000000003FULL)),
      std::memory_order_seq_cst);

    if((value&mask)>0)
      mDirtyBitOffset=std::min(mDirtyBitOffset,static_cast<uint32_t>(pos));
  }

  uint64_t L0BlockCounter::GetGroup(uint64_t pos,size_t groupSize)const{
    if(mAllocatedSize==0)
      return 0;

    auto value=std::atomic_load_explicit(
      GetQuadword(pos),
      std::memory_order_consume);

    uint64_t groupMask=~(0xFFFFFFFFFFFFFFFFULL<<groupSize);

    return value>>(pos&0x000000000000003FULL)&groupMask;
  }

  void L0BlockCounter::SetGroup(uint64_t pos,uint64_t val,size_t groupSize){
    if(mBitArrayAddr==nullptr)
      Allocate();

    uint64_t mask=GroupMask(val,pos,groupSize);

    auto value=std::atomic_fetch_or_explicit(
      GetQuadword(pos),
      mask,
      std::memory_order_seq_cst);

    if((value&mask)==0)
      mDirtyBitOffset=std::min(mDirtyBitOffset,static_cast<uint32_t>(pos));
  }

  void L0BlockCounter::ClearGroup(uint64_t pos,uint64_t val,size_t groupSize){
    if(mAllocatedSize==0)
      return;

    uint64_t mask=GroupMask(val,pos,groupSize);

    auto value=std::atomic_fetch_xor_explicit(
      GetQuadword(pos),
      ~mask,
      std::memory_order_seq_cst);

    if((value&mask)>0)
      mDirtyBitOffset=std::min(mDirtyBitOffset,static_cast<uint32_t>(pos));
  }

  uint64_t L0BlockCounter::PopCount()const{
    if(mBitArrayAddr==nullptr)
      return 0;

    return Rank(mBitCountReserve-1);
  }
}