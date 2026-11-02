/* Actual native Copy ctor/perform/undo and Extract ctor/dtor. Extraction and
 * failure receivers below are fixtures, not an audio/file persistence test. */
#define BATCH_PROOF_MAIN batch_proof_main
#include "check_batch_native.c"
#include "src/pad_eq_history.h"
static int extract_status,copy_status;
static int extract_receiver(void*p){assert(FIELD(p,0x58,unsigned)==4&&FIELD(p,0x68,void*)==pads[0]);return extract_status;}
static int copy_failure(void*p){(void)p;return copy_status;}
static int call(void*p,unsigned slot){return ((int(*)(void*))FIELD(p,0,uintptr_t*)[slot])(p);}
static void destroy_command(void*p){((void(*)(void*))FIELD(p,0,uintptr_t*)[0])(p);}
int main(int argc,char**argv){
 assert(!batch_proof_main(argc,argv));phase="Pad Colour native Undo history";music_reset();
 stub(0x143f0b0,extract_receiver);stub(0x143ee90,extract_receiver);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 assert(pad_eq_history_install(base)&&pad_eq_history_install(base));
 unsigned char command[0x4b8]={0};
 assert(music_set_pad_colour(0,5)&&music_set_pad_colour(17,8)&&music_set_pad_colour(80,12));
 FN(0x1986f90,void(*)(void*,void*,void*,void*,int,uint64_t,uint64_t))
  (command,properties,registry,copy_program,0,UINT64_C(1)<<17,UINT64_C(1)<<16);
 assert(FIELD(command,0x68,unsigned)==0&&FIELD(command,0x70,uint64_t)==(UINT64_C(1)<<17)&&FIELD(command,0x78,uint64_t)==(UINT64_C(1)<<16));
 assert(!call(command,8)&&music_pad_colour(17)==5&&music_pad_colour(80)==5);
 assert(!call(command,9)&&music_pad_colour(17)==8&&music_pad_colour(80)==12);
 assert(music_set_pad_colour(0,7)); /* Redo uses the command's captured colour. */
 assert(!call(command,8)&&music_pad_colour(17)==5&&music_pad_colour(80)==5);
 assert(!call(command,9)&&music_pad_colour(17)==8&&music_pad_colour(80)==12);
 destroy_command(command);
 stub(0x1987530,copy_failure);__builtin___clear_cache((char*)(base+0x1987530),(char*)(base+0x1987540));
 FN(0x1986f90,void(*)(void*,void*,void*,void*,int,uint64_t,uint64_t))
  (command,properties,registry,copy_program,0,UINT64_C(1)<<17,0);
 copy_status=3;assert(call(command,8)==3&&music_pad_colour(17)==8);
 copy_status=0;assert(!call(command,8)&&music_pad_colour(17)==7);destroy_command(command);
 /* The real Extract constructor establishes source instrument and target ABI. */
 unsigned char extraction[0xb0]={0},slice[0x368]={0};uint64_t name=0;
 FIELD(properties,0x1278,void*)=(void*)0x1234;FIELD(pads[0],0x2168,unsigned)=0;
 FN(0xf535c0,void(*)(void*,const char*))(&name,"colour-extract");
 assert(music_set_pad_colour(4,8));
 FN(0x143ed40,void(*)(void*,const uint64_t*,int,void*,void*,void*,void*,void*,void*))
  (extraction,&name,4,slice,pads[0],properties,(void*)0x7777,(void*)0x55,(void*)0x66);
 extract_status=3;assert(call(extraction,8)==3&&music_pad_colour(4)==8);
 extract_status=0;assert(!call(extraction,8)&&music_pad_colour(4)==7);
 assert(!call(extraction,9)&&music_pad_colour(4)==8);
 assert(music_set_pad_colour(0,1));assert(!call(extraction,8)&&music_pad_colour(4)==7);
 extract_status=3;assert(call(extraction,9)==3&&music_pad_colour(4)==7);
 extract_status=0;assert(!call(extraction,9)&&music_pad_colour(4)==8);destroy_command(extraction);
 assert(music_set_pad_colour(4,12));
 FN(0x143ed40,void(*)(void*,const uint64_t*,int,void*,void*,void*,void*,void*,void*))
  (extraction,&name,4,slice,pads[0],properties,(void*)0x7777,(void*)0x55,(void*)0x66);
 assert(!call(extraction,8)&&music_pad_colour(4)==1);
 assert(!call(extraction,9)&&music_pad_colour(4)==12);destroy_command(extraction);
 FN(0xf0a290,void(*)(void*))(&name);
 puts("PASS native Copy multi-target/cross-bank Pad Colour, actual perform/undo, captured Redo; native Extract ctor/dtor and per-command colour history, failed perform/undo, reused storage; extraction and failure bodies are fixtures");
 return 0;
}
