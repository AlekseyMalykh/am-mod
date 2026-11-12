#define PAD_EQ_PROOF_MAIN audio_proof_main
#include "check_pad_eq_native.c"
#include "src/music_ui.h"
#include "pad_lighting_project.h"
#include "src/mod_build.h"
static unsigned char screen[0x1600],controller[0x22b0];
static unsigned char sequence_screen[0x300],sequence_model[0x800],empty_sequence[0x800],current_things[0x1300];
static void *sequence_array[128];
static unsigned char repeat_screen[0xf00];static int repeat_drawing;
static void*current_context;
static unsigned char muted_sample[0x80];
static int test_program;
static void *sample_lookup(void *program,int pad){assert(program==&test_program);return pad==18?muted_sample:NULL;}
static int sequence_only,project_open,repeat_open,knob_open;
static unsigned char knob_screen[0x300],config_screen[0x300];
static int config_open;
static unsigned char project_screen[0x2000];
static _Alignas(8) unsigned char tempo_app[0xe0],tempo_audio[0x150],tempo_core[0x590],tempo_control[0x30],tempo_seq[0xac8];
static void set_tempo(void *p,float bpm){assert(p==tempo_control);F(tempo_seq,0xac0,float)=bpm;}
static unsigned draws,repaints,primes;static char captions[8192];static unsigned current_pad=18;
static unsigned char fx_screen[0x300],fx_header[0x240];
static uintptr_t value_source[7][2],value_vtable[3],var_vtable[24],var_type;
static unsigned values_read,values_destroyed,header_bars;static int header_testing,header_disabled;
static uint32_t draw_ink,header_caption_ink;
static unsigned spectrum_bars,muted_bars,graph_clears;static int graph_cleared;
extern void check_header_value_get(void);
void check_header_value_fill(void*out,void*source){++values_read;F(out,0,uintptr_t)=(uintptr_t)&var_type;F(out,8,uint64_t)=F(source,8,uint64_t);}
static int var_bool(void*type,void*data){assert(type==&var_type);return *(uint64_t*)data!=0;}
static void var_destroy(void*type,void*data){assert(type==&var_type);(void)data;++values_destroyed;}
int note_native_current(char*name,size_t size){assert(size>7);strcpy(name,"Shared");return 1;}
int note_native_pad_index(unsigned*out){*out=current_pad;return 1;}
void*note_native_context(void){return current_context;}
void note_native_observe_root(void*p){assert(p==screen);}
int note_native_tick(void){return 1;}
int music_archive_status(void){return 0;}
int project_exit_start(uintptr_t b,void*s){(void)b;(void)s;assert(!"Unexpected save in EQ test");return 0;}
int project_exit_status(void){return 0;}
const char*project_exit_message(void){return "";}
void*workflow_project_services(void){return NULL;}
void resample_ui_prime_eq(void){++primes;}
static int shown(void*p){if(p==config_screen)return config_open;if(p==knob_screen)return knob_open;if(p==repeat_screen)return repeat_open;if(p>=(void*)repeat_screen&&p<(void*)(repeat_screen+sizeof(repeat_screen)))return ((char*)p-(char*)repeat_screen-0x1c0)%0x440==0;assert(p==screen||p==sequence_screen||p==project_screen);if(p==project_screen)return project_open;return p==sequence_screen?sequence_only:!sequence_only;}
static int blocked(void*p){assert(p==screen||p==sequence_screen||p==project_screen||p==repeat_screen||p==knob_screen||p==config_screen);return 0;}
static int enabled(void*p){assert(p==screen+0xe50||p==fx_header);return p!=fx_header||!header_disabled;}
static void ui_repaint(void*p,uint64_t a,uint64_t b){assert((p==screen||p==sequence_screen||p==project_screen)&&!a&&!b);++repaints;}
/* fix6: Filter/Filt Env spectrum uses the EQ colour at alpha e0 (was 60). */
static void draw_colour(void*g,uint32_t c){assert(g==screen&&((c>>24)==255||c==0xe05a4419||c==0xe05b5b5b));draw_ink=c;if(c==0xe05b5b5b||c==0xff5b5b5b)++muted_bars;}
static void draw_rect(void*g,int x,int y,int w,int h){assert(g==screen&&x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240);if(draw_ink==0xff000000&&x==0&&y==88&&w==320&&h==110){++graph_clears;graph_cleared=1;}if(draw_ink==0xe05a4419){assert(x>=15&&x+w<=304&&y>=94&&y+h<=188);++spectrum_bars;}if(repeat_drawing)assert(y>=5&&y+h<=38);if(header_testing&&y==25)++header_bars;++draws;}
static void font_make(void*p,int type,float size){assert(p&&!type&&size>=8&&size<=30);*(uintptr_t*)p=0;}
static void font_set(void*g,const void*f){assert(g==screen&&f);}
static void font_delete(void*f){assert(f);}
static void draw_text(void*g,const uint64_t*s,int x,int y,int w,int h,int flags,int lines,float scale){(void)x;(void)y;(void)w;(void)h;(void)flags;(void)lines;(void)scale;assert(g==screen&&s);const char*str=*(const char*const*)s;if(header_testing&&!strcmp(str,"Bypass"))header_caption_ink=draw_ink;assert(strlen(captions)+strlen(str)+2<sizeof(captions));strcat(captions,str);strcat(captions,"\n");}
static int key(unsigned code,int down){unsigned char e[16]={0};e[8]=code;e[9]=down;return music_ui_button(e);}
static void knob(unsigned cc,unsigned v){unsigned char e[16]={0};e[8]=cc;e[9]=v;assert(music_ui_control(e));}
static void paint_ui(void){captions[0]=0;graph_cleared=0;music_ui_sample(screen,screen);}
static void noop_paint(void*p,void*g){(void)p;(void)g;}
int workflow_mono_open(void*p){(void)p;return 0;}
int main(int argc,char**argv){
 assert(!audio_proof_main(argc,argv));assert(F((void*)base,0x16739f4,uint32_t)==0x91394280); /* Native constructor: B3 Filter group is SampleScreen+e50, not Chop Split+a10. */phase="physical EQ tab controls";music_reset();
 patch(0x112fd50,shown);patch(0x112f6b0,blocked);patch(0x1149880,ui_repaint);patch(0x102d5e0,draw_colour);patch(0x102e000,draw_rect);patch(0x1082810,font_make);patch(0x102dca0,font_set);patch(0x1070d60,font_delete);patch(0x10817b0,draw_text);
 F(screen,0,uintptr_t)=base+0x6e212a0;F(screen,0x108,void*)=controller;F(screen,0x7f0+0x218,int)=-1;F(screen,0xe50+0x218,int)=0;
 music_ui_init(base);paint_ui();assert(!key(0x0e,1)&&!music_ui_owns(0x10)); /* First native Filter page untouched. */
 /* Only the active B3 group watches audio. All native controls remain unowned. */
 F(screen,0xe50+0x21c,int)=2;paint_ui();assert(!graph_cleared&&music_ui_filter_spectrum_visible(screen)&&pad_eq_spectrum_wanted(current_pad));
 usleep(650000);music_ui_tick();assert(pad_eq_spectrum_wanted(current_pad));pad_eq_spectrum_dirty();assert(pad_eq_spectrum_wanted(current_pad)); /* idle page must still capture the next note */
 for(unsigned i=0;i<2048;++i)left[i]=right[i]=.5f*(float)sin(2*3.141592653589793*1000*i/48000);
 pad_eq_spectrum_capture(current_pad,channels,2,2048,48000);spectrum_bars=0;paint_ui();assert(!graph_cleared&&spectrum_bars&&!music_ui_owns(0x10));
 F(screen,0xe50+0x218,int)=1;spectrum_bars=0;paint_ui();assert(!graph_cleared&&spectrum_bars&&!music_ui_owns(0x11));
 patch(0x286bbe0,sample_lookup);current_context=current_things;F(current_things,0xb8,void*)=&test_program;
 muted_sample[0x78]=1;uint32_t tint=0;assert(music_ui_waveform_tint(&tint)&&tint==0x999999);
 assert(music_set_mod_setting(MUSIC_SETTING_WAVE,0));assert(music_ui_waveform_tint(&tint)&&tint==0x999999);
 for(int page=0;page<2;++page){F(screen,0xe50+0x218,int)=page;pad_eq_spectrum_capture(current_pad,channels,2,2048,48000);muted_bars=0;paint_ui();assert(muted_bars&&!graph_cleared);}
 assert(key(0x0e,1)&&key(0x0e,0));pad_eq_spectrum_capture(current_pad,channels,2,2048,48000);muted_bars=0;paint_ui();assert(muted_bars&&strstr(captions,"EQ"));
 assert(!key(0x0e,1));key(0x0e,0);F(screen,0xe50+0x218,int)=1;
 current_pad=19;assert(!music_ui_waveform_tint(&tint));current_pad=18;
 muted_sample[0x78]=0;assert(!music_ui_waveform_tint(&tint));assert(music_set_mod_setting(MUSIC_SETTING_WAVE,1));
 F(current_things,0xb8,void*)=NULL;current_context=NULL;
 unsigned clears=graph_clears;paint_ui();assert(graph_clears==clears); /* never erase native curves, including silence */
 current_pad=19;paint_ui();assert(pad_eq_spectrum_wanted(19)&&!pad_eq_spectrum_wanted(18));current_pad=18;
 F(screen,0xe50+0x21c,int)=1;paint_ui(); /* An inactive remembered B3 index adds no bars. */
 spectrum_bars=0;paint_ui();assert(!spectrum_bars&&!graph_cleared&&!music_ui_filter_spectrum_visible(screen)&&!pad_eq_spectrum_wanted(current_pad));F(screen,0xe50+0x21c,int)=2;
 assert(primes>0);unsigned before_primes=primes;
 F(screen,0xe50+0x218,int)=1;assert(key(0x0e,1)&&key(0x0e,0)&&primes>before_primes&&music_ui_owns(0x10));paint_ui();assert(strstr(captions,"EQ")&&strstr(captions,"Bypass")&&strstr(captions,"Type"));
 /* Construct the actual native B3 group location/layout. The old a10 fixture
  * tested the implementation's guessed address and missed this regression. */
 void*header=screen+0xe50;F(header,0,uintptr_t)=base+0x706e940;F(header,0x18,void*)=screen;F(header,0x20,int)=214;F(header,0x24,int)=6;F(header,0x28,int)=106;F(header,0x2c,int)=29;F(header,0x21c,int)=2;F(header,0x214,int)=2;
 FN(0xf535c0,void(*)(void*,const char*))((char*)header+0xb8,"Filter Env");patch(0x10f98d0,enabled);captions[0]=0;unsigned old_draws=draws;music_ui_tab(header,screen);assert(strstr(captions,"EQ")&&draws==old_draws+4);
 F(controller,0x22a0,unsigned char)=1;captions[0]=0;music_ui_tab(header,screen);assert(strstr(captions,"Stereo > Mono")&&!strstr(captions,"EQ"));F(controller,0x22a0,unsigned char)=0;
 knob(0x12,20);knob(0x10,72);knob(0x11,22);uint64_t saved=music_get_pad_eq(18);assert(saved&&pad_eq_published(18)==saved&&!music_get_pad_eq(19));
 F(controller,0x22a0,unsigned char)=1;knob(0x11,90);uint64_t q=music_get_pad_eq(18);assert((q&~(UINT64_C(127)<<14))==(saved&~(UINT64_C(127)<<14)));knob(0x12,127);assert(music_get_pad_eq(18)==q);paint_ui();assert(strstr(captions,"Q"));
 F(controller,0x22a0,unsigned char)=0;knob(0x12,1);paint_ui();assert(strstr(captions,"BYPASS"));
 knob(0x12,110);knob(0x10,20);knob(0x11,110);
 uint64_t reset_before=music_get_pad_eq(18),factory=pad_eq_default();
 assert(music_ui_reset_knob(0,0));uint64_t reset_after=music_get_pad_eq(18);
 assert(reset_after==((reset_before&~UINT64_C(127))|(factory&127)));
 assert(music_ui_reset_knob(1,1));uint64_t reset_q=music_get_pad_eq(18);
 assert(reset_q==((reset_after&~(UINT64_C(127)<<14))|(factory&(UINT64_C(127)<<14))));
 assert(music_ui_reset_knob(2,0));uint64_t type_mask=UINT64_C(15)<<21;
 assert(music_get_pad_eq(18)==((reset_q&~type_mask)|(factory&type_mask)));
 assert(music_get_pad_eq(18)&(UINT64_C(1)<<24));
 puts("PASS EQ per-knob factory reset leaves unrelated fields intact; Type reset includes native Bypass choice");
 current_pad=19;paint_ui();assert(strstr(captions,"BYPASS"));current_pad=18;
 assert(!key(0x0e,1)&&!music_ui_owns(0x10));F(screen,0xe50+0x218,int)=0;paint_ui();assert(!strstr(captions,"EQ"));
 F(screen,0xe50+0x218,int)=1;assert(key(0x0e,1)&&key(0x0e,0));assert(!key(0x0c,1)&&!music_ui_owns(0x10));
 assert(key(0x0e,1)&&key(0x0e,0));music_ui_destroyed(screen);paint_ui();assert(!music_ui_owns(0x10)&&!strstr(captions,"EQ"));
 puts("PASS Filter/Filter Env spectrum preserves native graph pixels: no opaque clear, bounded translucent bars, selected pad and native knobs preserved, inactive tabs excluded; EQ controls unchanged");
 phase="native header Boolean versus unrelated listener count";
 value_vtable[2]=(uintptr_t)check_header_value_get;var_vtable[6]=(uintptr_t)var_bool;var_vtable[23]=(uintptr_t)var_destroy;var_type=(uintptr_t)var_vtable;
 for(unsigned i=0;i<7;++i)value_source[i][0]=(uintptr_t)value_vtable;
 F(fx_screen,0,uintptr_t)=base+0x6e1d890;F(fx_header,0,uintptr_t)=base+0x706e940;F(fx_header,0x18,void*)=fx_screen;
 F(fx_header,0x28,int)=106;F(fx_header,0x2c,int)=29;F(fx_header,0xb8,const char*)="Bypass";F(fx_header,0x21c,int)=1;F(fx_header,0x168,void*)=value_source[6];
 header_testing=1;
 for(unsigned on=0;on<2;++on)for(unsigned listeners=0;listeners<4;++listeners)for(unsigned pressed=0;pressed<3;++pressed){
  value_source[6][1]=on;F(fx_header,0x1d4,int)=(int)listeners;F(fx_header,0x160,int)=(int)pressed;header_bars=0;header_caption_ink=0;
  music_ui_tab(fx_header,screen);assert(header_caption_ink==(on?0xffffc247u:0xffffffffu)&&header_bars==on);
 }
 header_disabled=1;music_ui_tab(fx_header,screen);assert(header_caption_ink==0xff707070u);header_disabled=0;header_testing=0;
 assert(values_read==values_destroyed&&values_read==25);
 puts("PASS real native Value::getValue and var Boolean/destructor dispatch: both Bypass states ignore listener count and pressed feedback; disabled colour retained");
 phase="Note Repeat overlay excludes custom Sample controls";
 F(screen,0x7f0+0x218,int)=1;paint_ui();assert(key(0x0d,1)&&key(0x0d,0)&&music_ui_owns(0x10));
 music_ui_requested(0x0b,1);assert(!music_ui_owns(0x10)&&!key(0x0d,1));paint_ui();assert(!strstr(captions,"Set Orig Note"));music_ui_requested(0x0b,0);
 phase="real Seq vtable slots and Shift K1 colour selection";
 F((void*)base,0x6d3ba84,uint32_t)=0x529848e9;F((void*)base,0x6d3ba88,uint32_t)=0x72bfffe9;
 memcpy((void*)(base+0x6d3b400),lighting_project_before,sizeof(lighting_project_before));
 memcpy((void*)(base+0x1643908),lighting_dispatch_before,4);memcpy((void*)(base+0x1644ff4),lighting_rows_before,4);
 assert(music_ui_install(noop_paint,noop_paint,noop_paint));
 const uintptr_t config_vptr=0x6e1e9e8;
 {
  F(config_screen,0,uintptr_t)=base+config_vptr;
  void(*paint_config)(void*,void*)=(void(*)(void*,void*))F((void*)base,config_vptr+0xd8,uintptr_t);
  assert(!music_ui_pad_settings_visible());
  config_open=1;paint_config(config_screen,screen);music_ui_tick();assert(music_ui_pad_settings_visible());
  config_open=0;music_ui_tick();assert(!music_ui_pad_settings_visible());
 }
 F(knob_screen,0,uintptr_t)=base+0x6e1e258;
 void(*knob_paint)(void*,void*)=(void(*)(void*,void*))F((void*)base,0x6e1e258+0xd0,uintptr_t);
 knob_open=1;knob_paint(knob_screen,screen);assert(music_ui_knob_fx_active());
 knob_open=0;music_ui_tick();assert(!music_ui_knob_fx_active());
 music_ui_requested(0x31,1);music_ui_tick();assert(!music_ui_knob_fx_active());music_ui_requested(0x31,0);

 assert(!memcmp((void*)(base+0x6d3b400),lighting_project_after,sizeof(lighting_project_after)));
 assert(F((void*)base,0x1644ff4,uint32_t)==0xd28000c1); /* six-row viewport retained */
 assert(F((void*)base,0x6d3ba84,uint32_t)==(0x52800009u|(0xb000u<<5)));
 F(repeat_screen,0,uintptr_t)=base+0x6e1f060;
 const char*divisions[]={"1/8","1/8T","1/16","1/16T","1/32","1/32T"};
 for(unsigned i=0;i<6;++i){void*b=repeat_screen+0x1c0+i*0x220;F(b,0,uintptr_t)=base+0x706e940;F(b,0xb8,const char*)=divisions[i];F(b,0x20,int)=(i/2)*106+(i>=4);F(b,0x28,int)=106;F(b,0x1d4,int)=3;value_source[i][1]=i==2;F(b,0x168,void*)=value_source[i];}
 uintptr_t*rvt=F(repeat_screen,0,uintptr_t*);unsigned before_repeat=draws;
 ((void(*)(void*,void*))rvt[0xd0/8])(repeat_screen,screen);assert(draws==before_repeat);
 repeat_open=1;repeat_drawing=1;captions[0]=0;((void(*)(void*,void*))rvt[0xd8/8])(repeat_screen,screen);repeat_drawing=0;
 assert(music_ui_repeat_active());music_ui_requested(0x0b,0);music_ui_tick();assert(music_ui_repeat_active()&&!key(0x0d,1)&&!key(0x0d,0));
 repeat_open=0;music_ui_tick();assert(!music_ui_repeat_active());
 unsigned before_hidden=draws;repeat_drawing=1;((void(*)(void*,void*))rvt[0xd8/8])(repeat_screen,screen);repeat_drawing=0;
 assert(draws==before_hidden&&!music_ui_repeat_active()); /* queued hidden paint must not reclaim the header */
 music_ui_destroyed(repeat_screen);music_ui_tick();
 assert(strstr(captions,"1/8\n")&&strstr(captions,"1/16\n")&&strstr(captions,"1/32\n")&&!strstr(captions,"1/8T"));
 puts("PASS Repeat header38px amber with only visible native divisions, no white focus arms or footer clearing; Knob FX background saturated amber startup patch");
 F(sequence_screen,0,uintptr_t)=base+0x6e23e38;F(current_things,0x1c8,void*)=sequence_model;F(sequence_model,0x480,int)=11;current_context=current_things;
 uintptr_t*vt=F(sequence_screen,0,uintptr_t*);void(*paint_sequence)(void*,void*)=(void(*)(void*,void*))vt[0xd8/8];
 F(current_things,0,uintptr_t)=base+0x6ecc648;F(current_things,0x1250,void*)=sequence_array;F(current_things,0x125c,int)=128;
 F(sequence_model,0,uintptr_t)=F(empty_sequence,0,uintptr_t)=base+0x6ecdff0;F(sequence_model,0x490,unsigned char)=1;
 F(sequence_model,0x6f8,uint64_t)=1;F(empty_sequence,0x490,unsigned char)=1; /* initialized length but no events */
 sequence_array[11]=sequence_model;sequence_array[12]=empty_sequence;
 assert(music_ui_sequence_presence(0)==(1u<<11)&&!music_ui_sequence_presence(1)&&!music_ui_sequence_presence(8));
 unsigned char track_map[0xe0]={0},clip_node[0x30]={0},empty_clip[0x800]={0};void *map_bounds[]={track_map,track_map+sizeof(track_map)};
 void *map_header=track_map+0xb8;F(empty_sequence,0x620,void*)=map_bounds;
 F(map_header,8,void*)=F(map_header,16,void*)=F(map_header,24,void*)=clip_node;
 F(clip_node,0,unsigned)=1;F(clip_node,8,void*)=map_header;F(clip_node,0x28,void*)=empty_clip;
 assert(music_ui_sequence_presence(0)==(1u<<11));
 F(empty_clip,0x710,uint64_t)=1;assert(music_ui_sequence_presence(0)==((1u<<11)|(1u<<12)));
 F(empty_clip,0x710,uint64_t)=0;assert(music_ui_sequence_presence(0)==(1u<<11));
 F(empty_sequence,0x620,void*)=NULL;map_header=empty_sequence+0x630;
 F(map_header,8,void*)=F(map_header,16,void*)=F(map_header,24,void*)=clip_node;F(clip_node,8,void*)=map_header;
 F(empty_clip,0x710,uint64_t)=1;assert(music_ui_sequence_presence(0)==((1u<<11)|(1u<<12)));
 F(empty_clip,0x710,uint64_t)=0;assert(music_ui_sequence_presence(0)==(1u<<11));
 F(sequence_model,0x6f8,uint64_t)=0;assert(!music_ui_sequence_presence(0));F(sequence_model,0x6f8,uint64_t)=1;
 sequence_only=1;paint_sequence(sequence_screen,screen);assert(music_ui_sequence_visible()&&music_ui_active());music_ui_requested(0x31,1);knob(0x10,127);assert(music_sequence_colour(11)==7&&music_sequence_colour(12)==0);
 music_ui_requested(0x31,0); /* Shift released while its K1 edit waits in FIFO. */
 unsigned char queued_colour[16]={[8]=0x10,[9]=0};
 assert(music_ui_control_at(queued_colour,1));assert(music_sequence_colour(11)==0);
 queued_colour[9]=127;assert(!music_ui_control_at(queued_colour,0));assert(music_sequence_colour(11)==0);
 music_ui_requested(0x31,1);knob(0x10,127);assert(music_sequence_colour(11)==7);
 F(sequence_model,0x480,int)=12;knob(0x10,11);assert(music_sequence_colour(12)==0&&music_sequence_colour(11)==7);
 captions[0]=0;paint_sequence(sequence_screen,screen);assert(strstr(captions,"Seq Colour")&&strstr(captions,"Default"));
 music_ui_requested(0x31,0);assert(!music_ui_owns(0x10));current_context=NULL;
#ifdef SEQ_COLOUR_ONLY
 puts("PASS Seq Colour: Shift+K1 edits selected sequence; queued edit survives Shift release; unshifted K1 is not claimed");return 0;
#endif
 sequence_only=0;F(screen,0x7f0+0x218,int)=1;paint_ui();assert(key(0x0d,1)&&key(0x0d,0)&&music_ui_owns(0x10));
 F(project_screen,0,uintptr_t)=base+0x6e1f788;F(project_screen,0x100,void*)=controller;project_open=1;music_ui_project(project_screen,screen);
 assert(music_ui_pad_settings_visible());
 assert(!music_ui_owns(0x10)&&!key(0x6f,1)); /* Native Project menu owns its encoder even above Lights. */
 phase="Project subpages keep +/- local and preserve Back";
 assert(!key(0x36,1)&&!key(0x36,0));
 music_ui_request_project(project_screen);music_ui_tick();
 assert(key(0x36,1)&&key(0x36,0)&&key(0x37,1)&&key(0x37,0));
 captions[0]=0;music_ui_project(project_screen,screen);assert(strstr(captions,"Key / Scale / Tempo"));
 void *old_app=F((void*)base,0x71e3dc8,void*);
 F((void*)base,0x71e3dc8,void*)=tempo_app;F(tempo_app,0,uintptr_t)=base+0x6e07288;
 F(tempo_app,0xd8,void*)=tempo_audio;F(tempo_audio,0,uintptr_t)=base+0x6e0c430;
 F(tempo_audio,0x148,void*)=tempo_core;F(tempo_core,0,uintptr_t)=base+0x6e0cf38;
 F(tempo_core,0x588,void*)=tempo_control;F(tempo_control,0,uintptr_t)=base+0x6d95318;
 F(tempo_control,0x28,void*)=tempo_seq;F(tempo_seq,0xac0,float)=81.5f;patch(0x1956f00,set_tempo);
 knob(0x12,127);assert(F(tempo_seq,0xac0,float)==300);
 knob(0x12,0);assert(F(tempo_seq,0xac0,float)==20);
 music_ui_requested(0x31,1);knob(0x12,127);assert(fabsf(F(tempo_seq,0xac0,float)-20.99f)<.001f);
 assert(music_ui_reset_knob(2,1)&&F(tempo_seq,0xac0,float)==20);
 music_ui_requested(0x31,0);assert(music_ui_reset_knob(2,0)&&F(tempo_seq,0xac0,float)==120);
 F((void*)base,0x71e3dc8,void*)=old_app;
 puts("PASS Master Tempo K3 reaches both endpoints, Shift edits hundredths, normal/fine resets");
 assert(key(0x0c,1)&&key(0x0c,0));
 assert(!key(0x37,1)&&!key(0x37,0));
 char preferences[]="/tmp/am-pad-eq-XXXXXX";
 assert(mkdtemp(preferences)&&music_lighting_preferences_open(preferences));
 phase="Project Mod Settings tabs, encoder, reset and Standard/AM Mod switching";
 project_open=1;music_ui_project(project_screen,screen);
 assert(music_set_pad_colour(18,13)&&music_set_sequence_colour(11,4));
 music_ui_request_project((void*)((uintptr_t)project_screen|2));music_ui_tick();captions[0]=0;music_ui_project(project_screen,screen);assert(strstr(captions,"Pad lighting")&&strstr(captions,"Am MOD")&&strstr(captions,"Restore recommended"));
#ifdef SCREEN_MIRROR_FLAG
 assert(strstr(captions,"Screen Mirror")&&access(SCREEN_MIRROR_FLAG,F_OK)!=0);
 knob(0x64,1);
 assert(key(0x6f,1)&&key(0x6f,0)&&!access(SCREEN_MIRROR_FLAG,F_OK));
 knob(0x10,0);assert(access(SCREEN_MIRROR_FLAG,F_OK)!=0);
 knob(0x64,127);
 puts("PASS Screen Mirror defaults off; encoder toggles on, K1 toggles off");
#endif
 knob(0x10,0);assert(music_pad_lighting_mode()==MUSIC_LIGHTING_STANDARD);
 assert(music_pad_colour(18)==13&&music_sequence_colour(11)==4);
 knob(0x10,0);assert(music_pad_lighting_mode()==MUSIC_LIGHTING_STANDARD);knob(0x10,127);assert(music_pad_lighting_mode()==MUSIC_LIGHTING_MOD);
 assert(key(0x0e,1)&&key(0x0e,0));captions[0]=0;music_ui_project(project_screen,screen);assert(strstr(captions,"Preview from end")&&strstr(captions,"1000 ms"));
 knob(0x10,127);uint32_t wave=0;assert(music_mod_setting(MUSIC_SETTING_WAVE)&&music_ui_waveform_tint(&wave)&&wave==music_identity_rgb(13));
 knob(0x64,1);knob(0x10,127);assert(music_mod_setting(MUSIC_SETTING_SPECTRUM));
 knob(0x64,1);knob(0x10,0);assert(music_tail_ms((unsigned)music_mod_setting(MUSIC_SETTING_TAIL))==100);
 knob(0x64,1);uint64_t before_reset=music_mod_settings_snapshot();
 assert(key(0x6f,1)&&key(0x6f,1)&&key(0x6f,0)&&music_mod_settings_snapshot()==before_reset);
 captions[0]=0;music_ui_project(project_screen,screen);assert(strstr(captions,"Only this section is reset.")&&strstr(captions,"Sample")&&strstr(captions,"Cancel"));
 knob(0x10,0);knob(0x64,127);assert(key(0x6f,1)&&key(0x6f,0)&&music_mod_settings_snapshot()==before_reset);
 assert(key(0x0c,1)&&key(0x0c,0)&&music_mod_settings_snapshot()==before_reset);
 captions[0]=0;music_ui_project(project_screen,screen);assert(strstr(captions,"Restore recommended")&&!strstr(captions,"Only this section is reset."));
 assert(key(0x6f,1)&&key(0x6f,0)&&key(0x0e,1)&&key(0x0e,0)&&!music_mod_setting(MUSIC_SETTING_WAVE)&&!music_mod_setting(MUSIC_SETTING_SPECTRUM));
 assert(music_tail_ms((unsigned)music_mod_setting(MUSIC_SETTING_TAIL))==1000);
 assert(key(0x0e,1)&&key(0x0e,0));
 uint32_t saved_cursor=music_palette_rgb(music_service_colour(0));
 captions[0]=0;music_ui_project(project_screen,screen);
 assert(strstr(captions,"RGB Palette Editor")&&strstr(captions,"Press DATA: open"));
 knob(0x10,127);assert(music_service_colour(0)==MUSIC_USER_COLOURS);
 assert(key(0x6f,1)&&key(0x6f,0));
 captions[0]=0;music_ui_project(project_screen,screen);
 assert(strstr(captions,"#FFFFFF")&&strstr(captions,"Red")&&strstr(captions,"Save"));
 knob(0x10,13);knob(0x11,23);knob(0x12,37);
 uint64_t draft_light;
 assert(music_ui_palette_light(music_service_colour(0),&draft_light)&&(draft_light&0xffffffu)==0x1a2e4a);
 assert(music_palette_rgb(music_service_colour(0))==saved_cursor);
 assert(key(0x0e,1)&&key(0x0e,0)&&music_palette_rgb(music_service_colour(0))==0x1a2e4a);
 uint64_t palette_ticket=music_ui_palette_ticket(),light=1;
 assert(palette_ticket&&music_ui_palette_light(14,&light)&&!light);
 music_ui_palette_pad(14,1,0,100,palette_ticket);
 music_ui_palette_pad(14,0,0,101,palette_ticket);
 assert(key(0x0d,1)&&key(0x0d,0));
 knob(0x64,1);assert(key(0x0e,1)&&key(0x0e,0)); /* encoder types A */
 knob(0x11,95);
 assert(music_ui_palette_light(14,&light)&&(light&0xffffffu)==0x00bf00);
 assert(!music_palette_present(14)&&music_ui_palette_light(15,&light)&&!light);
 knob(0x12,63);
 assert(music_ui_palette_light(14,&light)&&(light&0xffffffu)==0x00bf7e);
 knob(0x12,0);
 palette_ticket=music_ui_palette_ticket();
 music_ui_palette_pad(15,1,0,150,palette_ticket);
 music_ui_palette_pad(15,0,0,151,palette_ticket);
 assert(music_ui_palette_light(14,&light)&&(light&0xffffffu)==0x00bf00);
 assert(key(0x0d,1)&&key(0x0d,0));knob(0x10,127);
 palette_ticket=music_ui_palette_ticket();
 music_ui_palette_pad(13,1,0,200,palette_ticket); /* pad 14 types a */
 music_ui_palette_pad(13,0,0,201,palette_ticket);
 assert(key(0x0e,1)&&key(0x0e,0));knob(0x10,63);
 assert(!music_palette_present(14)&&!music_palette_present(15));
 assert(music_ui_palette_light(15,&light)&&(light&0xffffffu)==0x7e0000);
 knob(0x64,127); /* encoder returns to the first edited slot */
 assert(music_ui_palette_light(14,&light)&&(light&0xffffffu)==0x00bf00);
 assert(key(0x0e,1)&&key(0x0e,0));
 assert(music_palette_present(14)&&music_palette_rgb(14)==0x00bf00&&!strcmp(music_palette_name(14),"A"));
 assert(music_palette_present(15)&&music_palette_rgb(15)==0x7e0000&&!strcmp(music_palette_name(15),"a"));
 knob(0x11,0);knob(0x64,1);knob(0x10,100);
 assert(key(0x0c,1)&&key(0x0c,0)&&!music_ui_palette_ticket());
 assert(music_palette_rgb(14)==0x00bf00&&music_palette_rgb(15)==0x7e0000);
 music_ui_requested(0x31,1);assert(key(0x0d,1)&&key(0x0d,0));music_ui_requested(0x31,0);
 music_ui_requested(0x31,1);assert(key(0x0d,1)&&key(0x0d,0));music_ui_requested(0x31,0);
 assert(key(0x0e,1)&&key(0x0e,0)&&music_palette_rgb(music_service_colour(0))==saved_cursor);
 assert(key(0x0c,1)&&key(0x0c,0));
 puts("PASS 4x4 Palette pad/encoder selection, live LEDs, both name inputs, batch Save, Cancel and Shift shortcut");
 knob(0x64,1);knob(0x10,127);assert(music_service_colour(0)==MUSIC_PALETTE_SIZE-1);
 knob(0x64,6);captions[0]=0;music_ui_project(project_screen,screen);assert(strstr(captions,"Restore recommended")&&!strstr(captions,"Cursor / selection"));
 assert(key(0x6f,1)&&key(0x6f,0)&&music_service_colour(0)==MUSIC_PALETTE_SIZE-1);
 captions[0]=0;music_ui_project(project_screen,screen);assert(strstr(captions,"Default colour roles are restored."));
 assert(key(0x0e,1)&&key(0x0e,0)&&music_service_colour(0)==MUSIC_USER_COLOURS);
 assert(key(0x0e,1)&&key(0x0e,0));knob(0x10,0);assert(!music_mod_setting(MUSIC_SETTING_BLINK));
 assert(key(0x6f,1)&&key(0x6f,0));knob(0x64,1);assert(music_mod_setting(MUSIC_SETTING_BLINK)==1);
 assert(key(0x6f,1)&&key(0x6f,0));knob(0x64,1);assert(key(0x6f,1)&&key(0x6f,0)&&music_mod_setting(MUSIC_SETTING_BLINK)==1);
 assert(key(0x0e,1)&&key(0x0e,0)&&music_mod_setting(MUSIC_SETTING_BLINK)==2);
 assert(key(0x0e,1)&&key(0x0e,0));captions[0]=0;music_ui_project(project_screen,screen);
 assert(key(0x36,1)&&key(0x36,0)&&key(0x37,1)&&key(0x37,0));
 captions[0]=0;music_ui_project(project_screen,screen);
 assert(strstr(captions,"About")&&strstr(captions,"Version " AM_MOD_VERSION)&&strstr(captions,AM_MOD_REPO_URL)&&!strstr(captions,"Restore recommended"));
 uint64_t about_snapshot=music_mod_settings_snapshot();knob(0x10,0);knob(0x11,127);knob(0x12,127);knob(0x64,127);assert(key(0x6f,1)&&key(0x6f,0)&&music_mod_settings_snapshot()==about_snapshot);
 assert(key(0x0e,1)&&key(0x0e,0));captions[0]=0;music_ui_project(project_screen,screen);assert(strstr(captions,"Pad lighting"));
 assert(key(0x0d,1)&&key(0x0d,0));captions[0]=0;music_ui_project(project_screen,screen);assert(strstr(captions,"About"));
 assert(key(0x0d,1)&&key(0x0d,0));captions[0]=0;music_ui_project(project_screen,screen);assert(strstr(captions,"Cursor pulse"));
 assert(key(0x0e,1)&&key(0x0e,0)&&key(0x0e,1)&&key(0x0e,0));
 puts("PASS About is the fifth and last tab with shared version/repository, no reset/edit mutation, B2/B3 wrap through all five pages");
 assert(key(0x0c,1)&&key(0x0c,0));project_open=0;sequence_only=1;music_ui_tick();music_ui_requested(0x31,1);
 assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_STANDARD));assert(!music_ui_owns(0x10)&&!music_ui_control((unsigned char[16]){[8]=0x10,[9]=127}));
 assert(music_sequence_colour(11)==4);music_ui_requested(0x31,0);
 phase="Standard retains 16 Levels layout controls";
 sequence_only=0;F(screen,0x7f0+0x218,int)=1;paint_ui();
 assert(key(0x0d,1)&&key(0x0d,0)&&music_ui_owns(0x10));
 paint_ui();assert(strstr(captions,"Layout")&&strstr(captions,"Tonic")&&strstr(captions,"Scale"));
 knob(0x10,40);knob(0x11,127);knob(0x12,127);
 struct music_sample layout;assert(music_get_sample("Shared",&layout));
 assert(layout.mode==MUSIC_PIANO&&layout.tonic==11&&layout.scale==7);
 F(controller,0x22a0,unsigned char)=1;knob(0x10,127);paint_ui();
 assert(music_get_sample("Shared",&layout)&&layout.transpose==24&&strstr(captions,"Transpose16"));
 F(controller,0x22a0,unsigned char)=0;
 music_ui_requested(0x0b,1);assert(!music_ui_owns(0x10)&&!key(0x0d,1));music_ui_requested(0x0b,0);
 paint_ui();assert(key(0x0d,1)&&key(0x0d,0));assert(!key(0x0c,1)&&!music_ui_owns(0x10));
 assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_MOD));
 assert(music_get_sample("Shared",&layout)&&layout.transpose==24&&layout.mode==MUSIC_PIANO);
 puts("PASS Standard retains Lights mode/key/scale and Transpose16 controls, Repeat priority and settings across lighting switches");
 puts("PASS Project Lighting entry and B2/B3/K1 controls; global mode preserves colours and disables Standard Seq Colour interception");
 puts("PASS native Seq screen hook: twelve colours by native sequence index, global and clip event presence, empty initialized slots excluded, Shift K1 only; Note Repeat keeps its controls");char path[128];
 snprintf(path,sizeof(path),"%s/.am-mod-palette",preferences);assert(!unlink(path));
 snprintf(path,sizeof(path),"%s/.am-mod-settings",preferences);assert(!unlink(path));
 assert(!rmdir(preferences));
 return 0;
}
