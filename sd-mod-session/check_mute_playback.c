/* Execute pinned native event dispatch with and without the shipped gate.
 * Only the final event-output receiver is replaced, not dispatch/filtering. */
#define main original_steps_main
#include "check_steps.c"
#undef main
static unsigned outputs;static struct step_event received;
static void output(void*p,uintptr_t track,unsigned input,void*event,unsigned modifying,void*a,void*b,void*c,unsigned d,unsigned e){
 (void)p;(void)track;(void)input;(void)modifying;(void)a;(void)b;(void)c;(void)d;(void)e;
 memcpy(&received,event,sizeof(received));++outputs;
}
typedef void(*Dispatch)(void*,uintptr_t,unsigned,struct step_event*,unsigned,void*,void*,void*,unsigned,unsigned,float);
int main(int argc,char**argv){
 assert(argc==2);load_elf(argv[1],0x1800000000ULL);stub(0x213a940,output);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 struct step_event on=make_note(4,240,.8f),muted=on;muted.type|=0x200;
 Dispatch dispatch=FN(0x213ba00,Dispatch);
 dispatch(NULL,1,0,&on,0,NULL,NULL,NULL,0,0,1.f);
 dispatch(NULL,1,0,&muted,0,NULL,NULL,NULL,0,0,1.f);
 assert(outputs==2&&event_muted(&received)); /* Native reproduces audible-mute bug. */
 FILE*f=fopen("build/phone_bridges.bin","rb");assert(f);
 size_t n=fread((void*)(base+0x6d3d000),1,PHONE_TABLE_VA-0x6d3d000,f);fclose(f);
 assert(n>4096&&n<PHONE_TABLE_VA-0x6d3d000);
 f=fopen("autorun/phone-muted-note-gate.bin","rb");assert(f);
 assert(fread((void*)(base+0x213ba04),1,4,f)==4);fclose(f);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 outputs=0;
 dispatch(NULL,1,0,&muted,0,NULL,NULL,NULL,0,0,1.f);assert(!outputs);
 dispatch(NULL,1,0,&on,0,NULL,NULL,NULL,0,0,1.f);assert(outputs==1&&!event_muted(&received));
 struct step_event off=muted;off.type|=0x100;
 dispatch(NULL,1,0,&off,0,NULL,NULL,NULL,0,0,1.f);assert(outputs==2&&(received.type&0x100));
 /* No data change to stored notes, including velocity and parameter masks. */
 assert(muted.type==(on.type|0x200)&&!memcmp(muted.data,on.data,40));
 puts("PASS actual native213ba00 reproduced muted note-on output; production ARM gate suppresses only muted note-ons, forwards unmuted notes and muted note-offs; native dispatcher retained, final audio receiver remains a fixture");
}
