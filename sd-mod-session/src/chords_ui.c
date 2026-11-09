#define _POSIX_C_SOURCE 200809L
#include "shift_badge.h"
#include "chords_ui.h"
#include "voice_guard.h"
#include "chords.h"
#include "music_lighting.h"
#include "music_settings.h"
#include "note_native.h"
#include "music_ui.h"
#include "workflow_ui.h"
#include "step_native.h"
#include "poly_record.h"
#include <math.h>
#ifdef MPC_FAST_RESAMPLE
#include "resample_fault.h"
#endif
#ifdef MPC_RESAMPLE_TRIAL
#include "resample_ui.h"
#endif
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static uintptr_t base;
static _Atomic unsigned ready,owned[112];
static unsigned source=~0u;
static int focus;
static int release_warning;
static int poly_pending;
/* Chords owns Shift while open (native never sees it), so track it here. */
static _Atomic int shift_held;
/* While Chords is open it owns Shift, so the native/music_ui state goes
 * stale; outside it that state is the truth (our copy may have missed a
 * release routed to another window). */
static int shift_active(void){return chords_active()?atomic_load(&shift_held)!=0:music_ui_shift_active();}
static struct timespec poly_requested;
static uint64_t painted;
static void *previous_instrument;
static unsigned reported_active,reports;
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
#define FN(o,T) ((T)(base+(o)))
void chords_ui_init(uintptr_t b){base=b;}
/* ---- Arpeggiator scheduler (JUCE message thread) ----------------------
 * Native MpcPlayHead::getCurrentPosition13ad070 (same owner chain as
 * fx_native_context_capture) gives the tempo. While the transport runs the
 * steps follow the engine tick grid, extrapolated from the native transport
 * timer; when stopped they run from the first held root. */
static double arp_bpm(void){
 void*application=F((void*)base,0x71e3dc8,void*);
 if(!application||F(application,0,uintptr_t)!=base+0x6e07288)return 0;
 void*audio=F(application,0xd8,void*);if(!audio||F(audio,0,uintptr_t)!=base+0x6e0c430)return 0;
 void*core=F(audio,0x148,void*);if(!core||F(core,0,uintptr_t)!=base+0x6e0cf38)return 0;
 void*playhead=F(core,0x490,void*);if(!playhead||F(playhead,0,uintptr_t)!=base+0x6d8a418)return 0;
 _Alignas(8) unsigned char position[88]={0};
 if(!FN(0x13ad070,int(*)(void*,void*))(playhead,position))return 0;
 double bpm=F(position,0,double);return isfinite(bpm)&&bpm>=20&&bpm<=999?bpm:0;
}
static uint64_t monotonic_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;}
static struct {
 int sounding[CHORD_NOTES*2],sounding_count;float velocity;
 unsigned step,seed;int64_t last_slot;uint64_t start_ms,off_ms;
 uint64_t anchor_serial;int64_t anchor_tick;uint64_t anchor_ms;int running;
} arp_run={.last_slot=-1,.seed=0x2545f491u};
static void arp_silence(void){
 for(int i=0;i<arp_run.sounding_count;++i)poly_record_arp_emit(arp_run.sounding[i],0,1.f);
 arp_run.sounding_count=0;
}
static int arp_tick(void){
 struct chord_config c;chords_get(&c);int notes[16];float velocity=1.f;
 /* Only while root pads are physically held: a lost key-up can never
  * leave the arpeggiator playing by itself. */
 int pattern=chords_arp_pattern(&c),rate=chords_arp_rate(&c);
 int n=c.enabled&&pattern!=ARP_OFF&&c.held_roots?poly_record_arp_notes(notes,16,&velocity):0;
 uint64_t now=monotonic_ms();
 if(!n){if(arp_run.sounding_count)arp_silence();arp_run.last_slot=-1;arp_run.step=0;arp_run.start_ms=0;return 0;}
 double bpm=arp_bpm();if(bpm<=0)bpm=120;
 unsigned ticks=chords_arp_rate_ticks[rate>=0&&rate<ARP_RATES?rate:3];
 double tick_ms=60000.0/(bpm*960.0);
 uint64_t serial;unsigned running;int64_t tick=step_native_clock_ticks(&serial,&running);
 int64_t slot;
 if(running){
  if(serial!=arp_run.anchor_serial||!arp_run.running){arp_run.anchor_serial=serial;arp_run.anchor_tick=tick;arp_run.anchor_ms=now;}
  double position=(double)arp_run.anchor_tick+(double)(now-arp_run.anchor_ms)/tick_ms;
  slot=(int64_t)floor(position/(double)ticks);
 }else{
  if(!arp_run.start_ms||arp_run.running)arp_run.start_ms=now,arp_run.last_slot=-1;
  slot=(int64_t)floor((double)(now-arp_run.start_ms)/(tick_ms*(double)ticks));
 }
 arp_run.running=(int)running;
 if(arp_run.sounding_count&&now>=arp_run.off_ms)arp_silence(); /* 50% gate */
 if(slot!=arp_run.last_slot){
  arp_run.last_slot=slot;arp_silence();
  int out[CHORD_NOTES*2],k=chords_arp_step(pattern,c.arp_range,notes,n,&arp_run.step,&arp_run.seed,out);
  for(int i=0;i<k;++i)if(poly_record_arp_emit(out[i],1,velocity))arp_run.sounding[arp_run.sounding_count++]=out[i];
  {static unsigned reports;if(reports<24){++reports;fprintf(stderr,"CHORDS ARP step=%u slot=%lld notes=%d first=%d pattern=%d rate=%d running=%u bpm=%.1f\n",arp_run.step,(long long)slot,k,k?out[0]:0,c.arp,c.arp_rate,running,bpm);}}
  arp_run.off_ms=now+(uint64_t)(tick_ms*(double)ticks*0.5);
 }
 return 1;
}
/* A private JUCE Timer (same native Timer as the naming keypad) at 4 ms,
 * only while notes are held with an arp pattern. */
