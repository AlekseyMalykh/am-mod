/* Host UI preview shim: native Graphics calls are recorded, everything else
 * is a no-op returning 0. For layout review only; not a device test. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static FILE *preview_out;
static uint32_t preview_ink=0xffffffffu;static float preview_size=12;
static long preview_noop(void){return 0;}
static int preview_true(void){return 1;}
static void p_colour(void*g,uint32_t c){(void)g;preview_ink=c;}
static void p_rect(void*g,int x,int y,int w,int h){(void)g;fprintf(preview_out,"R %d %d %d %d %08x\n",x,y,w,h,preview_ink);}
static void p_str(uint64_t*s,const char*t){*s=(uint64_t)(uintptr_t)strdup(t?t:"");}
static void p_font(uint64_t*f,int style,float size){(void)style;memcpy(f,&size,sizeof size);}
static void p_setfont(void*g,const uint64_t*f){(void)g;memcpy(&preview_size,f,sizeof preview_size);}
static void p_text(void*g,const uint64_t*s,int x,int y,int w,int h,int align,int lines,float minscale){(void)g;(void)lines;(void)minscale;
 const char*t=(const char*)(uintptr_t)*s;fprintf(preview_out,"T %d %d %d %d %d %.1f %08x %s\n",x,y,w,h,align,preview_size,preview_ink,t?t:"");}
static uint64_t preview_unknown[64];static unsigned preview_unknown_n;
static void* preview_fn(uintptr_t o){
 switch(o){
  case 0x102d5e0:return (void*)p_colour; case 0x102e000:return (void*)p_rect;
  case 0xf535c0:return (void*)p_str; case 0x1082810:return (void*)p_font;
  case 0x102dca0:return (void*)p_setfont; case 0x10817b0:return (void*)p_text;
  case 0x112fd50:return (void*)preview_true;
 }
 unsigned i;for(i=0;i<preview_unknown_n&&preview_unknown[i]!=o;++i);if(i==preview_unknown_n&&i<64)preview_unknown[preview_unknown_n++]=o;
 return (void*)preview_noop;
}
#define PREVIEW_BEGIN(name) do{preview_out=fopen(name,"w");}while(0)
#define PREVIEW_END() do{fclose(preview_out);}while(0)
