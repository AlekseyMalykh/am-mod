#define main unused_rear_ui_main
#include "check_rear_ui.c"
#undef main
#include <stdio.h>
static FILE*output;
static uint32_t ink;
static float font_size;
void rear_ui_wake(void){}
unsigned rear_record_state(void){return REAR_IDLE;}
uint64_t rear_record_frames(void){return 0;}
unsigned rear_native_status(void){return 0;}
const char*rear_native_message(void){return "";}
void rear_audio_meters(float v[2]){v[0]=.4f;v[1]=.2f;}
static int yes(void*p){(void)p;return 1;}
static int no(void*p){(void)p;return 0;}
static void string(void*p,const char*s){*(const char**)p=s;}
static void font(void*p,int style,float size){(void)p;(void)style;font_size=size;}
static void destroy_value(void*p){(void)p;}
static void set_font(void*g,const void*f){(void)g;(void)f;}
static void set_colour(void*g,uint32_t c){(void)g;ink=c;}
static void draw_rect(void*g,int x,int y,int w,int h){(void)g;assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);fprintf(output,"R %d %d %d %d %08x\n",x,y,w,h,ink);}
static void draw_text(void*g,const void*text,int x,int y,int w,int h,int align,int lines,float scale){(void)g;(void)lines;(void)scale;assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);fprintf(output,"T %d %d %d %d %u %08x %u %s\n",x,y,w,h,(unsigned)font_size,ink,(align&2)?2:((align&1)?0:1),*(const char*const*)text);}
static void stub(uintptr_t offset,void*fn){uint32_t code[]={0xd503245f,0x58000070,0xd61f0200,0xd503201f};memcpy((void*)(image+offset),code,16);memcpy((void*)(image+offset+16),&fn,8);__builtin___clear_cache((char*)(image+offset),(char*)(image+offset+24));}
int main(int argc,char**argv){assert(argc==2);image=(uintptr_t)mmap(NULL,0x1800000,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(image!=(uintptr_t)MAP_FAILED);
 stub(0x112fd50,yes);stub(0x112f6b0,no);stub(0xf535c0,string);stub(0x1082810,font);stub(0x102dca0,set_font);stub(0x102d5e0,set_colour);stub(0x102e000,draw_rect);stub(0x10817b0,draw_text);stub(0x1070d60,destroy_value);stub(0xf0a290,destroy_value);
 unsigned char root[0x40]={0};FIELD(root,0,uintptr_t)=image+0x6e22970;open_panel=1;panel_on_record=1;visible_roots=2;settings[0][REAR_MODE]=REAR_MIX;settings[0][REAR_TARGET]=20;settings[0][REAR_PAN]=30;settings[1][REAR_PAN]=70;settings[0][REAR_LEVEL]=80;settings[1][REAR_LEVEL]=65;selected_mask=1;
 output=fopen(argv[1],"w");assert(output);paint(root,NULL);fclose(output);munmap((void*)image,0x1800000);puts("PASS Rear Mix native painter bounds and recorded-pan/level labels");return 0;}
