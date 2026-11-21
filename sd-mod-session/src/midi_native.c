#define _GNU_SOURCE
#include "midi_native.h"
#include "midi_context.h"
#include "midi_wire.h"
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
static uintptr_t image;
static void (*original)(void*,uint64_t,const struct step_event*,void*);
static void *emu_engine;
static void emu_release(void *engine,void *track,unsigned midi);
#include "midi_live.inc"
extern void poly_record_release_event(void*,void*,unsigned,unsigned,int) __attribute__((weak));
/* Tuned MIDI notes go through the same native engine entry as physical
 * 16 Levels keys, so the recorder stores the tune/slice choice. */
static struct {void *track;unsigned midi,source,choice;_Atomic unsigned used;} emu[16];
static int emu_play(void *track,const struct step_event *incoming,int chop,int tune){
 void *engine=emu_engine;if(!engine||!track||!poly_record_release_event)return 0;
 void *things,*program,*pr;memcpy(&things,(char*)engine+0x3c0,8);if(!things)return 0;
 void *t2;memcpy(&t2,(char*)things+0x140,8);memcpy(&program,(char*)things+0xb8,8);memcpy(&pr,(char*)track+0xbb8,8);
 if(t2!=track||!program||program!=pr)return 0;
 unsigned midi=incoming->data[32];int source;
 memcpy(&source,(char*)program+0x14a8+midi*4,4);
 int back;if(source<0||source>=128)return 0;
 memcpy(&back,(char*)program+0x12a8+source*4,4);if(back!=(int)midi)return 0;
 struct step_event raw=*incoming;raw.time=0;raw.type&=~0x300u;raw.data[32]=(unsigned char)source;
 ((void(*)(void*,int,int,int))(image+0x27e4400))((char*)&raw+0x14,chop?15:0,0,tune);
 uint16_t mask;memcpy(&mask,(char*)&raw+0x14,2);unsigned kind=mask&0x8000u?15u:0u;
 unsigned choice=raw.data[6+kind]|(kind<<8)|(mask?0x1000u:0);
 for(unsigned i=0;i<16;++i){unsigned e=0;
  if(__atomic_compare_exchange_n(&emu[i].used,&e,2,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE)){
   emu[i].track=track;emu[i].midi=midi;emu[i].source=(unsigned)source;emu[i].choice=choice;
   ((void(*)(void*,int64_t,const void*,unsigned))(image+0x211b090))(engine,0,&raw,0x7ffffffa);
   __atomic_store_n(&emu[i].used,1,__ATOMIC_RELEASE);
   return 1;}}
 return 0;
}
static void emu_release(void *engine,void *track,unsigned midi){
 if(!poly_record_release_event)return;
 for(unsigned i=0;i<16;++i){
  if(__atomic_load_n(&emu[i].used,__ATOMIC_ACQUIRE)!=1||emu[i].track!=track||emu[i].midi!=midi)continue;
  unsigned e=1;if(!__atomic_compare_exchange_n(&emu[i].used,&e,2,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))continue;
  unsigned source=emu[i].source,choice=emu[i].choice;
  __atomic_store_n(&emu[i].used,0,__ATOMIC_RELEASE);
  poly_record_release_event(engine,track,source,choice,1);
  {static unsigned n;if(n<100){++n;fprintf(stderr,"AM_MIDI EMU_OFF midi=%u source=%u choice=%x\n",midi,source,choice);}}
 }
}
static void event(void *track,uint64_t offset,const struct step_event *incoming,void *output){
 /* Incoming-note modes: the stream thread queued a semitone for this pad just
  * before the stock note reached the music thread. */
 const struct step_event *source=incoming;int emulated=0;
 if(incoming&&(incoming->type&255)==3){
  uint16_t mask;memcpy(&mask,incoming->data+4,sizeof mask);int tune;
  int chop=0;
  {static unsigned seen;if(seen<300&&am_midi_mode_active()){++seen;fprintf(stderr,"AM_MIDI EVENT_SEEN pad=%u mask=%04x\n",(unsigned)incoming->data[32],(unsigned)mask);}}
  if(!mask&&incoming->data[32]<128&&am_midi_tune_take(incoming->data[32],&tune,&chop)&&tune){
   emulated=emu_play(track,incoming,chop,tune);
   if(!emulated){
    struct step_event *live=(struct step_event*)incoming;
    ((void(*)(void*,int,int,int))(image+0x27e4400))((char*)live+0x14,chop?15:0,0,tune);
   }
   fprintf(stderr,"AM_MIDI MODE_TUNE pad=%u %s=%d emu=%d\n",(unsigned)incoming->data[32],chop?"slice":"tune",tune,emulated);
  }
 }
 struct am_midi_context_frame frame;struct am_midi_context value;
 struct live_note *previous=live_event;unsigned blocked=live_blocked;
 am_midi_context_begin(track,source,(am_midi_choice_get)(image+0x27e4230),&frame);
 live_event=am_midi_context_snapshot(&value)?live_start(&value,source):NULL;
 live_blocked=!live_event&&am_midi_context_snapshot(&value);
 if(!emulated)original(track,offset,source,output);
 if(live_event&&!live_event->route_count)live_forget(live_event);
 live_event=previous;live_blocked=blocked;am_midi_context_end(&frame);
}
struct hook {uintptr_t offset,callback;uint32_t expected[4];void **saved;};
/* Constructor only. Relocate ADRP literals explicitly; no relative native
 * branch is permitted in the pinned displaced prologues. */
