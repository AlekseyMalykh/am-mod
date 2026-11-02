#define BATCH_PROOF_MAIN batch_proof_main
#include "check_batch_native.c"
#include "src/pad_eq.h"
int main(int argc,char**argv){
 assert(!batch_proof_main(argc,argv));phase="batch EQ snapshots";music_reset();
 uint64_t before[3];unsigned colours[3]={5,8,12};for(unsigned i=0;i<3;++i){before[i]=pad_eq_edit(pad_eq_default(),PAD_EQ_GAIN,20+i*30,0);assert(music_set_pad_eq(i,before[i]));assert(music_set_pad_colour(i,colours[i]));}
 unsigned src[3]={0,1,2},dst[3]={1,2,0};assert(batch_copy_submit(base,context,src,dst,3));
 for(unsigned i=0;i<3;++i)assert(music_get_pad_eq(dst[i])==before[i]&&music_pad_colour(dst[i])==(int)colours[i]);
 void*cmd=(void*)kept[0];assert(!((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x48,uintptr_t))(cmd));
 for(unsigned i=0;i<3;++i)assert(music_get_pad_eq(i)==before[i]&&pad_eq_published(i)==before[i]&&music_pad_colour(i)==(int)colours[i]);
 assert(!((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x40,uintptr_t))(cmd));for(unsigned i=0;i<3;++i)assert(music_get_pad_eq(dst[i])==before[i]&&music_pad_colour(dst[i])==(int)colours[i]);
 fail_clone=2;clones=0;assert(!batch_copy_submit(base,context,src,dst,3));for(unsigned i=0;i<3;++i)assert(music_get_pad_eq(dst[i])==before[i]&&music_pad_colour(dst[i])==(int)colours[i]);
 puts("PASS native Batch Copy with EQ: independent per-pad source/target snapshots, overlapping cycles, group Undo/Redo and failed preparation preserves published metadata");return 0;
}
