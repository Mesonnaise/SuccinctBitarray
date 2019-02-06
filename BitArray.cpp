#include<algorithm>
#include"BitArray.h"

namespace Succinct{

  BitArray::BitArray(const BitArray &rhs):BitArrayBase(rhs.mTotalBits){
    auto rhsBlock=rhs.mRootBlock.get();

    if(rhsBlock){
      L0Block *newBlock=new L0Block(*rhsBlock);
      mRootBlock.reset(newBlock,&BlockDeleter);
 
      mCurrentBlock=newBlock;
      rhsBlock=rhsBlock->Next();
    }

    while(rhsBlock){
      L0Block *newBlock=new L0Block(*rhsBlock);
      mCurrentBlock->Append(newBlock);
      mCurrentBlock=newBlock;
      rhsBlock=rhsBlock->Next();
    }

    mCurrentBlock=mRootBlock.get();
  }

  BitArray::BitArray(const BitArrayCounter &rhs):BitArrayBase(rhs.mTotalBits){
    auto rhsBlock=rhs.mRootBlock.get();

    if(rhsBlock){
      L0Block *newBlock=new L0Block(*rhsBlock);
      mRootBlock.reset(newBlock,&BlockDeleter);
      mCurrentBlock=newBlock;
      rhsBlock=rhsBlock->Next();
    }

    while(rhsBlock){
      L0Block *newBlock=new L0Block(*rhsBlock);
      mCurrentBlock->Append(newBlock);
      mCurrentBlock=newBlock;
      rhsBlock=rhsBlock->Next();
    }

    mCurrentBlock=mRootBlock.get();
  }

  uint64_t BitArray::PopCount()const{
    auto cb=mRootBlock.get();

    uint64_t rollCount=0;

    while(cb){
      rollCount+=cb->PopCount();
      cb=cb->Next();
    }

    return rollCount;
  }

  BitArray& BitArray::operator=(const BitArray &rhs){
    mRootBlock=rhs.mRootBlock;
    mTotalBits=rhs.mTotalBits;

    Reset();

    return *this;
  }

  BitArrayCounter::BitArrayCounter(const BitArrayCounter &rhs):BitArrayBase(rhs.mTotalBits){
    auto rhsBlock=rhs.mRootBlock.get();

    if(rhsBlock){
      L0BlockCounter *newBlock=new L0BlockCounter(*rhsBlock);
      mRootBlock.reset(newBlock,BlockDeleter);
      mCurrentBlock=newBlock;
      rhsBlock=rhsBlock->Next();
    }

    while(rhsBlock){
      L0BlockCounter *newBlock=new L0BlockCounter(*rhsBlock);
      mCurrentBlock->Append(newBlock);
      mCurrentBlock=newBlock;
      rhsBlock=rhsBlock->Next();
    }

    mCurrentBlock=mRootBlock.get();
  }

  BitArrayCounter::BitArrayCounter(const BitArray &rhs):BitArrayBase(rhs.mTotalBits){
    auto rhsBlock=rhs.mRootBlock.get();
    uint64_t count=0;

    if(rhsBlock){
      L0BlockCounter *newBlock=new L0BlockCounter(*rhsBlock);
      mRootBlock.reset(newBlock,BlockDeleter);
      mCurrentBlock=newBlock;

      count=newBlock->PopCount();

      rhsBlock=rhsBlock->Next();
    }

    while(rhsBlock){
      L0BlockCounter *newBlock=new L0BlockCounter(*rhsBlock);
      mCurrentBlock->Append(newBlock);
      mCurrentBlock=newBlock;

      newBlock->BaseCounter(count);
      count=newBlock->PopCount();

      rhsBlock=rhsBlock->Next();
    }

    mCurrentBlock=mRootBlock.get();
  }

  uint64_t BitArrayCounter::PopCount()const{
    auto cb=mCurrentBlock;
    auto lastBlock=cb;

    while(cb){
      lastBlock=cb;
      cb=cb->Next();
    }

    return lastBlock->PopCount();
  }

  uint64_t BitArrayCounter::Rank(uint64_t pos)const{
    Seek(pos);

    return mCurrentBlock->Rank(pos);
  }

  uint64_t BitArrayCounter::Select(uint64_t count)const{
    if(count<mCurrentBlock->BaseCounter()){
      while(count<mCurrentBlock->BaseCounter()&&mCurrentBlock->Prev()){
        mCurrentBlock=mCurrentBlock->Prev();
        mCurrentOffset-=mCurrentBlock->Size();
      }
    } else{
      while(count>mCurrentBlock->BaseCounter()&&mCurrentBlock->Next()){
        mCurrentOffset+=mCurrentBlock->Size();
        mCurrentBlock=mCurrentBlock->Next();
      }
    }

    return mCurrentOffset+mCurrentBlock->Select(count);
  }

  BitArrayCounter& BitArrayCounter::operator=(const BitArrayCounter &rhs){
    mRootBlock=rhs.mRootBlock;
    mTotalBits=rhs.mTotalBits;

    Reset();

    return *this;
  }

/*  BitArrayCounter Compact(BitArray &val,BitArray &mask,size_t groupSize){

  }
*/
}