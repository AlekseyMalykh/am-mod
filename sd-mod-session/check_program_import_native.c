/* Execute actual firmware CopyPadCommand, including one group Undo/Redo,
 * with an external program as source and a separate destination program. */
#define BATCH_PROOF_MAIN batch_regression_main
#include "check_batch_native.c"
#ifdef MPC_PAD_EQ
#include "src/pad_eq.h"
#endif
extern int batch_copy_import_program_submit(uintptr_t,void*,void*,const unsigned*,const unsigned*,unsigned);
static unsigned char imported_program[0x2900],imported_pads[128][0x2468];
static void*imported_slots[128];
static int occupied(void*p){return FIELD(p,0x104,int);}
int main(int argc,char**argv){
 assert(batch_regression_main(argc,argv)==0);
 stub(0x2804760,occupied);
 FIELD(imported_program,0,void*)=FIELD(copy_program,0,void*);
 FIELD(imported_program,0x2808,void*)=imported_slots;
 FIELD(imported_program,0x2810,void*)=imported_slots+128;
 unsigned src[64],dst[64];
#ifdef MPC_PAD_EQ
 uint64_t destination_eq[64];
#endif
 for(unsigned i=0;i<128;++i){
  imported_slots[i]=imported_pads[i];FIELD(imported_pads[i],0,void*)=source_vptr;
  FIELD(imported_pads[i],0x100,int)=5000+(int)i;
  FIELD(imported_pads[i],0x104,int)=1;
  FIELD(imported_program,0x16a8+(i+0x84)*4,int)=127-(int)i;
 }
 for(unsigned i=0;i<64;++i){src[i]=i;dst[i]=64+i;assert(!occupied(pads[dst[i]]));
#ifdef MPC_PAD_EQ
  destination_eq[i]=pad_eq_edit(pad_eq_default(),PAD_EQ_GAIN,30+i,0);
  assert(music_set_pad_eq(src[i],pad_eq_edit(pad_eq_default(),PAD_EQ_GAIN,-80,0)));
  assert(music_set_pad_eq(dst[i],destination_eq[i]));
  assert(music_set_pad_colour(src[i],4));assert(music_set_pad_colour(dst[i],9));
#endif
 }
 phase="external PGM program copy: 64 pads and preserved destination MIDI map";
 assert(batch_copy_import_program_submit(base,context,imported_program,src,dst,64));
 void*cmd=(void*)kept[0];
 for(unsigned i=0;i<64;++i){assert(FIELD(pads[dst[i]],0x100,int)==5000+(int)i);
  assert(FIELD(copy_program,0x16a8+(dst[i]+0x84)*4,int)==(int)dst[i]);
#ifdef MPC_PAD_EQ
  assert(music_get_pad_eq(dst[i])==destination_eq[i]);assert(music_pad_colour(dst[i])==9);
#endif
 }
 assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x48,uintptr_t))(cmd)==0);
 for(unsigned i=0;i<64;++i){assert(FIELD(pads[dst[i]],0x100,int)==1000+(int)dst[i]);assert(!occupied(pads[dst[i]]));}
 FIELD(imported_pads[0],0x100,int)=9999;
 assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x40,uintptr_t))(cmd)==0);
 assert(FIELD(pads[64],0x100,int)==5000); /* Redo uses the owned snapshot. */
 assert(!batch_copy_import_program_submit(base,context,imported_program,src,dst,64));
 assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x48,uintptr_t))(cmd)==0);
 phase="external import failed snapshot is transactional";
 fail_clone=2;clones=0;
 assert(!batch_copy_import_program_submit(base,context,imported_program,src,dst,64));
 for(unsigned i=0;i<64;++i)assert(FIELD(pads[dst[i]],0x100,int)==1000+(int)dst[i]);
 fail_clone=0;
 assert(!batch_copy_import_program_submit(base,context,copy_program,src,dst,1));
 assert(!batch_copy_import_program_submit(base,context,imported_program,src,dst,129));
 phase="128-pad XPM import with equal source/destination ordinals";
 unsigned allsrc[128],alldst[128];int original_values[128];
 for(unsigned i=0;i<128;++i){allsrc[i]=alldst[i]=i;FIELD(pads[i],0x104,int)=0;original_values[i]=FIELD(pads[i],0x100,int);}
 assert(batch_copy_import_program_submit(base,context,imported_program,allsrc,alldst,128));
 void*allcmd=(void*)kept[0];
 for(unsigned i=0;i<128;++i)assert(FIELD(pads[i],0x100,int)==(i?5000+(int)i:9999));
 assert(((int(*)(void*))FIELD((void*)FIELD(allcmd,0,uintptr_t),0x48,uintptr_t))(allcmd)==0);
 for(unsigned i=0;i<128;++i)assert(FIELD(pads[i],0x100,int)==original_values[i]);
 assert(((int(*)(void*))FIELD((void*)FIELD(allcmd,0,uintptr_t),0x40,uintptr_t))(allcmd)==0);
 for(unsigned i=0;i<128;++i)assert(FIELD(pads[i],0x100,int)==(i?5000+(int)i:9999));
 puts("PASS 128-pad external XPM command with one Undo/Redo and equal ordinals");
 puts("PASS external program import: 64 native pad copies; one Undo/Redo; destination MIDI preserved; occupied target/invalid source rejected; failed preparation leaves destinations unchanged");
 return 0;
}
