/* Production controls and focus; native widgets/drawing are receivers. */
#define _GNU_SOURCE
#include "src/note_native.c"
#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
static unsigned char sample[0x8000],controller[0x2300];
static unsigned paints,texts;static uint32_t ink;static int hidden;
static int visible(void*p){return !hidden&&p==sample;}
static int blocked(void*p){(void)p;return 0;}
static double semi(void*p){(void)p;return 0;}
static void repaint_receiver(void*p,uint64_t a,uint64_t b){(void)p;(void)a;(void)b;++paints;}
static void string_make(uint64_t*p,const char*s){*p=(uintptr_t)strdup(s);}
static void string_end(uint64_t*p){free((void*)(uintptr_t)*p);*p=0;}
void note_label_string(void*p,uint64_t*out,uintptr_t fn){assert(fn==base+0x11bae00);string_make(out,p==sample+0x1290?"A01":"fixture.wav");}
static void colour_receiver(void*g,uint32_t c){(void)g;ink=c;}
static void rect_receiver(void*g,int x,int y,int w,int h){(void)g;assert(x>=106&&x+w<=212&&y>=198&&y+h<=236);}
static void font_make(uint64_t*p,int style,float size){(void)style;(void)size;*p=1;}
static void font_use(void*g,const uint64_t*p){(void)g;assert(*p==1);}
static void font_end(uint64_t*p){assert(*p==1);}
static void text_receiver(void*g,const uint64_t*p,int x,int y,int w,int h,int a,int b,float s){(void)g;(void)a;(void)b;(void)s;assert(x>=106&&x+w<=212&&y>=198&&y+h<=236);if(!strcmp((char*)(uintptr_t)*p,"Pad Colour"))++texts;if(!strcmp((char*)(uintptr_t)*p,"S"))assert(ink==0xffffffff);}
static void stub(uintptr_t off,void*fn){uint32_t w[]={0x58000050,0xd61f0200};memcpy((void*)(base+off),w,8);memcpy((void*)(base+off+8),&fn,8);}
static int cc(unsigned code,unsigned value){unsigned char e[16]={0};e[8]=(unsigned char)code;e[9]=(unsigned char)value;return note_native_control(e);}
static int button(unsigned code,unsigned down){unsigned char e[16]={0};e[8]=(unsigned char)code;e[9]=(unsigned char)down;return note_native_button(e);}
static void shift(int on){controller[0x22a0]=(unsigned char)on;note_native_button_requested(0x31,(unsigned)on);button(0x31,(unsigned)on);note_native_tick();}
int main(void){
 void*image=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(image!=MAP_FAILED);base=(uintptr_t)image;
 stub(0x112fd50,visible);stub(0x112f6b0,blocked);stub(0x11234b0,semi);stub(0x1149880,repaint_receiver);
 stub(0xf535c0,string_make);stub(0xf0a290,string_end);stub(0x102d5e0,colour_receiver);stub(0x102e000,rect_receiver);
 stub(0x1082810,font_make);stub(0x102dca0,font_use);stub(0x1070d60,font_end);stub(0x10817b0,text_receiver);
 __builtin___clear_cache(image,(char*)image+0x7500000);
 FIELD(sample,0,uintptr_t)=base+SAMPLE_VPTR;FIELD(sample,0x108,void*)=controller;
 FIELD(sample,SEMI_OFFSET,uintptr_t)=base+SLIDER_VPTR;FIELD(sample,MIX_VOLUME_OFFSET,uintptr_t)=base+SLIDER_VPTR;
 FIELD(sample,0x190+0x21c,int)=2;FIELD(sample,0x190+0x218,int)=1;
 uintptr_t*focus=(uintptr_t*)(base+0x723cde0);uintptr_t volume=(uintptr_t)sample+MIX_VOLUME_OFFSET+0x1d8,pan=volume+0x350;
 music_reset();note_native_observe_root(sample);note_native_tick();
 assert(note_native_owns(0x11)&&!note_native_owns(0x10));assert(cc(0x11,60)&&music_pad_colour(0)==3&&focused_trim&&!*focus);
 assert(cc(0x64,1)&&music_pad_colour(0)==4);assert(!cc(0x10,100)&&!focused_trim&&music_pad_colour(0)==4);
 assert(cc(0x11,20));shift(1);int before=music_pad_colour(0);
 assert(!focused_trim&&!note_native_owns(0x11));assert(!cc(0x11,100)&&!cc(0x10,100)&&music_pad_colour(0)==before);
 texts=0;note_native_sample_overlay(sample,(void*)1);assert(!texts); /* native Kit Volume draws itself */
 shift(0);texts=0;note_native_sample_overlay(sample,(void*)1);assert(texts==1);
 *focus=volume;assert(button(0x6f,1)&&focused_trim&&!*focus);assert(button(0x6f,0));
 assert(!button(0x6f,1)&&!focused_trim&&*focus==volume);assert(!button(0x6f,0));
 *focus=pan;assert(!button(0x6f,1)); /* original handler advances Pan */
 *focus=volume;assert(button(0x6f,1));shift(1);assert(!focused_trim&&*focus==volume);assert(button(0x6f,0));shift(0);
 *focus=volume;assert(button(0x6f,1));hidden=1;note_native_tick();assert(!focused_trim&&*focus==volume);assert(button(0x6f,0));hidden=0;
 note_native_tick();assert(cc(0x11,60));*focus=pan;note_native_tick();assert(!focused_trim&&*focus==pan);
 assert((music_sequence_colours[1]&255)==0&&((music_sequence_colours[1]>>8)&255)>128);assert(music_sequence_colours[1]!=music_sequence_colours[12]);
 struct music_project saved;music_snapshot(&saved);music_reset();music_replace(&saved);assert(music_pad_colour(0)==3);
 for(unsigned value=0;value<128;++value){assert(cc(0x11,value));assert((unsigned)music_pad_colour(0)==music_colour_choice(value*MUSIC_IDENTITY_CHOICES/128));}
 struct music_project pink,decoded;char *payload=malloc(MUSIC_PAYLOAD_BYTES);assert(payload);
 music_snapshot(&pink);assert(pink.pad_colours[0]==13);int encoded=music_encode(&pink,payload,MUSIC_PAYLOAD_BYTES);assert(encoded>0&&music_decode(payload,(size_t)encoded,&decoded)&&decoded.pad_colours[0]==13);
 assert(music_set_pad_colour(0,5));music_snapshot(&pink);assert(pink.pad_colours[0]==5&&music_identity_rgb(5)==music_sequence_colours[4]);free(payload);
 assert(paints);puts("PASS production Mix K2 Pad Colour, Shift K2 native Kit Volume, K1 Volume, encoder insertion/release across Shift/page changes, focus handoff, K2-only overlay, green and project persistence");
 munmap(image,0x7500000);return 0;
}
