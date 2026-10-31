#define PAD_EQ_PROOF_MAIN audio_proof_main
#include "check_pad_eq_native.c"
#include "src/pad_eq_history.h"
static unsigned performed,undone;static int fail;
static int native_perform(void*p){assert(F(p,0x60,void*)==instrument);++performed;return fail;}
static int native_undo(void*p){assert(F(p,0x60,void*)==instrument);++undone;return fail;}
int main(int argc,char**argv){
 assert(!audio_proof_main(argc,argv));phase="EQ native reset history";
 patch(0x14913b0,native_perform);patch(0x1491040,native_undo);assert(pad_eq_history_install(base)&&pad_eq_history_install(base));
 unsigned char command[0x68]={0};FN(0x14912b0,void(*)(void*,void*,void*))(command,renderer,instrument);
 uintptr_t*v=F(command,0,uintptr_t*);uint64_t original_eq=pad_eq_edit(pad_eq_default(),PAD_EQ_GAIN,22,0);assert(music_set_pad_eq(18,original_eq));
 fail=1;assert(((int(*)(void*))v[8])(command)==1&&music_get_pad_eq(18)==original_eq);fail=0;
 assert(!((int(*)(void*))v[8])(command)&&!music_get_pad_eq(18)&&!pad_eq_published(18));
 assert(!((int(*)(void*))v[9])(command)&&music_get_pad_eq(18)==original_eq);
 assert(!((int(*)(void*))v[8])(command)&&!music_get_pad_eq(18));
 assert(!((int(*)(void*))v[9])(command)&&music_get_pad_eq(18)==original_eq);
 ((void(*)(void*))v[0])(command);
 /* Reusing the same address is a new command with a fresh old value. */
 FN(0x14912b0,void(*)(void*,void*,void*))(command,renderer,instrument);v=F(command,0,uintptr_t*);assert(music_set_pad_eq(18,0));assert(!((int(*)(void*))v[8])(command));assert(!((int(*)(void*))v[9])(command)&&!music_get_pad_eq(18));((void(*)(void*))v[0])(command);
 assert(performed==4&&undone==3);puts("PASS actual ResetInstrumentCommand ctor/dtor and patched per-class native history slots: clear baked EQ, Undo/Redo restores exact settings, failed native reset unchanged, reused command storage fresh; native perform/undo bodies are fixtures");return 0;
}
