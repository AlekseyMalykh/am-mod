#define main legacy_scaled_main
#include "check_scaled_native.c"
#undef main
#include "src/takeover_native.h"
int main(int argc,char**argv){
 assert(!legacy_scaled_main(argc,argv));
 const uintptr_t entries[]={0x15f5690,0x15e70c0};uint32_t before[2][3];
 for(unsigned i=0;i<2;++i)memcpy(before[i],(void*)(base+entries[i]),12);
 assert(takeover_native_check(base)&&takeover_native_check(base));
 for(unsigned i=0;i<2;++i){
  assert(!memcmp(before[i],(void*)(base+entries[i]),12));
  FIELD((void*)base,entries[i]+4,uint32_t)=0xd503201f;
  assert(!takeover_native_check(base));
  assert(FIELD((void*)base,entries[i]+4,uint32_t)==0xd503201f);
  FIELD((void*)base,entries[i]+4,uint32_t)=before[i][1];
 }
 assert(!takeover_native_check(0));
 puts("PASS production native takeover audit: RotaryFixedConnection and TrimParameter code remains unchanged; mismatched prologues rejected without writes");return 0;
}
