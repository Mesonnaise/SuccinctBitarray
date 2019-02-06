#pragma once
#include<cstdlib>
#include<cinttypes>
#include<malloc.h>
#include<intrin.h>


template <typename T,std::size_t Align>
class AlignedAllocator{
public:
  using value_type=T;
  using propagate_on_container_move_assignment=std::true_type;
  using is_always_equal=std::true_type;

  template <class U>
  struct rebind { typedef AlignedAllocator<U,Align> other; };

  AlignedAllocator() noexcept {}

  template <class U>
  AlignedAllocator(const AlignedAllocator<U,Align>&) noexcept {}

  [[nodiscard]] T* allocate(const std::size_t n){
    void* ptr=_mm_malloc(n*sizeof(T),Align);

    if(ptr==nullptr)
      throw std::bad_alloc();

    return reinterpret_cast<T*>(ptr);
  }

  void deallocate(T* const p,const std::size_t) noexcept{
    return _mm_free(p);
  }
};

inline uint64_t le(uint64_t x,uint64_t y){
  uint64_t t=(y|0x8080808080808080ULL)-(x&~0x8080808080808080ULL);
  return (t^x^y)&0x8080808080808080ULL;
}

inline uint64_t lt(uint64_t x,uint64_t y){
  uint64_t t=(x|0x8080808080808080ULL)-(y&~0x8080808080808080ULL);
  return (t^x^~y)&0x8080808080808080ULL;
}

inline uint64_t BwordSelect(uint64_t x,uint64_t k){
  //Implementation is based off paper 
  //Broadword Implementation of Rank/Select Queries, Nov 19,2014
  //by Sebastiano Vigna

  uint64_t s,b,l;
  //Individual bit count per byte. 
  s=x-((x>>1)&0x5555555555555555ULL);
  s=(s&0x3333333333333333ULL)+((s>>2)&0x3333333333333333ULL);
  s=(s+(s>>4))&0x0F0F0F0F0F0F0F0FULL;
  s*=0x0101010101010101ULL;


  //Compaire one's count k to the bytes s and
  //use the results to find the byte where the k'th bit is. 
  //b=k*0x0101010101010101ULL;
  //b=__popcnt64(le(s,b))*8;

  //Fixed a flaw in the papers implementation, where leading zeros
  //in x would results in most significant byte being asigned to b
  //instead of the actual byte because sideways addition would copy
  //the same value for each zero filled byte. 
  b=k*0x0101010101010101ULL;
  b=(8-__popcnt64(le(b,s)))*8;

  //Old method for popcount
  //b=le(s,k*0x0101010101010101ULL)>>7;
  //b=((b*0x0101010101010101ULL)>>53)&~0x07;

  l=(s<<8)>>b;
  l=k-(l&0xFF);
  //Move the byte where k'th bit resides to the LSB
  s=(x>>b)&0xFF;


  //Spread the byte with target bit a cross a full 64bit word
  //Then a less than operation is used to move any set bits to the 
  //most significant bit for each 8 bit word.
  //The spread operation is fallowed by bitshift to LSB, so 
  //a sideways addition can be performed. 

  //BMI2's PDEP instruction can be used as an 
  //alterative for spread operation
  s=_pdep_u64(s,0x0101010101010101ULL)*0x0101010101010101ULL;
  //s=(s*0x0101010101010101ULL)&0x8040201008040201;
  //s=(lt(0,s)>>7)*0x0101010101010101ULL;

  s=__popcnt64(lt(s,l*0x0101010101010101ULL));

  return b+s;
}