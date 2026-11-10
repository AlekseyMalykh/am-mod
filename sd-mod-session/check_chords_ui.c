#define main old_fx_main
#include "check_fx_native.c"
#undef main
#include "src/chords.h"
#include "src/chords_ui.h"
#include "src/music_settings.h"
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
static unsigned char things[0x700],instrument[0x1700];
static FILE*svg;
static unsigned ink,texts,rects,repaints;
static unsigned poly_requests;
static float font_size;
void *note_native_context(void){return things;}
int note_native_request_poly(void){++poly_requests;return F(instrument,0x1690,int)==0?1:2;}
int note_native_pad_index(unsigned*p){*p=12;return 1;}
int note_native_current(char*out,size_t size){snprintf(out,size,"Chord test A13");return 1;}
void note_native_repaint_sample(void){++repaints;}
int music_lighting_levels_state(unsigned*enabled,unsigned*mode,unsigned*origin){*enabled=1;*mode=1;*origin=3;return 1;}
int music_ui_knob_fx_active(void){return 0;}
int music_ui_shift_active(void){return 0;}
/* Arpeggiator/playback seams: no sequence playback or held arp notes here. */
int step_native_playback_pitches(int*o,int m){(void)o;(void)m;return 0;}
int64_t step_native_clock_ticks(uint64_t*s,unsigned*r){if(s)*s=0;if(r)*r=0;return 0;}
int poly_record_arp_notes(int*n,int m,float*v){(void)n;(void)m;(void)v;return 0;}
int poly_record_arp_emit(int p,int on,float v){(void)p;(void)on;(void)v;return 0;}
void voice_guard_lock(void){}
int music_ui_repeat_rate(void){return -1;}
unsigned phone_native_button_light(unsigned c){(void)c;return 1;}
void voice_guard_unlock(void){}
int music_ui_chords_available(void){return 1;}
int workflow_blocks_sample_controls(void){return 0;}
int resample_ui_active(void){return 0;}
int resample_ui_shutdown_busy(void){return 0;}
static void redirect(uintptr_t offset,void*fn){
 uint32_t code[]={0x58000050,0xd61f0200};memcpy((void*)(base+offset),code,8);F((void*)base,offset+8,uintptr_t)=(uintptr_t)fn;
 __builtin___clear_cache((char*)base+offset,(char*)base+offset+16);
}
static void set_colour(void*g,uint32_t c){assert(g==(void*)0x1234);ink=c&0xffffff;}
static void rectangle(void*g,int x,int y,int w,int h){assert(g==(void*)0x1234&&x>=0&&y>=0&&w>=0&&h>=0&&x+w<=320&&y+h<=240);++rects;
 fprintf(svg,"<rect x='%d' y='%d' width='%d' height='%d' fill='#%06x'/>\n",x,y,w,h,ink);}
static void font_create(uint64_t*f,int family,float height){(void)family;*f=0;font_size=height;}
static void font_destroy(uint64_t*f){(void)f;}
static void font_set(void*g,const void*f){(void)g;(void)f;}
static void draw_text(void*g,const uint64_t*s,int x,int y,int w,int h,int align,int lines,float scale){
 (void)align;(void)lines;(void)scale;assert(g==(void*)0x1234&&x>=0&&y>=0&&x+w<=320&&y+h<=240);++texts;
 fprintf(svg,"<text x='%g' y='%g' font-family='monospace' font-size='%g' text-anchor='middle' fill='#%06x'>%s</text>\n",x+w/2.,y+h*.78,font_size,ink,(const char*)(uintptr_t)*s);
}
static int button(unsigned code,unsigned down){unsigned char e[16]={0};e[8]=(unsigned char)code;e[9]=(unsigned char)down;return chords_ui_button(e);}
int main(int argc,char**argv){
 assert(argc==3);load(argv[1]);F(things,0x668,void*)=instrument;
 redirect(0x102d5e0,set_colour);redirect(0x102e000,rectangle);redirect(0x1082810,font_create);
 redirect(0x1070d60,font_destroy);redirect(0x102dca0,font_set);redirect(0x10817b0,draw_text);
 struct music_sample settings={.root=0,.tonic=-1,.scale=-1};music_publish(&settings,0);
 chords_ui_init(base);chords_ui_tick(1);assert(chords_ui_button_owns(0x4b));
 assert(button(0x4b,1)&&button(0x4b,0)&&chords_active());
 assert(!button(0x52,1)&&chords_active()); /* Play remains native. */
 svg=fopen(argv[2],"w");assert(svg);fputs("<svg xmlns='http://www.w3.org/2000/svg' width='960' height='720' viewBox='0 0 320 240'>\n",svg);
 assert(chords_ui_paint(NULL,(void*)0x1234));fputs("</svg>\n",svg);fclose(svg);
 assert(texts>=12&&rects>=10&&repaints);
 assert(button(0x6f,1)&&button(0x6f,0));unsigned char e[16]={0};e[8]=0x64;e[9]=1;assert(chords_ui_control(e));
 struct chord_config c;chords_get(&c);assert(c.inversion==1);
 chords_roots(1,0);
 assert(chords_ui_button_owns(0x0b)&&button(0x0b,1)&&button(0x0b,0)&&chords_active());
 assert(chords_ui_button_owns(0x09)&&button(0x09,1)&&button(0x09,0)&&chords_active());
 assert(button(0x0c,1)&&!chords_active()&&button(0x0c,0)); /* Back never traps a held key. */
 chords_roots(0,-1);chords_ui_tick(1);
 assert(button(0x4b,1)&&button(0x4b,0));chords_ui_tick(0);assert(!chords_active());
 chords_ui_tick(1);assert(button(0x4b,1)&&button(0x4b,0));
 /* Note Repeat keeps Chords: held, the chord repeats whole at the native
  * repeat rate (1/16 until the native overlay was seen). */
 assert(chords_ui_button_owns(0x0b)&&button(0x0b,1)&&chords_active());
 chords_get(&c);assert(c.repeat&&c.repeat_rate==3&&chords_arp_pattern(&c)==ARP_CHORD&&chords_arp_rate(&c)==3);
 assert(button(0x0b,0));chords_get(&c);assert(!c.repeat&&chords_arp_pattern(&c)==ARP_OFF);
 assert(button(0x0c,1)&&button(0x0c,0)&&!chords_active());
 F(instrument,0x1690,int)=1;chords_ui_tick(1);unsigned requests=poly_requests;
 assert(button(0x4b,1)&&button(0x4b,0)&&chords_active());
 chords_ui_tick(1);chords_get(&c);assert(!c.valid&&poly_requests==requests+1);
 F(instrument,0x1690,int)=0;chords_ui_tick(1);chords_get(&c);assert(c.valid&&poly_requests==requests+1);
 assert(button(0x0c,1)&&button(0x0c,0)&&!chords_active());
 puts("PASS production Chords UI paint bounds, controls, Back while held and asynchronous Auto Poly entry");return 0;
}
