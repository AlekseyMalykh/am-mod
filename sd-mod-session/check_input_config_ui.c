/* Execute the production row-insertion relay at the pinned native callsite.
 * Component/Graphics callbacks are fixtures; audio and device are not used. */
#define main unused_rear_ui_main
#include "check_rear_ui.c"
#undef main
int rear_native_monitor(void){return 0;}
int rear_native_mic_waiting(void){return 0;}
void rear_ui_wake(void){}
void rear_audio_meters(float p[2]){p[0]=p[1]=0;}
unsigned rear_record_state(void){return REAR_IDLE;}
uint64_t rear_record_frames(void){return 0;}
unsigned rear_native_status(void){return 0;}
const char*rear_native_message(void){return "";}
#include <assert.h>
#include <stdlib.h>
static unsigned fx_mode,rows;
static void *args[3];
static uint32_t ink,pixels[98*38];
static char caption[32];
static void*midi_host;static unsigned midi_opens;
void am_midi_ui_open_input(void*p){midi_host=p;++midi_opens;}
unsigned music_fx_mode_active(void){return fx_mode;}
static void add_row(void*a,void*b,void*c){assert(a==args[0]&&b==args[1]&&c==args[2]);++rows;}
static void set_ink(void*g,uint32_t c){(void)g;ink=c;}
static void draw_rect(void*g,int x,int y,int w,int h){(void)g;
 assert(x>=0&&y>=0&&x+w<=98&&y+h<=38);
 for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i)pixels[j*98+i]=ink;
}
static void string_make(uint64_t*s,const char*t){*s=(uintptr_t)t;}
static void font_make(uint64_t*f,int flags,float size){(void)flags;(void)size;*f=0;}
static void discard(void*p){(void)p;}
static void set_font(void*g,const void*f){(void)g;(void)f;}
static void draw_text(void*g,const uint64_t*s,int x,int y,int w,int h,int align,int max,float scale){
 (void)g;(void)x;(void)y;(void)w;(void)h;(void)align;(void)max;(void)scale;
 snprintf(caption,sizeof caption,"%s",(const char*)(uintptr_t)*s);
}
static void redirect(uintptr_t off,void*fn){
 uint32_t jump[]={0x58000050,0xd61f0200};memcpy((void*)(image+off),jump,sizeof jump);
 memcpy((void*)(image+off+8),&fn,8);__builtin___clear_cache((char*)image+off,(char*)image+off+16);
}
int main(int argc,char**argv){
 assert(argc==2);image=(uintptr_t)mmap((void*)UINT64_C(0x1800000000),0x7400000,7,
 MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);assert(image!=UINTPTR_MAX);
 const uintptr_t site=0x16237a4;FILE*f=fopen(argv[1],"rb");assert(f);
 assert(!fseek(f,(long)site,SEEK_SET)&&fread((void*)(image+site),4,1,f)==1);fclose(f);
 assert(FIELD((void*)image,site,uint32_t)==0x949531f3u);
 FIELD((void*)image,site,uint32_t)=0;assert(!install_input_effects_row());
 FIELD((void*)image,site,uint32_t)=0x949531f3u;assert(install_input_effects_row());
 /* Keep the real patched BL at its original PC, bracketed by a test frame. */
 void*page=(void*)((image+site)&~(uintptr_t)4095);assert(!mprotect(page,4096,7));
 FIELD((void*)image,site-4,uint32_t)=0xa9bf7bfdu;
 FIELD((void*)image,site+4,uint32_t)=0xa8c17bfdu;
 FIELD((void*)image,site+8,uint32_t)=0xd65f03c0u;
 __builtin___clear_cache((char*)image+site-4,(char*)image+site+12);
 redirect(0x3b6ff70,add_row);
 for(unsigned mode=0;mode<2;++mode){
  fx_mode=mode;rows=3;for(unsigned i=0;i<3;++i)args[i]=(void*)(uintptr_t)(123+i);
  ((void(*)(void*,void*,void*))(image+site-4))(args[0],args[1],args[2]);
  assert(rows==(mode?3u:4u));
 }
 redirect(0x102d5e0,set_ink);redirect(0x102e000,draw_rect);
 redirect(0xf535c0,string_make);redirect(0x1082810,font_make);
 redirect(0x102dca0,set_font);redirect(0x10817b0,draw_text);
 redirect(0x1070d60,discard);redirect(0xf0a290,discard);
 const char*labels[]={"Off","Record","On"};uint32_t hashes[3];
 for(unsigned mode=0;mode<3;++mode){
  monitor_header(NULL,mode);assert(!strcmp(caption,labels[mode]));hashes[mode]=0;
  for(unsigned i=0;i<98*38;++i)hashes[mode]=hashes[mode]*33u+pixels[i];
  assert(pixels[19*98+14]==(mode?0xff202020u:REAR_GREY));
  if(mode==1)assert(pixels[8*98+27]==REAR_RED);
 }
 assert(hashes[0]!=hashes[1]&&hashes[1]!=hashes[2]&&hashes[0]!=hashes[2]);
 unsigned char config[0x400]={0};roots[0]=config;atomic_store(&visible_roots,1);atomic_store(&open_panel,0);
 button(0x0e);assert(midi_opens==1&&midi_host==config);
 button(0x6d);assert(midi_opens==2); /* Native soft-button alias shares the action. */
 atomic_store(&visible_roots,0);assert(!rear_ui_owns(2,0x0e));
 puts("PASS pinned Input Config insertion and Monitor headers; B3 MIDI In on Input Config opens shared settings with paired release and hidden-screen ownership");
 return 0;
}
