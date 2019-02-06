#include<memory>
#include<iterator>
#include<vector>
#include<atomic>
#include<iostream>
#include<cstdlib>
#include<malloc.h>


#include <intrin.h>
/*
class Test{



public:
  class Iterator{
  public:
    using iterator_category=std::random_access_iterator_tag;
    using value_type=bool;
    using difference_type=std::ptrdiff_t;
    using pointer=bool*;
    using reference=bool&;


  public:
    Test    &mContainer;
    uint64_t mPos;

    struct BoolProxy{
      friend class Iterator;
    private:
      const uint64_t mPos;
      Test          *mContainer;
    protected:
      BoolProxy(Test *cont,uint64_t pos):mContainer(cont),mPos(pos){}

    public:
      operator bool() const{
        return mContainer->Get(mPos);
      }

      BoolProxy& operator=(const bool& rhl){
        if(rhl)
          mContainer->Set(mPos);
        else
          mContainer->Clear(mPos);

        return *this;
      }
    };

    BoolProxy& operator*(){
      return BoolProxy(&mContainer,mPos);
    }

    bool operator==(const Iterator & rhs);
  };
public:
  bool Get(uint64_t pos)const;
  bool Set(uint64_t pos);
  bool Clear(uint64_t pos);

  void begin();
  void end();
};
*/

constexpr uint64_t bflyStep(uint64_t val,uint64_t mask,uint64_t shift){
    uint64_t t=0;
    t=((val>>shift)^val) & mask;
    val=(val ^ t)^(t<<shift);
    return val;
}

constexpr uint64_t LROTC(size_t width,uint64_t shift){
  uint64_t ctrl=(0xFFFFFFFFFFFFFFFULL<<(shift%width));
  ctrl=(shift/width)%2?ctrl:~ctrl;

  return ctrl&~(0xFFFFFFFFFFFFFFFULL<<width);
}
  
uint64_t Cmerge(std::vector<uint64_t> &c,uint64_t s,uint64_t e,uint64_t p){
  uint64_t r=0;
  for(int i=s;i>=e;i--)
    r=(r|c[i])<<p;

  return r;
}


int main(){
  uint64_t mask=0x0000000100010001ULL;
  uint64_t val= 0x0000000100010001ULL;

  std::vector<uint64_t> control(31);


  control[0]=LROTC(32,__popcnt64(mask&0x00000000FFFFFFFFULL));
  uint64_t c32=control[0];
  val=bflyStep(val,c32,32);

  control[1]=LROTC(16,__popcnt64(mask&0x000000000000FFFFULL));
  control[2]=LROTC(16,__popcnt64(mask&0x0000FFFFFFFFFFFFULL));
  uint64_t c16=control[1]|control[1]<<32;
  val=bflyStep(val,c16,16);


  control[3]=LROTC(8,__popcnt64(mask&0x00000000000000FFULL));
  control[4]=LROTC(8,__popcnt64(mask&0x0000000000FFFFFFULL));
  control[5]=LROTC(8,__popcnt64(mask&0x000000FFFFFFFFFFULL));
  control[6]=LROTC(8,__popcnt64(mask&0x00FFFFFFFFFFFFFFULL));
  uint64_t c8=Cmerge(control,6,3,16);
  val=bflyStep(val,c8,8);

  control[7]=LROTC(4,__popcnt64(mask&0x000000000000000FULL));
  control[8]=LROTC(4,__popcnt64(mask&0x0000000000000FFFULL));
  control[9]=LROTC(4,__popcnt64(mask&0x00000000000FFFFFULL));
  control[10]=LROTC(4,__popcnt64(mask&0x000000000FFFFFFFULL));
  control[11]=LROTC(4,__popcnt64(mask&0x0000000FFFFFFFFFULL));
  control[12]=LROTC(4,__popcnt64(mask&0x00000FFFFFFFFFFFULL));
  control[13]=LROTC(4,__popcnt64(mask&0x000FFFFFFFFFFFFFULL));
  control[14]=LROTC(4,__popcnt64(mask&0x0FFFFFFFFFFFFFFFULL));
  uint64_t c4=Cmerge(control,14,7,8);
  val=bflyStep(val,c4,4);

  control[15]=LROTC(2,__popcnt64(mask&0x0000000000000003ULL));
  control[16]=LROTC(2,__popcnt64(mask&0x000000000000003FULL));
  control[17]=LROTC(2,__popcnt64(mask&0x00000000000003FFULL));
  control[18]=LROTC(2,__popcnt64(mask&0x0000000000003FFFULL));
  control[19]=LROTC(2,__popcnt64(mask&0x000000000003FFFFULL));
  control[20]=LROTC(2,__popcnt64(mask&0x00000000003FFFFFULL));
  control[21]=LROTC(2,__popcnt64(mask&0x0000000003FFFFFFULL));
  control[22]=LROTC(2,__popcnt64(mask&0x000000003FFFFFFFULL));
  control[23]=LROTC(2,__popcnt64(mask&0x00000003FFFFFFFFULL));
  control[24]=LROTC(2,__popcnt64(mask&0x0000003FFFFFFFFFULL));
  control[25]=LROTC(2,__popcnt64(mask&0x000003FFFFFFFFFFULL));
  control[26]=LROTC(2,__popcnt64(mask&0x00003FFFFFFFFFFFULL));
  control[27]=LROTC(2,__popcnt64(mask&0x0003FFFFFFFFFFFFULL));
  control[28]=LROTC(2,__popcnt64(mask&0x003FFFFFFFFFFFFFULL));
  control[29]=LROTC(2,__popcnt64(mask&0x03FFFFFFFFFFFFFFULL));
  control[30]=LROTC(2,__popcnt64(mask&0x3FFFFFFFFFFFFFFFULL));
  uint64_t c2=Cmerge(control,30,15,4);
  val=bflyStep(val,c2,2);


  return 0;
}