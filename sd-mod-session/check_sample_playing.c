#define main unused_fx_main
#include "check_fx_native.c"
#undef main
#undef FN
#include "src/sample_playing.c"
static unsigned char program[0x2880],things[0x1200],layer[0x100],instruments[128][0x2500],voice[128][0x940];
static uintptr_t voices[128];static unsigned calls;static unsigned expect_pad;
static uintptr_t voice_layers[128],layer_vtable[2],voice_layer_objects[128][2];
static int rendering(void *p){return F(p,8,int);}
static void*cast(void*p,void*from,void*to,intptr_t offset){assert(p==program&&from==(void*)1&&to==(void*)2&&!offset);return p;}
static void*instrument(void*p,int pad){assert(p==program&&pad>=0&&pad<128);return instruments[pad];}
static int checked_refresh(void *p,unsigned pad,int(*method)(void*,unsigned)){
#ifdef MPC_MIXER_LIVE
 sample_playing_render(F(things,0xb8,void*));
#endif
 return method(p,pad);
}
static int original_refresh(void*p,unsigned pad){assert(p==layer&&pad==expect_pad);++calls;return 9;}
static void stub(uintptr_t offset,void*fn){uint32_t words[]={0x58000050,0xd61f0200};memcpy((void*)(base+offset),words,8);memcpy((void*)(base+offset+8),&fn,8);}
static void relation_checks(void){
 /* Execute the pinned native getter, not a stub: stored links are 1-based,
  * normalized by 128; Sample mute groups are normalized by 32. */
 const unsigned offsets[]={3752,3840,3928,4016};
 for(unsigned source=0;source<128;++source){
  memset(instruments,0,sizeof(instruments));
  F(instruments[source],0x234d,unsigned char)=1;
  for(unsigned k=0;k<4;++k)F(instruments[source],offsets[k],int)=(int)((source+1+k*16)%128)+1;
  for(unsigned bank=0;bank<8;++bank){
   unsigned expected=0;
   for(unsigned k=0;k<4;++k){unsigned target=(source+1+k*16)%128;if(target/16==bank)expected|=1u<<(target%16);}
   assert(sample_playing_links(program,source,bank)==expected);
  }
  F(instruments[source],0x234d,unsigned char)=0;
  for(unsigned bank=0;bank<8;++bank)assert(!sample_playing_links(program,source,bank));
 }
 memset(instruments,0,sizeof(instruments));
 F(instruments[0],0x234d,unsigned char)=1;
 F(instruments[0],3752,int)=1; /* self */
 F(instruments[0],3840,int)=129; /* out of range */
 F(instruments[0],3928,int)=-1;
 F(instruments[0],4016,int)=128; /* last bank boundary */
 assert(!sample_playing_links(program,0,0)&&sample_playing_links(program,0,7)==0x8000);
 /* Keep legacy groups deliberately different: these are not the Sample UI groups. */
 for(unsigned i=0;i<128;++i)F(instruments[i],8840,int)=i%2?7:0;
 for(unsigned group=0;group<=32;++group){
  for(unsigned i=0;i<128;++i)F(instruments[i],0x1588,int)=i%3?0:(int)group;
  for(unsigned source=0;source<128;++source)for(unsigned bank=0;bank<8;++bank){
   unsigned expected=0;
   if(group&&source%3==0)for(unsigned i=0;i<16;++i)if((bank*16+i)%3==0&&bank*16+i!=source)expected|=1u<<i;
   assert(sample_playing_group(program,source,bank)==expected);
  }
 }
 assert(!sample_playing_links(NULL,0,0)&&!sample_playing_links(program,128,0)&&!sample_playing_links(program,0,8));
 assert(!sample_playing_group(NULL,0,0)&&!sample_playing_group(program,128,0)&&!sample_playing_group(program,0,8));
 puts("PASS actual native Pad Link getter across128 sources/eight banks, disabled/self/invalid links; all32 Sample Mute Groups, self exclusion and bank boundaries");
}
int main(int argc,char**argv){
 assert(argc==2);load(argv[1]);phase="native playing lighting snapshot";
 stub(0xb51d78,cast);stub(0x286bbe0,instrument);stub(0x1539610,original_refresh);
 F((void*)base,0x71657d8,void*)=(void*)1;F((void*)base,0x71658a8,void*)=(void*)2;
 F(things,0xb8,void*)=program;F(layer,0xc8,void*)=things;
 layer_vtable[1]=(uintptr_t)rendering;
 for(unsigned i=0;i<128;++i){
  F(instruments[i],0x2310,void*)=program;F(instruments[i],0x2168,unsigned)=i;
  voices[i]=(uintptr_t)voice[i];F(voice[i],0x8f8,void*)=instruments[i];F(voice[i],0x924,unsigned)=200+i;F(voice[i],0x928,unsigned)=1;F(program,0x12a8+i*4,unsigned)=200+i;
  voice_layer_objects[i][0]=(uintptr_t)layer_vtable;voice_layer_objects[i][1]=1;
  voice_layers[i]=(uintptr_t)voice_layer_objects[i];F(voice[i],0x310,void*)=&voice_layers[i];F(voice[i],0x318,void*)=&voice_layers[i+1];
 }
 F(program,0x2838,void*)=voices;F(program,0x2840,void*)=(char*)voices+sizeof(voices);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 assert(sample_playing_install(base));int(*method)(void*,unsigned)=F((void*)base,0x6e10080,void*);
 for(unsigned i=0;i<128;++i){expect_pad=i;assert(checked_refresh(layer,i,method)==9&&sample_playing_active(program,i));
  F(voice[i],0x928,unsigned)=5;assert(checked_refresh(layer,i,method)==9&&sample_playing_active(program,i));
  voice_layer_objects[i][1]=0;assert(checked_refresh(layer,i,method)==9&&!sample_playing_active(program,i));voice_layer_objects[i][1]=1;
  F(voice[i],0x924,unsigned)=999;assert(checked_refresh(layer,i,method)==9&&sample_playing_active(program,i));F(voice[i],0x924,unsigned)=200+i;
 }
 /* A delayed target starts without ANY native lighting callback. */
#ifdef MPC_MIXER_LIVE
 unsigned before_callbacks=calls;
 voice_layer_objects[7][1]=0;sample_playing_render(program);assert(!sample_playing_active(program,7));
 voice_layer_objects[7][1]=1;sample_playing_render(program);assert(sample_playing_active(program,7));
 voice_layer_objects[7][1]=0;sample_playing_render(program);assert(!sample_playing_active(program,7)&&calls==before_callbacks);
 puts("PASS audio-owner activity starts and ends with zero native LED refreshes");
#endif
 assert(calls==512&&!sample_playing_active(NULL,0)&&!sample_playing_active(program,128));
 /* Exact user gesture: a different editor selection cannot clear A01,
  * even if its mapped note was changed by another pad/16 Levels action. */
 for(unsigned selected=1;selected<16;++selected){
  F(things,0x5f8,unsigned)=selected;F(program,0x12a8,unsigned)=999;
  expect_pad=0;assert(checked_refresh(layer,0,method)==9&&sample_playing_active(program,0));
 }
 voice_layer_objects[0][1]=0;expect_pad=0;assert(checked_refresh(layer,0,method)==9&&!sample_playing_active(program,0));
 F(things,0xb8,void*)=NULL;expect_pad=0;assert(checked_refresh(layer,0,method)==9&&!sample_playing_active(program,0));
 relation_checks();
 puts("PASS actual native any-note voice ownership and rendering predicate for128 pads; released but rendering loops stay active, stopped layers clear, selection changes preserve ownership");return 0;
}