static int install_hook(uintptr_t base,const struct hook *h){
 unsigned char *code=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
 if(code==MAP_FAILED)return 0;
 uint32_t words[8]={0xd503245f,0,0,0,0,0x58000070,0xd61f0200,0xd503201f};
 uintptr_t continuation=base+h->offset+16;memcpy(code+32,&continuation,8);unsigned literals=40;
 for(unsigned i=0;i<4;++i){uint32_t w=h->expected[i];
  if((w&0x9f000000u)==0x90000000u){
   int64_t imm=(int64_t)(((w>>5)&0x7ffffu)<<2|((w>>29)&3));if(imm&0x100000)imm-=0x200000;
   uintptr_t page=((base+h->offset+i*4)&~(uintptr_t)4095)+imm*4096;
   words[i+1]=0x58000000u|((literals-(i+1)*4)/4<<5)|(w&31);
   memcpy(code+literals,&page,8);literals+=8;
  }else words[i+1]=w;
 }
 memcpy(code,words,32);__builtin___clear_cache((char*)code,(char*)code+literals);
 if(mprotect(code,4096,PROT_READ|PROT_EXEC)){munmap(code,4096);return 0;}
 uintptr_t page=(base+h->offset)&~(uintptr_t)4095;
 if(mprotect((void*)page,4096,PROT_READ|PROT_WRITE)){munmap(code,4096);return 0;}
 *h->saved=code;
 const uint32_t jump[2]={0x58000050,0xd61f0200};
 memcpy((void*)(base+h->offset),jump,8);memcpy((void*)(base+h->offset+8),&h->callback,8);
 __builtin___clear_cache((char*)base+h->offset,(char*)base+h->offset+16);
 if(mprotect((void*)page,4096,PROT_READ|PROT_EXEC))_exit(121);
 return 1;
}
static int install_scheduled_call(uintptr_t base){
 uintptr_t site=base+0x2118654;unsigned char *code=MAP_FAILED;
 for(uintptr_t off=0x7800000;off<0x9600000;off+=0x10000){
  uintptr_t address=base+off;if(address-site>=0x8000000)break;
  code=mmap((void*)address,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
  if(code!=MAP_FAILED)break;
 }
 if(code==MAP_FAILED)return 0;
 const uint32_t words[]={0xd503245f,0xd10083ff,0xa9017bfd,0xf90003e8,0x580000d0,0xd63f0200,0xa9417bfd,0x910083ff,0xd65f03c0,0xd503201f};
 memcpy(code,words,sizeof words);uintptr_t fn=(uintptr_t)live_scheduled_release;memcpy(code+40,&fn,8);
 __builtin___clear_cache((char*)code,(char*)code+48);
 if(mprotect(code,4096,PROT_READ|PROT_EXEC)){munmap(code,4096);return 0;}
 uintptr_t page=site&~(uintptr_t)4095;
 if(mprotect((void*)page,4096,PROT_READ|PROT_WRITE))_exit(121);
 uint32_t branch=0x94000000u|((uint32_t)(((uintptr_t)code-site)/4)&0x3ffffffu);memcpy((void*)site,&branch,4);
 __builtin___clear_cache((char*)site,(char*)site+4);
 if(mprotect((void*)page,4096,PROT_READ|PROT_EXEC))_exit(121);
 return 1;
}
int am_midi_native_install(uintptr_t base){
 if(image)return image==base;
 if(!base||sysconf(_SC_PAGESIZE)!=4096)return 0;
 const struct hook hooks[]={
  {0x29caa30,(uintptr_t)event,{0xd503233f,0xd10783ff,0xd0023cc4,0xf941cc84},(void**)&original},
  {0x28a8d00,(uintptr_t)live_on,{0xd503233f,0xd10243ff,0xa9057bfd,0x910143fd},(void**)&native_on},
  {0x28a8eb0,(uintptr_t)live_off,{0xd503233f,0xd10243ff,0x900245e5,0xf941cca5},(void**)&native_off},
  {0x28a8410,(uintptr_t)live_pressure,{0xd503245f,0x1e204001,0xd503233f,0xd10203ff},(void**)&native_pressure},
  {0x1e2ac50,(uintptr_t)live_queue,{0xd503233f,0xa9ba7bfd,0x910003fd,0xa90153f3},(void**)&native_queue},
  {0x2118460,(uintptr_t)live_release_note,{0xd503233f,0xd10183ff,0x90028267,0xf941cce7},(void**)&native_release},
  {0xe05620,(uintptr_t)live_send,{0xd503233f,0xd10203ff,0xf0031ae2,0xf941cc42},(void**)&native_send},
  {0x1e620b0,(uintptr_t)live_filter,{0xd503233f,0xd10203ff,0xd0029803,0xf941cc63},(void**)&native_filter},
  {0x1e39910,(uintptr_t)live_port_enqueue,{0xd503233f,0xa9ba7bfd,0x910003fd,0xa9025bf5},(void**)&native_port_queue}
 };
 for(unsigned i=0;i<sizeof hooks/sizeof *hooks;++i)if(memcmp((void*)(base+hooks[i].offset),hooks[i].expected,16))return 0;
 const uint32_t scheduled_bl=0x97ffff83; if(memcmp((void*)(base+0x2118654),&scheduled_bl,4))return 0;
 image=base;
 for(unsigned i=0;i<sizeof hooks/sizeof *hooks;++i)if(!install_hook(base,hooks+i))_exit(121);
 if(!install_scheduled_call(base))_exit(121);
 fprintf(stderr,"AM_MIDI_NATIVE SOURCE_CONTEXT_INSTALLED LIVE_LAYERS_INSTALLED\n");return 1;
}