struct native_timer {uintptr_t *vtable;int64_t id;int32_t interval;int32_t padding;};
static struct native_timer arp_timer;static uintptr_t arp_vtable[3];static int arp_timer_created,arp_timer_running;
static void arp_noop(void*p){(void)p;}
static void arp_timer_callback(void*p){(void)p;voice_guard_lock();int more=arp_tick();voice_guard_unlock();if(!more&&arp_timer_running){FN(0xff99b0,void(*)(void*,int))(&arp_timer,0);arp_timer_running=0;}}
static void arp_wake(void){
 if(!arp_timer_created){FN(0xff1a00,void(*)(void*))(&arp_timer);arp_vtable[0]=arp_vtable[1]=(uintptr_t)arp_noop;arp_vtable[2]=(uintptr_t)arp_timer_callback;arp_timer.vtable=arp_vtable;arp_timer_created=1;}
 if(!arp_timer_running){FN(0xff9370,void(*)(void*,int))(&arp_timer,4);arp_timer_running=1;}
}
void chords_ui_poll(int naming_or_steps){
#ifdef MPC_FAST_RESAMPLE
 /* Install the fault/abort backtrace once the app has set its own handlers
  * (first UI tick), so ours chains to them instead of being replaced. */
 static int fault_ready;if(!fault_ready){fault_ready=1;if(!resample_fault_install(base))fprintf(stderr,"FAULT_DIAGNOSTIC_UNAVAILABLE\n");}
#endif
 unsigned blocked=(naming_or_steps?1u:0u)|(!music_ui_chords_available()?2u:0u)|
                  (workflow_blocks_sample_controls()?4u:0u);
#ifdef MPC_RESAMPLE_TRIAL
 if(resample_ui_active()||resample_ui_shutdown_busy())blocked|=8u;
#endif
 static unsigned previous=~0u,count;
 if(blocked!=previous){previous=blocked;if(count++<48)fprintf(stderr,"CHORDS_ENTRY blocked=%u (input=1 screen=2 workflow=4 render=8)\n",blocked);}
 chords_ui_tick(!blocked);
}
void chords_ui_tick(int available){
 unsigned enabled,mode,origin,pad=~0u;
 music_lighting_levels_state(&enabled,&mode,&origin);
 available=available&&enabled&&mode==1&&note_native_pad_index(&pad);
 atomic_store(&ready,(unsigned)available);
 if(!available||(source!=~0u&&source!=pad))chords_close();
 source=available?pad:~0u;
 uint64_t v=music_published();void*t=note_native_context(),*instrument=t?F(t,0x668,void*):NULL;
 if(previous_instrument&&previous_instrument!=instrument)chords_close();
 previous_instrument=instrument;
 if(poly_pending&&instrument&&F(instrument,0x1690,int)==0)poly_pending=0;
 if(poly_pending){struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);
  if(now.tv_sec-poly_requested.tv_sec>=10)poly_pending=0;}
 chords_context((int)(v&15)-1,(int)((v>>20)&15),(int)((v>>24)&7),(int)((v>>12)&255)-96,
                available&&v&&(v&15)&&instrument&&F(instrument,0x1690,int)==0);
 struct chord_config config;chords_get(&config);if(!config.held_roots)release_warning=0;
 if((unsigned)config.enabled!=reported_active){reported_active=(unsigned)config.enabled;
  if(reports++<64)fprintf(stderr,"CHORDS active=%d source=%u orig=%d tonic=%d scale=%d semi=%d valid=%d\n",
      config.enabled,source,config.root,config.tonic,config.scale,config.semi,config.valid);}
 if(config.enabled&&chords_arp_pattern(&config)!=ARP_OFF){int held[16];if(poly_record_arp_notes(held,16,NULL))arp_wake();}
 else if(arp_run.sounding_count){voice_guard_lock();arp_silence();voice_guard_unlock();arp_run.last_slot=-1;}
 {static int shift_seen=-1;int s=shift_active();if(config.enabled&&s!=shift_seen){shift_seen=s;note_native_repaint_sample();}}
 if(chords_generation()!=painted){painted=chords_generation();note_native_repaint_sample();}
}
/* Input thread, before queueing: a mode/screen button leaves Chords at once,
 * so pads pressed right after it (Seq selects sequences) never reach the
 * chord path while the UI queue is still catching up. */
