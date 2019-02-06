#pragma once
#include<atomic>
#include<cinttypes>
#include<malloc.h>
#include<cstring>
#include<string>

namespace Succinct{
  template<class BlockType>
  class L0BlockBase{
    friend class L0Block;
    friend class L0BlockCounter;
  public:
    constexpr static size_t mBitsPerL1=2048;

  protected:
    BlockType                  *mNextBlock=nullptr;
    BlockType                  *mPrevBlock=nullptr;

    uint32_t                    mAllocatedSize=0;
    const uint32_t              mBitCountReserve;

    std::atomic<std::uint64_t*> mBitArrayAddr=nullptr;

  protected:
    L0BlockBase(uint32_t reserveBitCount):mBitCountReserve(reserveBitCount){
      if(reserveBitCount%mBitsPerL1)
        throw std::runtime_error("Succinct::L0Block: Reserve bit count must be a multible of "+std::to_string(mBitsPerL1));
    }


    inline    auto       GetQuadword(uint64_t pos){

      auto t=mBitArrayAddr.load(std::memory_order_consume)+(pos/64);
      return reinterpret_cast<std::atomic<uint64_t>*>(t);
    }

    inline    auto       GetQuadword(uint64_t pos)const{

      auto t=mBitArrayAddr.load(std::memory_order_consume)+(pos/64);
      return reinterpret_cast<std::atomic<uint64_t>*>(t);
    }
  public:

    void                 Append(BlockType *block){
      if(mNextBlock!=nullptr)
        throw std::runtime_error("Block is not the last block in the list");

      mNextBlock=block;
      block->mPrevBlock=static_cast<BlockType*>(this);

    }

    /*** For removing pointer tags if they are set ***/
    constexpr BlockType* Next(){               return (BlockType*)((uint64_t)mNextBlock&0x7FFFFFFFFFFFFFFFULL); }
    constexpr BlockType* Next()const{          return (BlockType*)((uint64_t)mNextBlock&0x7FFFFFFFFFFFFFFFULL); }

    constexpr BlockType* Prev(){               return (BlockType*)((uint64_t)mPrevBlock&0x7FFFFFFFFFFFFFFFULL); }
    constexpr BlockType* Prev()const{          return (BlockType*)((uint64_t)mPrevBlock&0x7FFFFFFFFFFFFFFFULL); }

    constexpr size_t     Size()const{          return mBitCountReserve; }

    constexpr size_t     AllocatedSize()const{ return mAllocatedSize; }

    constexpr uint64_t*  Data(){               return mBitArrayAddr; }
  };

  class L0BlockCount;

  class L0Block: public L0BlockBase<L0Block>{
  protected:
    void                 Allocate();

  public:
    L0Block(uint32_t reserveBitCount):L0BlockBase(reserveBitCount){}

    L0Block(const L0Block &rhs);

    L0Block(const L0BlockCounter &rhs);

    ~L0Block();

    bool                 Get(uint64_t pos)const;

    void                 Set(uint64_t pos);

    void                 Clear(uint64_t pos);

    uint64_t             GetGroup(uint64_t pos,uint64_t groupSize)const;

    void                 SetGroup(uint64_t pos,uint64_t val,uint64_t groupSize);

    void                 ClearGroup(uint64_t pos,uint64_t val,uint64_t groupSize);

    uint64_t             PopCount()const;
  };

  class L0BlockCounter:public L0BlockBase<L0BlockCounter>{
  protected:
    constexpr static uint32_t       NOTDIRTY=0xFFFFFFFFUL;


    /***
        The counters are front loaded into the bitarray,
        with mBitArrayAddr just being an offset past the end of the counters
        L1/L2 counter layout
        [10bit L2]0[10bit L2]0[10bit L2][32bit L1]

        Only 3 L2 counters are used the last qroup is done through a normal popcount

        Each L2 counter is 512bits (8 quadwords)
        L1 counters track a total of 2048bits (32 quadwords),

    ***/
    mutable std::atomic<uint64_t*>  mCountersAddr=nullptr;
    std::atomic<uint64_t>           mL0Counter=0;


    mutable uint32_t                mDirtyBitOffset=NOTDIRTY;

  private:
    constexpr uint64_t   GroupMask(uint64_t value,uint64_t pos,uint64_t groupSize){
      uint64_t groupMask=~(0xFFFFFFFFFFFFFFFFULL<<groupSize);
      return   (value&groupMask)<<(pos&0x000000000000003FULL);
    }
  protected:
    void                 Allocate();

  public:
    L0BlockCounter(uint32_t reserveBitCount):L0BlockBase(reserveBitCount){}

    L0BlockCounter(const L0BlockCounter &rhs);

    L0BlockCounter(const L0Block &rhs);

    ~L0BlockCounter();

    inline    uint64_t   BaseCounter()const{ return mL0Counter; }
    
    inline    void       BaseCounter(uint64_t count){ mL0Counter=count; }

    void                 RebuildCounters()const;

    uint64_t             Rank(uint64_t pos)const;

    uint64_t             Select(uint64_t count);

    bool                 Get(uint64_t pos)const;

    void                 Set(uint64_t pos);

    void                 Clear(uint64_t pos);

    uint64_t             GetGroup(uint64_t pos,size_t groupSize)const;

    void                 SetGroup(uint64_t pos,uint64_t val,size_t groupSize);

    void                 ClearGroup(uint64_t pos,uint64_t val,size_t groupSize);

    uint64_t             PopCount()const;
  };
}