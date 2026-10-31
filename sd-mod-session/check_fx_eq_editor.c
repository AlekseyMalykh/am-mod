#include "src/fx_eq_editor.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static unsigned curves,texts;
static void rect(void*p,int x,int y,int w,int h,uint32_t colour){(void)p;(void)colour;assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240);++curves;}
static void text(void*p,const char*s,int x,int y,int w,int h,unsigned size,uint32_t colour,int centre){(void)p;(void)colour;(void)centre;assert(s&&x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240&&size);++texts;}
int main(void){
 struct fx_eq_config c;fx_eq_default(&c);struct fx_eq_editor e;
 assert(fx_eq_editor_open(&e,&c,48000));assert(fx_eq_editor_open(&e,&e.config,48000)&&e.config.count==4);
 assert(!fx_eq_editor_open(&e,&c,0));
 assert(fx_eq_editor_button(&e,1,0)==EQ_EDITOR_REDRAW);
 while(e.config.count<FX_EQ_MAX_BANDS)assert(fx_eq_editor_button(&e,1,0)==EQ_EDITOR_CHANGED);
 assert(fx_eq_editor_button(&e,1,0)==EQ_EDITOR_REDRAW&&strstr(e.message,"Maximum"));
 assert(fx_eq_editor_button(&e,0,0)==EQ_EDITOR_REDRAW);
 fx_eq_editor_control(&e,0x64,127,0);fx_eq_editor_control(&e,0x11,110,0);
 struct resample_painter p={0,rect,text};fx_eq_editor_paint(&e,&p);
 assert(curves>276&&texts>10);unsigned revision=e.paint_revision;fx_eq_editor_paint(&e,&p);assert(e.paint_revision==revision);
 double response=fx_eq_response_db(&e.config,48000,e.config.band[e.selected].hz);assert(response>10);
 fx_eq_editor_button(&e,2,0);assert(e.config.bypass&&fabs(fx_eq_response_db(&e.config,48000,1000))<.001);
 fx_eq_editor_paint(&e,&p);fx_eq_editor_button(&e,2,0);assert(!e.config.bypass);
 fx_eq_editor_button(&e,1,0);while(e.config.count)assert(fx_eq_editor_button(&e,2,0)==EQ_EDITOR_CHANGED);
 assert(!e.config.count&&e.selected==0);fx_eq_editor_paint(&e,&p);fx_eq_editor_button(&e,1,0);assert(e.config.count==1);
 for(unsigned i=0;i<1000;++i){unsigned cc=0x10+i%3;fx_eq_editor_control(&e,cc,i%128,(i/128)%2);assert(fx_eq_valid(&e.config,48000));fx_eq_editor_paint(&e,&p);}
 puts("PASS graphic EQ editor: eight addable/removable points, alias-safe reopen, curve matches DSP, bypass, limits, bounded 320x240 drawing and parameter controls");
 return 0;
}