void chords_ui_button_input(unsigned code,int down){
 if(!down||code>=112||!chords_active())return;
 if(code==0x00||code==0x0b||code==0x0d||code==0x0e||code==0x6f||code==0x31||code==0x35||code==0x50||code==0x51||code==0x52||code==0x4b||code==0x23)return;
 /* Held chord keys still finish through the release path after close. */
 chords_close();
}
int chords_ui_button_owns(unsigned code){
 if(code==0x23)return 0; /* Native hold-bank picker remains available. */
 return code<112&&(atomic_load(&owned[code])||
   (chords_active()&&code!=0x50&&code!=0x51&&code!=0x52&&code!=0x35&&code!=0x00)||
   (code==0x4b&&atomic_load(&ready)&&!music_ui_shift_active()));
}
int chords_ui_button(void*event){
 unsigned code=F(event,8,unsigned char),down=F(event,9,unsigned char)!=0;
 if(code>=112)return 0;
 if(code==0x31)atomic_store(&shift_held,down?1:0);
 /* Note Repeat in Chords: while held, the held chord repeats as a whole at
  * the native Note Repeat rate (last seen; 1/16 until then). */
 if(code==0x0b&&chords_active()){
  if(down){int r=music_ui_repeat_rate();chords_set(CHORD_REPEAT_RATE,r>=0?r:3);chords_set(CHORD_REPEAT,1);atomic_store(&owned[code],1);}
  else{chords_set(CHORD_REPEAT,0);atomic_store(&owned[code],0);}
  note_native_repaint_sample();return 1;
 }
 if(code==0x0b&&!down)chords_set(CHORD_REPEAT,0);
 if(atomic_load(&owned[code])){if(!down)atomic_store(&owned[code],0);return 1;}
 if(!down)return 0;
 struct chord_config config;chords_get(&config);
 /* Back/Chop leave this overlay over the same native Tune input object.
  * Its physical key-up callback remains alive and finishes held notes. */
 if(chords_active()&&config.held_roots&&code!=0x0c&&code!=0x4b&&code!=0x51&&code!=0x52&&code!=0x50&&code!=0x35&&code!=0x0b&&code!=0x00&&code!=0x31&&code!=0x0d&&code!=0x0e&&code!=0x6f){
  release_warning=1;note_native_repaint_sample();atomic_store(&owned[code],1);return 1;
 }
 if(code==0x4b&&atomic_load(&ready)&&!music_ui_shift_active()){
  if(chords_active())chords_close();else{focus=release_warning=0;
   atomic_store(&shift_held,0); /* opened by CHOP without Shift */
   poly_pending=note_native_request_poly()==2;
   if(poly_pending)clock_gettime(CLOCK_MONOTONIC,&poly_requested);
   chords_open();}
  atomic_store(&owned[code],1);return 1;
 }
 if(!chords_active())return 0;
 if(code==0x0c)chords_close();
 else if(code==0x0d)chords_reset();
 else if(code==0x0e){struct chord_config c;chords_get(&c);chords_set(CHORD_LATCH,!c.latch);}
 else if(code==0x6f){focus=(focus+1)%3;note_native_repaint_sample();}
 else if(code==0x51||code==0x52||code==0x50||code==0x35||code==0x00)return 0; /* transport, Tap Tempo, Knob FX */
 else if(code==0x31){atomic_store(&owned[code],1);return 1;}
 else{chords_close();return 0;} /* Other screen/mode buttons retain stock behavior. */
 atomic_store(&owned[code],1);return 1;
}
/* Knob FX (LED lit) takes K1-K3 for its own overlay. */
extern unsigned phone_native_button_light(unsigned code);
static int knob_fx(void){return music_ui_knob_fx_active();}
int chords_ui_owns(unsigned cc){return chords_active()&&!knob_fx()&&(cc==0x10||cc==0x11||cc==0x12||cc==0x64);}
int chords_ui_control(void*event){
 unsigned cc=F(event,8,unsigned char),value=F(event,9,unsigned char);
 if(!chords_ui_owns(cc))return 0;
 struct chord_config c;chords_get(&c);int k=cc==0x64?focus:(int)cc-0x10,shift=shift_active();
 /* K1 Octave (Shift: Arp range), K2 Inversion, K3 Arp pattern (Shift: rate).
  * Latch stays on B3 only. */
 int field=k==0?(shift?CHORD_ARP_RANGE:CHORD_OCTAVE):k==1?CHORD_INVERSION:(shift?CHORD_ARP_RATE:CHORD_ARP);
 int current=field==CHORD_OCTAVE?c.octave:field==CHORD_INVERSION?c.inversion:field==CHORD_ARP?c.arp:field==CHORD_ARP_RATE?c.arp_rate:c.arp_range;
 int lo=field==CHORD_OCTAVE?-2:field==CHORD_INVERSION?-3:field==CHORD_ARP_RANGE?1:0;
 int hi=field==CHORD_OCTAVE?2:field==CHORD_INVERSION?3:field==CHORD_ARP?ARP_PATTERNS-1:field==CHORD_ARP_RATE?ARP_RATES-1:2;
 int next=cc==0x64?current+(value>64?(int)value-128:(int)value):lo+(int)((value*(unsigned)(hi-lo+1))/128);
 chords_set(field,next);return 1;
}
int chords_ui_reset_knob(unsigned column,unsigned shifted){
 if(!chords_active()||column>2||knob_fx())return 0;
 int field=column==0?(shifted?CHORD_ARP_RANGE:CHORD_OCTAVE):column==1?CHORD_INVERSION:(shifted?CHORD_ARP_RATE:CHORD_ARP);
 chords_set(field,field==CHORD_ARP_RANGE?1:0);focus=(int)column;note_native_repaint_sample();return 1;
}
static void colour(void*g,uint32_t c){FN(0x102d5e0,void(*)(void*,uint32_t))(g,c);}
static void rect(void*g,int x,int y,int w,int h){FN(0x102e000,void(*)(void*,int,int,int,int))(g,x,y,w,h);}
static void text(void*g,const char*s,int x,int y,int w,int h,float height){
 uint64_t string=0,font=0;FN(0xf535c0,void(*)(uint64_t*,const char*))(&string,s);
 FN(0x1082810,void(*)(uint64_t*,int,float))(&font,0,height);FN(0x102dca0,void(*)(void*,const uint64_t*))(g,&font);
 FN(0x10817b0,void(*)(void*,const uint64_t*,int,int,int,int,int,int,float))(g,&string,x,y,w,h,0x24,1,.7f);
 FN(0x1070d60,void(*)(uint64_t*))(&font);FN(0xf0a290,void(*)(uint64_t*))(&string);
}
static void shift_badge(void *g,int bx,int top,int on)
{
    colour(g,on ? 0xffff4747 : 0xffffffff);
    SHIFT_BADGE_RECTS(rect,g,bx,top);
}
int chords_ui_paint(void*root,void*g){
 if(!chords_active())return 0;
 struct chord_config c;chords_get(&c);char line[128],name[MUSIC_NAME_BYTES]={0};
 int fx=knob_fx(); /* Knob FX overlay keeps the footer */
 colour(g,0xff000000);rect(g,0,5,320,fx?193:235);
 /* Header: B1-B3 in blocks, like the other mod windows. */
 const char*heads[3]={"Back","Reset",c.latch?"Latch On":"Latch Off"};
 for(int i=0;i<3;++i){int x=i*107;colour(g,0xff202020);rect(g,x+1,6,105,26);
  colour(g,i==2&&c.latch?0xffffc247:0xffffffff);text(g,heads[i],x+1,7,105,24,15);}
 /* Info block: chord, its notes, then sample and key (or a warning). */
 int root_note=(c.tonic+chords_degree(c.scale,(unsigned)c.last_root))%12;
 snprintf(line,sizeof(line),"%s %s",music_note_names[root_note],chords_quality_name(c.types));
 {static const char*const added[4]={"6","7","maj7","9"};size_t at=strlen(line);
  for(int i=0;i<4;++i)if(c.extensions&(1u<<i)){int wrote=snprintf(line+at,sizeof(line)-at," +%s",added[i]);if(wrote<0||(size_t)wrote>=sizeof(line)-at)break;at+=(size_t)wrote;}}
 colour(g,0xffffffff);text(g,line,6,35,150,28,c.extensions?18:24);
 int notes[CHORD_NOTES],n=chords_make(&c,(unsigned)c.last_root,notes);size_t used=0;line[0]=0;
 for(int i=0;i<n;++i){int note=(c.root+c.semi+notes[i])%12;if(note<0)note+=12;
  int wrote=snprintf(line+used,sizeof(line)-used,"%s%s",i?" ":"",music_note_names[note]);if(wrote<0||(size_t)wrote>=sizeof(line)-used)break;used+=(size_t)wrote;}
 colour(g,0xff00e060);text(g,line,158,39,158,22,16);
 if(!c.valid||release_warning){
  void*t=note_native_context(),*instrument=t?F(t,0x668,void*):NULL;
  const char *warning=release_warning?"Release root pads before exit":
   !instrument?"Select a sample pad and retry":F(instrument,0x1690,int)!=0?
   (poly_pending?"Switching to Poly...":"Could not enable Poly"):"Set Orig Note: Tune + Shift K1";
  colour(g,0xffff4040);text(g,warning,6,64,308,15,12);
 }else{
  note_native_current(name,sizeof(name));colour(g,0xffffc247);text(g,name,6,64,200,15,12);
  snprintf(line,sizeof(line),"%s %s",music_note_names[c.tonic%12],music_scale_names[c.scale]);
  colour(g,0xff909090);text(g,line,206,64,108,15,12);
 }
 /* Pad grid, laid out like the pads: 13-16 types, 9-12 extensions,
  * 5-8 and 1-4 scale-degree roots. Selected blocks are filled. */
 static const char*const types[4]={"Maj","Min","Dim","Aug"},*const ext[4]={"6","7","maj7","9"};
 for(int row=0;row<4;++row)for(int col=0;col<4;++col){
  unsigned pad=(unsigned)((3-row)*4+col);
  int x=4+col*79,y=82+row*28,w=75,h=25;
  uint32_t family;int on;const char*label;char buf[16];
  if(pad>=12){family=0xffffffff;on=(c.types>>(pad-12))&1;label=types[pad-12];}
  else if(pad>=8){family=0xffffb830;on=(c.extensions>>(pad-8))&1;label=ext[pad-8];}
  else{family=0xff00e060;on=(c.held_roots>>pad)&1;
   int note=(c.tonic+chords_degree(c.scale,pad))%12;
   snprintf(buf,sizeof(buf),"%u  %s",pad+1,music_note_names[note]);label=buf;}
  if(on){colour(g,family);rect(g,x,y,w,h);colour(g,0xff000000);}
  else{colour(g,0xff1c1c1c);rect(g,x,y,w,h);colour(g,pad<8&&!c.valid?0xff505050:family);}
  text(g,label,x,y+1,w,h-2,pad<8?14:15);
  if(pad<8&&(int)pad==c.last_root&&!on){colour(g,0xff00e060);
   rect(g,x,y,w,2);rect(g,x,y+h-2,w,2);rect(g,x,y,2,h);rect(g,x+w-2,y,2,h);}
 }
 if(fx){(void)root;return 1;}
 int shift=shift_active();
 for(int i=0;i<3;++i){int x=i*106,w=i==2?108:106;colour(g,i==focus?0xffffc247:0xffffffff);
  const char*title=i==0?(shift?"Arp Range":"Octave"):i==1?"Inversion":(shift?"Arp Rate":"Arp");
  if(i==0&&shift)snprintf(line,sizeof(line),"x%d",c.arp_range);
  else if(i==2&&c.repeat)snprintf(line,sizeof(line),"Repeat %s",chords_arp_rate_names[c.repeat_rate]);
  else if(i==2)snprintf(line,sizeof(line),"%s",shift?chords_arp_rate_names[c.arp_rate]:chords_arp_names[c.arp]);
  else snprintf(line,sizeof(line),"%+d",i==0?c.octave:c.inversion);
  text(g,title,x+4,200,w-8,13,12);text(g,line,x+4,215,w-8,18,16);
  if(i!=1){shift_badge(g,i*106+94,200,shift);colour(g,i==focus?0xffffc247:0xffffffff);}
  if(i==focus){rect(g,x,198,w,2);rect(g,x,198,2,34);rect(g,x+w-2,198,2,34);}
  int count=i==0?(shift?2:5):i==1?7:(shift?ARP_RATES:ARP_PATTERNS);
  int selected=i==0?(shift?c.arp_range-1:c.octave+2):i==1?c.inversion+3:(shift?c.arp_rate:c.arp);
  for(int j=0;j<count;++j){colour(g,j==selected?0xffffc247:0xff404040);rect(g,x+4+j*(w-8)/count,236,(w-8)/count-2,2);}
 }
 (void)root;return 1;
}
int chords_ui_button_light(unsigned code,unsigned*value){if(!chords_active()||(code!=0x28&&code!=0x4b))return 0;*value=2;return 1;}
