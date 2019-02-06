#pragma once
#include<iterator>
#include<list>
#include<cassert>
#include<memory>
#include<intrin.h>

#include"L0Block.h"

namespace Succinct{
  template<class BlockBase,size_t MaxBlockSize=3145728>
  class BitArrayBase{
    friend class BitArray;
    friend class BitArrayCounter;

  protected:
    std::shared_ptr<BlockBase> mRootBlock;

            size_t             mTotalBits;

    mutable BlockBase         *mCurrentBlock;
    mutable uint64_t           mCurrentOffset=0;

  protected:
    void Reset(){
      mCurrentBlock=mRootBlock.get();
      mCurrentOffset=0;
    }

    void Seek(uint64_t &pos)const{
      if(pos>=mTotalBits)
        throw std::range_error("Exceeded bounds of array");


      while(mCurrentBlock->Next()&&
        (mCurrentOffset+mCurrentBlock->Size())<pos){

        mCurrentOffset+=mCurrentBlock->Size();
        mCurrentBlock=mCurrentBlock->Next();
      }

      while(mCurrentBlock->Prev()&&
            mCurrentOffset>pos){

        mCurrentBlock=mCurrentBlock->Prev();
        mCurrentOffset-=mCurrentBlock->Size();
      }

      pos-=mCurrentOffset;
    }

    static void BlockDeleter(BlockBase *block){
      auto cb=block;

      while(cb){
        auto t=cb;
        cb=cb->Next();
        delete t;
      }
    }

  public:
    BitArrayBase(size_t bitCount):mTotalBits(bitCount){
      uint32_t blockSize=static_cast<uint32_t>(std::min(bitCount,MaxBlockSize));
      mRootBlock.reset(
        new BlockBase(blockSize),
        BlockDeleter);

      bitCount-=blockSize;

      mCurrentBlock=mRootBlock.get();

      while(bitCount){
        blockSize=static_cast<uint32_t>(std::min(bitCount,MaxBlockSize));
        auto newBlock=new BlockBase(blockSize);
        bitCount-=blockSize;

        mCurrentBlock->Append(newBlock);

        mCurrentBlock=mCurrentBlock->Next();
      }

      mCurrentBlock=mRootBlock.get();
    }

    ~BitArrayBase(){}

    size_t AllocatedSize()const{
      auto cb=mRootBlock.get();
      size_t totalSize=0;

      while(cb){
        totalSize+=cb->AllocatedSize();
        cb=cb->Next();
      }

      return totalSize;
    }

    bool Get(uint64_t pos)const{
      Seek(pos);

      return mCurrentBlock->Get(pos);
    }

    void Set(uint64_t pos){
      Seek(pos);

      mCurrentBlock->Set(pos);
    }

    void Clear(uint64_t pos){
      Seek(pos);

      mCurrentBlock->Clear(pos);
    }

    uint64_t GetGroup(uint64_t pos,size_t size)const{
      assert(size<=64&&size>1&&_blsr_u64(size)==0);

      Seek(pos);

      return mCurrentBlock->GetGroup(pos,size);
    }

    void SetGroup(uint64_t pos,uint64_t val,size_t size){
      assert(size<=64&&size>1&&_blsr_u64(size)==0);

      Seek(pos);

      mCurrentBlock->SetGroup(pos,val,size);
    }

    void ClearGroup(uint64_t pos,uint64_t val,size_t size){
      assert(size<=64&&size>1&&_blsr_u64(size)==0);

      Seek(pos);

      mCurrentBlock->ClearGroup(pos,val,size);
    }
  };

  class BitArrayCounter;

  class BitArray:public BitArrayBase<L0Block>{
  public:
    BitArray(size_t size):BitArrayBase(size){}
    BitArray(const BitArray &rhs);
    BitArray(const BitArrayCounter &rhs);

    ~BitArray(){}

    uint64_t           PopCount()const;

    BitArray&          operator=(const BitArray &rhs);
  };

  class BitArrayCounter:public BitArrayBase<L0BlockCounter>{
  public:

    BitArrayCounter(size_t size):BitArrayBase(size){}

    BitArrayCounter(const BitArrayCounter &rhs);
    BitArrayCounter(const BitArray &rhs);
    ~BitArrayCounter(){}

    uint64_t           PopCount()const;

    uint64_t           Rank(uint64_t pos)const;
    uint64_t           Select(uint64_t count)const;

    BitArrayCounter&   operator=(const BitArrayCounter &rhs);
  };


  BitArrayCounter Compact(BitArray &val,BitArray &mask,size_t groupSize=2);

  BitArray Expand(const BitArrayCounter &val,const BitArrayCounter &mask,size_t groupSize=2);

}