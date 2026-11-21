/* Exercise the production renderer and its UI-owned rolling history. */
#include "src/mixer_view.c"
#include <assert.h>
static unsigned grey_only,rectangles;
static unsigned vinyl_transport=2;
unsigned step_native_transport_state(void){return vinyl_transport;}
static void paint_rect(void*u,int x,int y,int w,int h,uint32_t colour){
 (void)u;(void)x;(void)y;assert(w>0&&h>0);++rectangles;
 if(grey_only)assert(((colour>>16)&255)==((colour>>8)&255)&&((colour>>8)&255)==(colour&255));
}
static void paint_text(void*u,const char*s,int x,int y,int w,int h,unsigned size,uint32_t colour,int centre){
 (void)u;(void)s;(void)x;(void)y;(void)w;(void)h;(void)size;(void)centre;
 if(grey_only)assert(((colour>>16)&255)==((colour>>8)&255)&&((colour>>8)&255)==(colour&255));
}
int main(void){
 struct resample_painter p={NULL,paint_rect,paint_text};
 struct fx_stage stage={.kind=FX_STAGE_SIDECHAIN};
 struct fx_catalog_entry e={.name="Sidechain",.category="Dynamics"};
 struct mixer_fx_monitor signal={0},silence={0};signal.input_peak=.8f;signal.output_peak=.4f;signal.key_peak=.6f;signal.gain_reduction_db=6;
 assert(fx_visual_paint(&p,&e,&stage,&signal,48000,123,1000));
 assert(fx_visual_paint(&p,&e,&stage,&signal,48000,123,1040));assert(fxv_hist.count==1&&fxv_hist.head==1);
 for(unsigned time=1080;time<=3040;time+=40)assert(fx_visual_paint(&p,&e,&stage,&silence,48000,123,time));
 assert(fxv_hist.count==51&&fxv_hist.head==51&&fxv_hist.in[0]==.8f&&fxv_now==3040);
 assert(fx_visual_paint(&p,&e,&stage,NULL,48000,123,3080));assert(fxv_hist.count==52); /* Missing monitor is silence, not a reset. */
 stage.bypass=1;grey_only=1;
 assert(fx_visual_paint(&p,&e,&stage,&signal,48000,123,3120));
 assert(fxv_hist.count==53&&fxv_hist.in[0]>0&&fxv_hist.in[0]<.8f);
 for(unsigned time=3160;time<=4120;time+=40)assert(fx_visual_paint(&p,&e,&stage,&signal,48000,123,time));
 assert(fxv_hist.in[0]<.02f&&fxv_hist.count==78); /* Smoothly faded in place, never repopulated with dry input. */
 stage.bypass=0;grey_only=0;float faded=fxv_hist.in[0];
 assert(fx_visual_paint(&p,&e,&stage,&signal,48000,123,4160));assert(fxv_hist.in[0]==faded&&fxv_hist.count==79);
 assert(fx_visual_paint(&p,&e,&stage,&silence,48000,123,16000));assert(fxv_hist.count==FXV_HIST);
 for(unsigned i=0;i<FXV_HIST;++i)assert(fxv_hist.in[i]==0);
 /* Shared behavior: every illustrated effect gets the real clock in silence. */
 for(unsigned i=0;i<fx_catalog_count();++i){
  const struct fx_catalog_entry *item=fx_catalog_at(i);struct fx_stage effect;
  if(!item||!fx_catalog_stage(i,48000,&effect))continue;
  effect.bypass=0;fx_visual_paint(&p,item,&effect,&silence,48000,1000+i,20000);assert(!fxv_idle&&fxv_now==20000);
  effect.bypass=1;grey_only=1;fx_visual_paint(&p,item,&effect,&signal,48000,1000+i,20040);grey_only=0;
 }
 struct fx_catalog_entry vinyl={.id=0x56456678,.name="Vinyl",.category="Lo-Fi"};
 struct fx_stage record={.kind=FX_STAGE_NATIVE};record.parameter[2]=.5f;
 vinyl_transport=2;fx_visual_paint(&p,&vinyl,&record,&silence,48000,99,25000);
 fx_visual_paint(&p,&vinyl,&record,&silence,48000,99,25040);assert(!fxv_vinyl_state.running&&!fxv_vinyl_state.angle);
 vinyl_transport=3;fx_visual_paint(&p,&vinyl,&record,&silence,48000,99,25080);double angle=fxv_vinyl_state.angle;
 fx_visual_paint(&p,&vinyl,&record,&silence,48000,99,25120);assert(fxv_vinyl_state.running&&fxv_vinyl_state.angle>angle);
 vinyl_transport=2;fx_visual_paint(&p,&vinyl,&record,&signal,48000,99,25160);angle=fxv_vinyl_state.angle;
 fx_visual_paint(&p,&vinyl,&record,&signal,48000,99,35000);assert(!fxv_vinyl_state.running&&fxv_vinyl_state.angle==angle);
 vinyl_transport=3;fx_visual_paint(&p,&vinyl,&record,&silence,48000,99,35100);assert(fxv_vinyl_state.angle>angle&&fxv_vinyl_state.angle-angle<.36);
 record.bypass=1;angle=fxv_vinyl_state.angle;grey_only=1;
 fx_visual_paint(&p,&vinyl,&record,&signal,48000,99,35140);assert(!fxv_vinyl_state.running&&fxv_vinyl_state.angle==angle);grey_only=0;
 assert(rectangles);puts("PASS all illustrated FX: continuous history in silence, Bypass fade/grey, Vinyl follows transport and retains its stopped angle without resume jumps");
 return 0;
}
