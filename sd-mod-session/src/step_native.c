#define _GNU_SOURCE
#include "step_native.h"
#include "ui_caption.h"
#include "workflow_ui.h"
#include "music_settings.h"
#include <math.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include "pad_palette.h"
#include "sample_mute.h"
#ifdef MPC_LOOP_FEEDBACK
#include "sample_playing.h"
#endif

#define FN(off,type) ((type)(base+(off)))
#define FIELD(p,off,type) (*(type *)((char *)(p)+(off)))
#define SCREEN_VPTR 0x6e24f38
#define CONTROLLER_VPTR 0x6d8ec20
#define NOW_VPTR 0x6da4588
#define SCREEN_DTOR 0x16aea50
#define WHITE 0xffffffffu
#define AMBER 0xffffc247u
#define GREEN 0xff32c780u
#define RED 0xffff4040u
#define CYAN 0xff39c1d6u
#define VIOLET 0xffa45cffu
#define GREY 0xff777777u
#define BUTTON_MINUS 0x37
#define BUTTON_PLUS 0x36
#define BUTTON_PLAY 0x52
#define BUTTON_STOP 0x51
#define BUTTON_RECORD 0x50
#define BUTTON_SAMPLE_RECORD 0x49 /* Physical SAMPLE RECORD; Step Toggle only in Steps. */
#define BUTTON_ERASE 0x09 /* GlobalControls+0x23b0, native Erase ctor */
#define BUTTON_LEVELS 0x28 /* GlobalControls+0x2720; light +2778 */
#define BUTTON_CHOP 0x4b /* GlobalControls+2510 ->1541bc0 Slice8; light+2568 */
#define BUTTON_MUTE 0x2b /* GlobalControls+0x25c0 -> AC50 mode30f4 MutePadMode */
/* Ticket is one coherent atomic producer snapshot: no GUI pointers. */
#define T_VISIBLE (UINT64_C(1)<<63)
#define T_STEPS (UINT64_C(1)<<62)
#define T_SHIFT (UINT64_C(1)<<61)
#define T_MENU (UINT64_C(1)<<60)
#define T_SOUND_SELECT (UINT64_C(1)<<59)
#define T_ERASE (UINT64_C(1)<<58)
#define T_EPOCH_MASK UINT64_C(0xfffff)
#define T_PAGE_SHIFT 20
#define T_SOUND_SHIFT 36
#define T_GRID_SHIFT 43
#define T_BANK_SHIFT 47
#define T_PICKER (UINT64_C(1)<<50)
#define T_MUTE (UINT64_C(1)<<51)

_Static_assert(sizeof(struct step_event)==56,"Pinned tag-3 event size");
_Static_assert(offsetof(struct step_event,data)==16,"Native event payload");
extern void step_command_result(void *,void *,const void *,const void *,void *,uintptr_t);
extern void step_add_result(void *,void *,void *,void *,void *,uint64_t *,uint64_t *,int,void *,uintptr_t);
extern int step_try_call(void (*)(void*),void*);
extern int step_set_bars(uintptr_t,void*,void*,int);
extern void step_layer_sample(void *,unsigned,uintptr_t[2],uintptr_t);
extern void step_pad_string(int,uint64_t *,uintptr_t);
extern void step_table_cell_string(void *,int,const void *,uint64_t *,uintptr_t);
/* Exact empty native STL containers initialized at 1581a58..1581ac8.
 * Node allocation/copy/destruction is performed by the pinned firmware. */
struct native_tree {uint64_t padding,colour;void *parent,*left,*right;uint64_t size;};
_Static_assert(sizeof(struct native_tree)==48,"Native tree ABI");
static void tree_init(struct native_tree *t)
{memset(t,0,sizeof(*t));t->left=t->right=&t->colour;}
struct bbt { int bar,beat,tick; };
struct slot { unsigned count,muted,categories; struct step_event first; };
static uintptr_t base;
static void *root,*controller,*sequence,*event_list,*last_sequence,*last_program;
static pthread_t ui_thread;
static int ui_known,steps,want_steps,menu,menu_index,focus,sound,grid=2,velocity=100,follow=1;
static int show_other_sounds=1;
static int cursor_visible=1;
static unsigned other_sound_steps;
static int page,cursor,showing,shift,was_playing;
static int64_t length,play_tick,edit_tick;
static struct slot slots[16];
static int scan_ok,clip_valid,clip_sound;
static struct step_event clip;
static int64_t clip_offset;
static unsigned epoch=1,records;
static uint64_t last_visual;
static char sample_name[513]="Empty";
static char message[80];
static uint64_t message_until;
static _Atomic(uintptr_t) root_identity;
static _Atomic unsigned dead,project_reset,queue_fault,requested_shift,requested_sound,requested_erase,active;
static _Atomic uint64_t ticket,clock_tick;
static _Atomic unsigned clock_running,clock_recording;
static _Atomic(uintptr_t) bank_owner,bank_model;
static int last_native_bank=-1,browse_bank,sound_initialized;
static _Atomic unsigned write_armed,toggle_armed;
static _Atomic unsigned mute_active;
static unsigned mute_latched,mute_held,mute_used;
static uint64_t mute_down_ms;
static unsigned sound_present;
static unsigned picker_source_reports;
static unsigned playback_reports,playback_gate_last=~0u;
static _Atomic unsigned event_edit_mask;
_Static_assert(ATOMIC_LLONG_LOCK_FREE==2,"Audio snapshot requires lock-free64-bit atomics");
static _Atomic uint64_t preview_gate_serial,preview_gate_words[7];
static _Atomic(uintptr_t) preview_gate_track;
static _Atomic unsigned preview_gate_enabled;
static void preview_gate_set(void*track,const struct step_event*event){
 if(!event&&!atomic_load(&preview_gate_enabled))return;
 atomic_fetch_add_explicit(&preview_gate_serial,1,memory_order_acq_rel);
 atomic_store(&preview_gate_enabled,0);
 if(track&&event){
  uint64_t words[7];memcpy(words,event,56);
  for(unsigned i=0;i<7;++i)atomic_store_explicit(&preview_gate_words[i],words[i],memory_order_relaxed);
  atomic_store_explicit(&preview_gate_track,(uintptr_t)track,memory_order_relaxed);
  atomic_store(&preview_gate_enabled,1);
 }
 atomic_fetch_add_explicit(&preview_gate_serial,1,memory_order_release);
}
int step_native_preview_suppressed(void*track,const void*event,unsigned mask){
 /* Only sequence scheduling; the ordinary Sample input uses7ffffffa and
  * must still audition the original pitch. No models, locks or I/O here. */
 if(mask!=0x7ffffffe||!event)return 0;
 uint32_t type=FIELD(event,8,uint32_t);if((type&255)!=3||(type&0x100))return 0;
 uint64_t words[7];memcpy(words,event,56);
 for(unsigned attempt=0;attempt<2;++attempt){
  uint64_t seq=atomic_load_explicit(&preview_gate_serial,memory_order_acquire);
  if((seq&1)||!atomic_load(&preview_gate_enabled))return 0;
  int equal=(uintptr_t)track==atomic_load_explicit(&preview_gate_track,memory_order_relaxed);
  for(unsigned i=0;i<7;++i){
   /* Native variant alignment/payload padding is not event identity. */
   uint64_t valid=i==1?UINT64_C(0xffffffff):i==6?UINT64_C(0xffffffffff):UINT64_MAX;
   equal&=((words[i]^atomic_load_explicit(&preview_gate_words[i],memory_order_relaxed))&valid)==0;
  }
  if(seq==atomic_load_explicit(&preview_gate_serial,memory_order_acquire))return equal;
 }
 return 0;
}
static _Atomic(uintptr_t) native_audition;
static _Alignas(16) unsigned char local_audition[0x30];
static int picker_audition_active,picker_preview_wait;static unsigned picker_audition_pad;
static void picker_probe(const char*,unsigned);
void step_native_audition_created(void*p){atomic_store(&native_audition,(uintptr_t)p);fprintf(stderr,"PAD_STEPS AUDITION_CAPTURED\n");}
void step_native_audition_bind(void*queues,void*things,void*receiver){
 /* Full MPC constructs EventAudition at139fe8c; the Sample application has
  * its own service graph and need not construct that optional service.
  * Retain one session-lifetime instance using the same native constructor
  * and existing queue/current-track/sequencer dependencies. Never put an
  * object referenced by queued native closures on the stack. */
 if(atomic_load(&native_audition)||!queues||!things||!receiver)return;
 FN(0x18b04f0,void(*)(void*,void*,void*,void*))(local_audition,queues,things,receiver);
 atomic_store(&native_audition,(uintptr_t)local_audition);
 fprintf(stderr,"PAD_STEPS AUDITION_BOUND_SAMPLE_QUEUE\n");
}
int step_native_event_preview_enabled(void){return 1;}
static void stop_picker_audition(void){
    if(picker_audition_active){picker_preview_wait=1;picker_probe("AUDIO_WAIT",picker_audition_pad);workflow_event_preview_stop(1);}
    picker_audition_active=0;
}
static _Atomic unsigned events_erase_forwarded;
static int picker_kind,picker_mode,picker_base,picker_value;
static int picker_chop,picker_slice_count=128,picker_focus=1,picker_has_choice,picker_insert;
#define PICKER_SOURCES_MAX 64
static struct step_event picker_sources[PICKER_SOURCES_MAX];
static unsigned picker_source_count,picker_source_index;
static int picker_source_kind;
static uintptr_t levels_processor;
static _Atomic unsigned native_levels_enabled,native_levels_mode,native_levels_origin;
static _Atomic unsigned native_mute;
static _Atomic unsigned playback_mask;
/* Published by the UI's native event scan; RGB receivers read only this
 * numeric snapshot, never the mutable program or playback mode. */
static _Atomic int playback_colour_bank=-1;
static uint64_t playback_until[16],playback_muted_until[16];
static _Atomic unsigned playback_muted_mask,sample_palette_enabled;
static _Atomic uint64_t sample_palette[16];
static _Atomic unsigned sample_native_rgb[16];
static void *sample_focus_program;
static int sample_focus_index=-1,sample_focus_bank=-1;
static uint64_t sample_focus_since;
void step_native_observe_rgb(unsigned pad,unsigned rgb){if(pad<16)atomic_store(&sample_native_rgb[pad],rgb);}
static void sample_palette_publish(void *things,void *program,int bank,int enabled,int mute_mode,uint64_t now){
 if(!enabled||bank<0||bank>=8){atomic_store(&sample_palette_enabled,0);sample_focus_program=NULL;sample_focus_index=-1;return;}
 /* SelectPad's absolute index exists even for an empty slot. The cached
  * Instrument at+60 can still point to the previously selected loaded pad. */
 unsigned selected=things?__atomic_load_n((unsigned*)((char*)things+0x5f8),__ATOMIC_ACQUIRE):128;
 int focus=selected<128?(int)selected:-1;
 if(program!=sample_focus_program||focus!=sample_focus_index||bank!=sample_focus_bank||now<sample_focus_since){
  sample_focus_program=program;sample_focus_index=focus;sample_focus_bank=bank;sample_focus_since=now;
 }
 for(unsigned i=0;i<16;++i){
  unsigned n=(unsigned)bank*16+i;void*p=program?FN(0x286bbe0,void*(*)(void*,int))(program,(int)n):NULL;
  int used=p&&FN(0x2804760,int(*)(void*))(p);
  int muted=used&&sample_instrument_muted(p);
  if(mute_mode){
   uint64_t light=used?pad_light(muted?PAD_DANGER:PAD_FREE,now<playback_until[i]?1.f:.75f):0;
   atomic_store(&sample_palette[i],light);
   continue;
  }
  uint32_t rgb=muted?PAD_MUTED:used?music_identity_rgb((unsigned)music_pad_colour(n)):0;
  float strength=used?(muted?.32f:PAD_BRIGHT_CONTENT):0;
  unsigned native=atomic_load(&sample_native_rgb[i]),top=(native>>16)&255;
  if(((native>>8)&255)>top)top=(native>>8)&255;
  if((native&255)>top)top=native&255;
  if(used&&!muted&&top>=192)strength=.90f; /* real held/Loop playback remains visible */
  if(used&&now<playback_until[i]){rgb=now<playback_muted_until[i]||muted?PAD_MUTED:rgb;strength=rgb==PAD_MUTED?.65f:.90f;}
#ifdef MPC_LOOP_FEEDBACK
  int sustained=used&&!(rgb==PAD_MUTED&&now<playback_until[i])&&sample_playing_active(program,n);
  if(sustained){
   /* A playing unselected pad must visibly turn OFF, not merely blend back
    * into the .45 content brightness shared by all loaded pads. */
   if(now%800<400)strength=muted?.65f:.95f;
   else{rgb=0;strength=0;}
  }
#endif
  uint64_t light=pad_light(rgb,strength);
  if((int)n==focus&&!(rgb==PAD_MUTED&&now<playback_until[i]))
#ifdef MPC_LOOP_FEEDBACK
   light=sustained?(music_cursor_on(now-sample_focus_since,500)?pad_light(PAD_SELECT,.95f):
       pad_light(muted?PAD_MUTED:music_identity_rgb((unsigned)music_pad_colour(n)),muted?.65f:.95f)):
       pad_focus_light(rgb,strength,now-sample_focus_since);
#else
   light=pad_focus_light(rgb,strength,now-sample_focus_since);
#endif
  atomic_store(&sample_palette[i],light);
 }
 atomic_store(&sample_palette_enabled,1);
}
/* Tune pitches (-120..120) heard from the sequence, for Chords feedback. */
static uint64_t playback_pitch_until[241];
static int64_t playback_previous=-1;
static void *playback_list,*playback_program;
static int playback_sound=-1,playback_mode=-1,playback_first;
static struct step_event detail_old,detail_edit;
static struct step_event picker_notes[16];
static unsigned picker_note_count;
static int picker_toggle_notes;
static int apply_picker_notes(void);
static int add_event(const struct step_event *);
static _Atomic unsigned clock_valid,clock_serial;
static unsigned last_clock_serial;
static unsigned clock_reports;
static void read_sample_name(char out[513]);
static int sample_tuning;
static int event_slices(const struct step_event *);
static int collect_picker_sources(int);
static int scan_page(int,int,int,struct slot[16]);
static int64_t last_reported_clock=-1;
static _Atomic uint64_t lights[16];
static _Atomic unsigned owned_buttons[112],held_buttons;
static const int grid_ticks[10]={960,480,240,120,60,640,320,160,80,40};
static const char *grid_names[10]={"1/4","1/8","1/16","1/32","1/64","1/4T","1/8T","1/16T","1/32T","1/64T"};
static const char *level_names[10]={"Velocity","Tune","Filter","Layer","Attack","Decay","Probability","Ratchet","Slice","Articulation"};
static const int level_params[10]={-1,0,2,14,7,8,-1,-1,15,-1};

static void report(const char *s)
{ if (records++<128) fprintf(stderr,"PAD_STEPS %s\n",s); }
static uint64_t milliseconds(void)
{ struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000; }
static unsigned picker_probe_serial;
#ifndef PICKER_PROBE_ROOT
#define PICKER_PROBE_ROOT "/run/mpc-trial-app"
#endif
static void picker_probe(const char *phase,unsigned pad){
 /* RAM only. The existing supervisor exports this small trace to the SD
  * report, outside the GUI thread. No audio or queue calls in this probe. */
 char path[512],line[160];int n=snprintf(line,sizeof(line),"%s %u mode=%d pad=%u time=%llu\n",phase,picker_probe_serial,picker_mode,pad,(unsigned long long)milliseconds());
 snprintf(path,sizeof(path),"%s/picker-phase.txt",PICKER_PROBE_ROOT);
 int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC|O_NOFOLLOW,0600);
 if(fd>=0){if(write(fd,line,(size_t)n)!=n)fprintf(stderr,"PICKER_PHASE_WRITE_FAILED\n");close(fd);}
 if(picker_probe_serial<=64){snprintf(path,sizeof(path),"%s/picker-trace.txt",PICKER_PROBE_ROOT);fd=open(path,O_WRONLY|O_CREAT|O_APPEND|O_CLOEXEC|O_NOFOLLOW,0600);if(fd>=0){if(write(fd,line,(size_t)n)!=n)fprintf(stderr,"PICKER_TRACE_WRITE_FAILED\n");close(fd);}}
 fprintf(stderr,"PAD_STEPS %s",line);
}
static int on_ui(void) { return ui_known && pthread_equal(ui_thread,pthread_self()); }
static int clamp(int n,int lo,int hi) { return n<lo ? lo : n>hi ? hi : n; }
static void reset_mute(void)
{mute_latched=mute_held=mute_used=0;atomic_store(&mute_active,0);}
static int64_t clamp_tick(int64_t n) { return n<0 ? 0 : n>=length ? length-1 : n; }
static int count(void) { return length>0 ? (int)((length+grid_ticks[grid]-1)/grid_ticks[grid]) : 0; }
static int pages(void) { return (count()+15)/16; }
/* Physical pad numbers count bottom-up; musical steps read top-down. This
 * involution is used only at the physical pad boundary, never in note time. */
static int pad_step(unsigned pad) { return (3-(int)(pad/4))*4+(int)(pad%4); }
static void invalidate(void)
{ epoch=(epoch+1)&T_EPOCH_MASK; if (!epoch) epoch=1; atomic_store(&ticket,0); }
static void repaint(void)
{ if (root && on_ui()) FN(0x1149880,void(*)(void*,uint64_t,uint64_t))(root,0,FIELD(root,0x28,uint64_t)); }
static void say(const char *s) { snprintf(message,sizeof(message),"%s",s);message_until=milliseconds()+1800;report(s);repaint(); }
static void publish(void)
{
    uint64_t t=0;
    if (showing) t=T_VISIBLE|(steps ? T_STEPS : 0)|(menu ? T_MENU : 0)|(menu==3 ? T_PICKER : 0)
        |(atomic_load(&requested_shift) ? T_SHIFT : 0)|epoch
        |((steps && !menu && atomic_load(&requested_sound)) ? T_SOUND_SELECT : 0)
        |(atomic_load(&requested_erase) ? T_ERASE : 0)
        |(steps && !menu && atomic_load(&mute_active) ? T_MUTE : 0)
        |((uint64_t)(unsigned)page<<T_PAGE_SHIFT)|((uint64_t)(unsigned)sound<<T_SOUND_SHIFT)
        |((uint64_t)(unsigned)grid<<T_GRID_SHIFT)|((uint64_t)(unsigned)browse_bank<<T_BANK_SHIFT);
    atomic_store(&ticket,t); atomic_store(&active,(unsigned)showing);
}
static int current(uint64_t t)
{ return showing && (t&T_VISIBLE) && (t&T_EPOCH_MASK)==epoch; }
void step_native_init(uintptr_t b) { base=b; }
void step_native_levels_processor(uintptr_t p) {levels_processor=p;}
void step_native_levels_state(unsigned enabled,unsigned mode,unsigned origin)
{
    if(mode>9 || origin>15){enabled=0;mode=0;origin=0;}
    atomic_store(&native_levels_enabled,!!enabled);atomic_store(&native_levels_mode,mode);atomic_store(&native_levels_origin,origin);
}
int step_native_sample_chop(void){return atomic_load(&native_levels_enabled)&&atomic_load(&native_levels_mode)==8;}
void step_native_native_mute(unsigned enabled){atomic_store(&native_mute,!!enabled);}
int step_native_active(void) { return atomic_load(&active) || atomic_load(&held_buttons) || atomic_load(&events_erase_forwarded); }
int step_native_custom(void) { return !!(atomic_load(&ticket)&T_STEPS); }
int step_native_sound_layout(void)
{uint64_t t=atomic_load(&ticket);return !!(t&T_VISIBLE)&&!(t&(T_STEPS|T_MENU));}
int step_native_record_light(unsigned *value)
{
    if (!(atomic_load(&ticket)&T_STEPS)) return 0;
    *value=atomic_load(&write_armed) ? 1u : 0u;return 1;
}
int step_native_button_light(unsigned code,unsigned *value)
{
    if (code==BUTTON_RECORD) return step_native_record_light(value);
    if (code==BUTTON_SAMPLE_RECORD && (atomic_load(&ticket)&T_STEPS)) {
        *value=atomic_load(&toggle_armed)?1u:0u;return 1;
    }
    uint64_t t=atomic_load(&ticket);
    if(code==BUTTON_MUTE && (t&T_STEPS)) {*value=atomic_load(&mute_active)?2u:0u;return 1;}
    if (!(t&T_VISIBLE) || (t&T_SHIFT)) return 0;
    if (code!=BUTTON_LEVELS && code!=BUTTON_CHOP) return 0;
    *value=(atomic_load(&event_edit_mask)&(code==BUTTON_LEVELS ? 1u : 2u)) ? 2u : 0u;
    return 1;
}
int step_native_pad_route(uint64_t t)
{ return t&T_PICKER ? 1 : !(t&T_STEPS) ? ((t&T_VISIBLE) && !(t&T_MENU) && (t&T_ERASE) ? 3 : 0) : (t&T_SHIFT) ? 2 : 1; }
uint64_t step_native_ticket(void) { return atomic_load(&ticket); }
void step_native_requested(unsigned code,unsigned down)
{
    if(code==BUTTON_MUTE){
        uint64_t t=atomic_load(&ticket);
        if(down && (t&T_STEPS) && !(t&(T_SHIFT|T_MENU)))atomic_fetch_or(&ticket,T_MUTE);
        return;
    }
    if (code==BUTTON_ERASE) {
        atomic_store(&requested_erase,!!down);
        if (down && (atomic_load(&ticket)&T_VISIBLE)) atomic_fetch_or(&ticket,T_ERASE);
        else atomic_fetch_and(&ticket,~T_ERASE);
        return;
    }
    if (code==0x0c) {
        atomic_store(&requested_sound,!!down);
        if (down && (atomic_load(&ticket)&T_STEPS) && !(atomic_load(&ticket)&T_MENU)) atomic_fetch_or(&ticket,T_SOUND_SELECT);
        else atomic_fetch_and(&ticket,~T_SOUND_SELECT);
        return;
    }
    if (code!=0x31) return;
    atomic_store(&requested_shift,!!down);
    if (down) atomic_fetch_or(&ticket,T_SHIFT);
    else atomic_fetch_and(&ticket,~T_SHIFT);
}
int step_native_owns(unsigned cc)
{
    uint64_t t=atomic_load(&ticket);
    if (!(t&T_VISIBLE)) return 0;
    if ((t&T_STEPS)||(t&T_MENU)) return cc==0x10 || cc==0x11 || cc==0x12 || cc==0x64 || cc==0x14;
    return cc==0x64 || (cc==0x12 && (t&T_SHIFT));
}
int step_native_button_owns(unsigned code)
{
    if (code==BUTTON_ERASE && atomic_load(&events_erase_forwarded)) return 1;
    if (code<112 && atomic_load(&owned_buttons[code])) return 1;
    uint64_t t=atomic_load(&ticket);
    if (!(t&T_VISIBLE)) return 0;
    /* In either event view these buttons are contextual. An unavailable
     * action must not fall through and switch the native performance mode. */
    if ((code==BUTTON_LEVELS || code==BUTTON_CHOP) && !(t&T_SHIFT)) return 1;
    if ((t&T_PICKER) && code==BUTTON_RECORD) return 1;
    if (code==0x0d || ((t&T_STEPS) && code==BUTTON_RECORD)) return 1;
    if ((t&T_STEPS) && code==BUTTON_SAMPLE_RECORD && !(t&T_SHIFT)) return 1;
    if(code==BUTTON_MUTE && (t&T_STEPS) && !(t&T_MENU))return 1;
    if (code==0x0e) return 1;
    if (t&T_MENU) return code==BUTTON_ERASE || code==0x0c || code==0x6f || code==BUTTON_MINUS || code==BUTTON_PLUS;
    if (t&T_STEPS) return code==BUTTON_ERASE || code==0x0c || code==0x6f || (!(t&T_SHIFT) && (code==BUTTON_MINUS || code==BUTTON_PLUS));
    return 0;
}
void step_native_destroyed(void *p)
{
    if ((uintptr_t)p==atomic_load(&root_identity)) {
        atomic_store(&dead,1); atomic_store(&ticket,0); atomic_store(&active,0);
    }
}
void step_native_project_changed(void)
{ atomic_store(&project_reset,1); atomic_store(&ticket,0); }
void step_native_queue_fault(void)
{ atomic_store(&queue_fault,1); atomic_store(&ticket,0); }
unsigned step_native_transport_state(void)
{return atomic_load(&clock_running)|(atomic_load(&clock_valid)?2u:0u)|(atomic_load(&clock_recording)?4u:0u);}
void step_native_clock(void *p)
{
    /* Use the exact transport-position getter already called by this native
     * Timer (2109bc8), not the +238 property updated later by a queued task.
     * The engine pointer is used only while its owning Timer callback lives. */
    if (FIELD(p,0,uintptr_t)!=base+NOW_VPTR) return;
    void *engine=FIELD(p,0x258,void*);
    if (!engine) return;
    int64_t t=FN(0x2113b10,int64_t(*)(void*))(engine);
    void *transport=FIELD(engine,0x768,void*);
    /* 2133870 is record-mode == None, not transport stopped. The native
     * combined predicate at2133800 dispatches this separate isPlaying()
     * interface before it examines record mode. Query only that interface. */
    void *playing=transport ? FIELD(transport,0x148,void*) : NULL;
    uintptr_t vtable=playing ? FIELD(playing,0,uintptr_t) : 0;
    uintptr_t method=vtable ? FIELD((void*)vtable,0x18,uintptr_t) : 0;
    unsigned running=method && (((unsigned(*)(void*))method)(playing)&1u);
    atomic_store(&clock_running,running);
    /* 2133870: record mode == None. */
    /* Armed counts too: Rec+Play waiting for the start already gives the
     * pads to the Sample layout. */
    atomic_store(&clock_recording,transport&&!(FN(0x2133870,int(*)(void*))(transport)&1));
    atomic_store(&clock_tick,(uint64_t)t); atomic_store(&clock_valid,method!=0);
    atomic_fetch_add(&clock_serial,1);
    if (step_native_custom() && clock_reports<8 && t!=last_reported_clock) {
        last_reported_clock=t;++clock_reports;
        fprintf(stderr,"PAD_STEPS CLOCK engine_ticks=%lld queued_property_ticks=%lld running=%u\n",
                (long long)t,(long long)FIELD(p,0x238,int64_t),running);
    }
}
void step_native_bank_created(void *owner)
{
    if (FIELD(owner,0,uintptr_t)!=base+0x6d8c6f0) return;
    void *model=FIELD(owner,0x140,void*);
    atomic_store(&bank_owner,(uintptr_t)owner);atomic_store(&bank_model,(uintptr_t)model);
}
void step_native_bank_destroyed(void *owner)
{
    if (atomic_load(&bank_owner)==(uintptr_t)owner) {
        atomic_store(&bank_model,0);atomic_store(&bank_owner,0);
    }
}
static int native_bank(void)
{
    void *model=(void*)atomic_load(&bank_model);
    if (!model) return -1;
    int n=FIELD(model,0x50,int),selected=FIELD(model,0x158,int);
    return n>=1 && n<=8 && selected>=0 && selected<n ? selected : -1;
}
static void set_sound_bank(int bank)
{
    void *model=(void*)atomic_load(&bank_model);
    if (native_bank()>=0 && bank<FIELD(model,0x50,int))
        FN(0x1a36330,void(*)(void*,int))(model,bank);
    browse_bank=bank;
}
int step_native_browser_bank(int bank)
{
    if(bank>=0&&bank<8)set_sound_bank(bank);
    return native_bank();
}

static void *time_signatures(void)
{
    if (!controller) return NULL;
    void *ui=FIELD(controller,0x250,void*);
    void *list=ui ? FIELD(ui,0x578,void*) : NULL;
    if (!list || FIELD(list,0x30,void*)==(char*)list+0x30) return NULL;
    return list;
}
static struct bbt position(int64_t ticks)
{
    void *list=time_signatures();
    if (list) return FN(0x29bf230,struct bbt(*)(void*,int64_t))(list,ticks);
    struct bbt p={(int)(ticks/3840),(int)(ticks%3840/960),(int)(ticks%960)};
    return p;
}
static void pad_name(int n,char out[8])
{ snprintf(out,8,"%c%02d",'A'+n/16,n%16+1); }
static int event_pad(const struct step_event *e)
{
    if ((e->type&255)!=3) return -1;
    unsigned note=e->data[0x20];
    return note<128 ? FN(0x157f7d0,int(*)(void*,int))(controller,(int)note) : -1;
}
static int musical(const struct step_event *e)
{ return (e->type&255)==3 && isfinite(FIELD(e,0x10,float)) && FIELD(e,0x10,float)>=0 && FIELD(e,0x10,float)<=1.01f; }
/* Native IsMuted export2ac09bc and Mute Selected Events29e2020 use
 * tag-word bit9. XPJ note conversion maps it to the persisted muted flag. */
static int event_muted(const struct step_event *e) {return !!(e->type&0x200);}
static int selected_event(struct step_event *out)
{
    if (!controller || !event_list) return 0;
    uintptr_t begin=FIELD(controller,0x280,uintptr_t),end=FIELD(controller,0x288,uintptr_t);
    int index=FIELD(controller,0x124,int);
    if (index<0 || !begin || end<begin || (end-begin)%56 || end-begin>56*65536u
        || (unsigned)index>=(end-begin)/56) return 0;
    memcpy(out,(void*)(begin+(size_t)index*56),56); return musical(out);
}
static int choose_native(int64_t time,int wanted_sound)
{
    FN(0x15814a0,void(*)(void*,int64_t))(controller,time);
    uintptr_t begin=FIELD(controller,0x280,uintptr_t),end=FIELD(controller,0x288,uintptr_t);
    if (!begin || end<begin || (end-begin)%56 || end-begin>56*65536u) return 0;
    int index=-1;
    for (uintptr_t p=begin;p<end;p+=56) {
        struct step_event e; memcpy(&e,(void*)p,56);
        if (e.time==time && event_pad(&e)==wanted_sound) { index=(int)((p-begin)/56); break; }
    }
    if (index>=0) {
        /* Native Value setter, not an assignment to the event selection. */
        void *v=(char*)controller+0x128; int value=index;
        if (!FIELD(v,0x10,void*)) return 0;
        int (*normalize)(void*,const int*)=(int(*)(void*,const int*))FIELD(v,0x18,uintptr_t);
        int normalized=normalize(v,&value);
        FN(0x14e19c0,void(*)(void*,int))((char*)controller+0x108,normalized);
    }
    return index>=0;
}
static int choose_exact(const struct step_event *wanted)
{
    int64_t unit=FIELD(controller,0x78,int64_t);
    int64_t centre=unit>0 ? ((wanted->time+unit/2)/unit)*unit : wanted->time;
    FN(0x15814a0,void(*)(void*,int64_t))(controller,centre);
    uintptr_t begin=FIELD(controller,0x280,uintptr_t),end=FIELD(controller,0x288,uintptr_t);
    if (!begin || end<begin || (end-begin)%56 || end-begin>56*65536u) return 0;
    int index=-1;
    for(uintptr_t p=begin;p<end;p+=56) {
        if (FN(0x19722e0,int(*)(const void*,const void*))((void*)p,wanted)&255)
            index=(int)((p-begin)/56);
    }
    if(index<0) return 0;
    void *v=(char*)controller+0x128;
    if(!FIELD(v,0x10,void*)) return 0;
    int (*normalize)(void*,const int*)=(int(*)(void*,const int*))FIELD(v,0x18,uintptr_t);
    int value=normalize(v,&index);
    FN(0x14e19c0,void(*)(void*,int))((char*)controller+0x108,value);
    return 1;
}
static void release_command(uintptr_t result[2])
{ if (result[1]) FN(0x123bca0,void(*)(void*))((void*)result[1]); }
static void change(const struct step_event *old,const struct step_event *replacement)
{
    uintptr_t result[2]={0};
    step_command_result(FIELD(controller,0x240,void*),event_list,old,replacement,result,base+0x194fbb0);
    release_command(result); report("CHANGE_EVENT native_Undo");
}
static void remove_event(const struct step_event *old)
{
    uintptr_t result[2]={0};
    step_command_result(FIELD(controller,0x240,void*),event_list,old,NULL,result,base+0x194fa30);
    release_command(result); report("DELETE_EVENT native_Undo");
}
/* Tune and Slice are packed note modifiers 0 and 15. +33 is articulation,
 * not Slice. Use the native packed-value getters/setters and active masks. */
static int event_choice_kind(const struct step_event *e)
{
    unsigned mask=FIELD(e,0x14,uint16_t);
    if ((mask&0x8000) && (e->data[0x21]==15 || !(mask&1))) return 15;
    return mask&1 ? 0 : -1;
}
static int event_level_mode(const struct step_event *e)
{
    unsigned mask=FIELD(e,0x14,uint16_t),last=e->data[0x21];
    if(event_choice_kind(e)==15 && last==15) return 8;
    for(int m=1;m<9;++m) if(level_params[m]>=0 && (unsigned)level_params[m]==last && (mask&(1u<<last))) return m;
    for(int m=1;m<9;++m) if(level_params[m]>=0 && (mask&(1u<<level_params[m]))) return m;
    if(e->data[0x22]!=100) return 6;
    if(e->data[0x24]&7) return 7;
    if(e->data[0x23]<128) return 9;
    unsigned mode=atomic_load(&native_levels_mode);
    return atomic_load(&native_levels_enabled) && (mode==6 || mode==7 || mode==9) ? (int)mode : 0;
}
/* The stock event has modifier values, not a general original-input-mode tag.
 * Show only persisted evidence. Plain Velocity cannot prove 16 Levels use. */
static unsigned event_categories(const struct step_event *e)
{
    if(!musical(e))return 0;
    unsigned mask=FIELD(e,0x14,uint16_t),flags=(mask&0x8000)?1u:0u;
    const unsigned levels_mask=(1u<<0)|(1u<<2)|(1u<<7)|(1u<<8)|(1u<<14);
    if((mask&levels_mask)||e->data[0x22]!=100||(e->data[0x24]&7)||e->data[0x23]<128)flags|=2;
    return flags;
}
static unsigned category_buttons(unsigned flags)
{return ((flags&1u)?2u:0u)|((flags&2u)?1u:0u);}
void step_native_event_label(int pad,const struct step_event *e,uint64_t *out)
{
    /* Dedicated callsite bridge, not a global pad-name formatter hook.
     * The native list still owns scrolling, row selection and other columns. */
    if(!out)return;
    if(!e||!musical(e)||pad<0||pad>=128){step_pad_string(pad,out,base+0x2d444e0);return;}
    char label[16];
    unsigned flags=event_categories(e);
    static const char *suffix[]={""," C"," L"," CL"};
    snprintf(label,sizeof(label),"%c%02d%s",'A'+pad/16,pad%16+1,suffix[flags]);
    FN(0xf535c0,void(*)(uint64_t*,const char*))(out,label);
}
static int event_choice_value(const struct step_event *e,int kind)
{ return FN(0x27e4230,int(*)(const void*,int,int))((const char*)e+0x14,kind,0); }
static void event_choice_set(struct step_event *e,int kind,int value)
{ FN(0x27e4400,void(*)(void*,int,int,int))((char*)e+0x14,kind,0,value); }
int step_native_chop_audition(void*program,unsigned source,unsigned slice,int down){
 void*p=(void*)atomic_load(&native_audition);if(!p)return 0;
 if(!down){FN(0x18b0c00,void(*)(void*))(p);return 1;}
 if(!program||source>=128||slice>=128)return 0;
 int note=FIELD(program,0x16a8+(source+0x84)*4,int);if(note<0||note>127)return 0;
 struct step_event e=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))((unsigned)note,0,960,100,0,1.f);
 event_choice_set(&e,15,(int)slice+1);
 FN(0x18b0510,void(*)(void*,const struct step_event*))(p,&e);
 return 1;
}
static double level_value(const struct step_event *e,int mode)
{
    if(level_params[mode]>=0) return event_choice_value(e,level_params[mode]);
    if(mode==0) return FIELD(e,0x10,float)*127.0;
    if(mode==6) return e->data[0x22];
    if(mode==7) return (e->data[0x24]&7)+1;
    return e->data[0x23];
}
static void level_text(const struct step_event *e,int mode,char out[32])
{
    double v=level_value(e,mode);
    if(mode==1)snprintf(out,32,"%+.0f st",v);
    else snprintf(out,32,"%.0f",v);
}
static void level_candidate(const struct step_event *old,struct step_event *next,int mode,int pad,int first)
{
    _Alignas(8) unsigned char props[0x230]={0};
    FIELD(props,0x88,int)=mode;FIELD(props,0x228,int)=3;
    *next=*old;
    ((void(*)(void*,int,void*))(levels_processor ? levels_processor : base+0x21472f0))(props,pad,next);
    if(mode==1) event_choice_set(next,0,first+pad);
    if(mode==8) event_choice_set(next,15,first+pad);
}
static int level_first(const struct step_event *e,int mode)
{
    int value=(int)lround(level_value(e,mode));
    return mode==8 ? clamp((value-1)/16*16+1,1,113)
        : mode==1 ? clamp(((value+123)/12)*12-123,-120,105) : 0;
}
static int level_nearest(const struct step_event *e,int mode,int first)
{
    double wanted=level_value(e,mode),distance=1e10;int nearest=-1;
    for(int i=0;i<16;++i) {
        struct step_event candidate;level_candidate(e,&candidate,mode,i,first);
        double d=fabs(level_value(&candidate,mode)-wanted);
        if(d<distance) {nearest=i;distance=d;}
    }
    return nearest;
}
void step_native_playback_context(void *things,int transpose,int sample_visible)
{
    /* Called only from phone_native's UI Timer. Sample playback must not
     * depend on opening Step Edit first to discover the UI thread. */
    if(!ui_known){ui_thread=pthread_self();ui_known=1;}
    if (!on_ui()) return;
    uint64_t now=milliseconds();unsigned mask=0;
    int picker=showing && menu==3;
    unsigned running=atomic_load(&clock_running);
    if (!things && root && FIELD(root,0,uintptr_t)==base+SCREEN_VPTR && controller)
        things=FIELD(controller,0x248,void*);
    /* Mode10 is the outer Sample pad bank: any event flashes its source.
     * Inside an enabled native mode, decode the corresponding value instead. */
    int seq_mute=sample_visible==2&&atomic_load(&native_mute);
    int sample_mute=sample_visible==1&&atomic_load(&native_mute);
    int mode=picker ? picker_mode : seq_mute ? 10 : atomic_load(&native_levels_enabled) ? (int)atomic_load(&native_levels_mode) : 10;
    /* A lit Mute button is not a screen/mode identity. Never let it turn
     * note-layout playback into source-bank playback or enable hidden Seq. */
    int enabled=picker || sample_visible==1 || seq_mute;
    void *program=things ? FIELD(things,0xb8,void*) : NULL;
    void *seq=things ? FIELD(things,0x1c8,void*) : NULL;
    void *clip=things ? FIELD(things,0x3d0,void*) : NULL;
    void *list=clip ? (char*)clip+0x688 : NULL;
    if (!list && showing) list=event_list;
    /* Native ChopPadsLighting152a8f8 uses the 16-Levels instrument at668,
     * not the ordinary selected instrument60. Both are borrowed on UI. */
    void *instrument=things ? FIELD(things,0x668,void*) : NULL;
    int bank=native_bank();
    int selected=mode==10 ? bank : picker ? event_pad(&detail_old) : program && instrument
        ? FN(0x286bc20,int(*)(void*,void*))(program,instrument) : -1;
    int first=picker ? picker_base : mode==8 ? 1
        : mode==1 ? -(int)atomic_load(&native_levels_origin)+transpose : 0;
    int64_t len=seq ? FN(0x29571f0,int64_t(*)(void*))(seq) : 0;
    int64_t tick=(int64_t)atomic_load(&clock_tick);
    unsigned gate=(!enabled?1u:0u)|(!running?2u:0u)|(!atomic_load(&clock_valid)?4u:0u)
        |(!program?8u:0u)|(!list?16u:0u)|(selected<0||selected>=128?32u:0u)
        |(mode<0||mode>10?64u:0u)|(len<=0||tick<0?128u:0u)|(showing&&steps&&!picker?256u:0u);
    if(gate!=playback_gate_last && playback_reports<32){
        playback_gate_last=gate;++playback_reports;
        fprintf(stderr,"PAD_STEPS PLAYBACK gate=%03x mode=%d sound=%d origin=%u tick=%lld\n",
                gate,mode,selected,atomic_load(&native_levels_origin),(long long)tick);
    }
    int sample_enabled=enabled&&mode==10&&!picker&&!(showing&&steps);
    if (gate || picker) { /* Editor markers describe original/choice, not loop flashes. */
        playback_previous=-1;memset(playback_until,0,sizeof(playback_until));memset(playback_pitch_until,0,sizeof(playback_pitch_until));atomic_store(&playback_colour_bank,-1);atomic_store(&playback_mask,0);atomic_store(&playback_muted_mask,0);memset(playback_muted_until,0,sizeof(playback_muted_until));sample_palette_publish(things,program,bank,sample_enabled,sample_mute,now);return;
    }
    unsigned muted_pads=0;
    if(mode==10) {
        /* Native MutePadMode15309e0..1530a08 resolves the instrument and
         * tests bit0 at+78. Muted events still flash grey after leaving its
         * overlay; only the LED snapshot changes, never audio state.
         * Do not infer mute from a transient LED colour. */
        for(unsigned i=0;i<16;++i) {
            void *p=FN(0x286bbe0,void*(*)(void*,int))(program,bank*16+(int)i);
            if(!p || sample_instrument_muted(p)) {
                muted_pads|=1u<<i;
            }
        }
    }
    tick%=len;
    if(list!=playback_list || program!=playback_program || selected!=playback_sound || mode!=playback_mode || first!=playback_first) {
        playback_previous=-1;memset(playback_until,0,sizeof(playback_until));memset(playback_muted_until,0,sizeof(playback_muted_until));
        playback_list=list;playback_program=program;playback_sound=selected;playback_mode=mode;playback_first=first;
    }
    int64_t previous=playback_previous;playback_previous=tick;
    int64_t distance=previous<0 ? 0 : (tick-previous+len)%len;
    /* A seek/long UI stall must not flash an entire skipped sequence. Work
     * remains bounded and cannot touch the audio callback or synthesize time. */
    if (previous<0 || distance<=len/2) {
        unsigned budget=4096;
        for(int pass=0;pass<(previous>=0 && tick<previous ? 2 : 1);++pass) {
            int64_t low=previous<0 ? tick : pass==0 ? previous+1 : 0;
            int64_t high=previous>=0 && tick<previous && pass==0 ? len-1 : tick;
            void *tree=(char*)list+0x60,*end=(char*)list+0x68;
            void *node=FN(0x27d00f0,void*(*)(void*,int64_t))(tree,low);
            while(node && node!=end && budget) {
                --budget;struct step_event e;memcpy(&e,(char*)node+0x20,sizeof(e));
                if(e.time>high) break;
                unsigned note=e.data[0x20];
                int pad=note<128 ? FIELD(program,0x14a8+note*4,int) : -1;
                int parameter=mode<10 ? level_params[mode] : -1;
                if(mode==10 && musical(&e) && pad>=0 && pad/16==bank){
                    playback_until[pad%16]=now+120;
                    playback_muted_until[pad%16]=(event_muted(&e)||(muted_pads&(1u<<(pad%16))))?now+120:0;
                }
                else if(mode<10 && musical(&e) && !event_muted(&e) && pad==selected && (parameter<0 || (FIELD(&e,0x14,uint16_t)&(1u<<parameter)))) {
                    if(mode==1){int v=(int)lround(level_value(&e,1));if(v>=-120&&v<=120)playback_pitch_until[v+120]=now+120;}
                    int key=level_nearest(&e,mode,first);
                    struct step_event candidate;level_candidate(&e,&candidate,mode,key,first);
                    /* A pitch/slice outside this displayed page has no key. */
                    if ((mode!=1 && mode!=8) || fabs(level_value(&candidate,mode)-level_value(&e,mode))<.01)
                        playback_until[key]=now+120;
                }
                node=FN(0xb51738,void*(*)(void*))(node);
            }
        }
    }
    for(unsigned i=0;i<16;++i) if(now<playback_until[i]) mask|=1u<<i;
    if(mask && mask!=atomic_load(&playback_mask) && playback_reports<32){
        ++playback_reports;fprintf(stderr,"PAD_STEPS PLAYBACK_PADS mode=%d mask=%04x\n",mode,mask);
    }
    unsigned muted_mask=0;for(unsigned i=0;i<16;++i)if(now<playback_muted_until[i]||(muted_pads&(1u<<i)))muted_mask|=1u<<i;
    atomic_store(&playback_muted_mask,muted_mask&mask);
    sample_palette_publish(things,program,bank,sample_enabled,sample_mute,now);
    atomic_store(&playback_colour_bank,mode==10?bank:-1);
    atomic_store(&playback_mask,mask);
}
int64_t step_native_clock_ticks(uint64_t *serial,unsigned *running)
{
    if(serial)*serial=atomic_load(&clock_serial);
    if(running)*running=atomic_load(&clock_running)&&atomic_load(&clock_valid);
    return (int64_t)atomic_load(&clock_tick);
}
int step_native_playback_pitches(int *out,int max)
{
    uint64_t now=milliseconds();int n=0;
    for(int i=0;i<241&&n<max;++i)if(now<playback_pitch_until[i])out[n++]=i-120;
    return n;
}
static void picker_select(unsigned pad)
{
    if ((picker_mode==1 && picker_base+(int)pad>120) || (picker_mode==8 && picker_base+(int)pad>picker_slice_count)) return;
    level_candidate(&detail_old,&detail_edit,picker_mode,(int)pad,picker_base);
    picker_has_choice=1;
    picker_value=(int)lround(level_value(&detail_edit,picker_mode));
}
static int picker_note_index(const struct step_event*e)
{
    for(unsigned i=0;i<picker_note_count;++i)
        if(fabs(level_value(&picker_notes[i],1)-level_value(e,1))<.01)return (int)i;
    return -1;
}
static void picker_toggle(unsigned pad)
{
    picker_select(pad);
    int i=picker_note_index(&detail_edit);
    if(i>=0){memmove(picker_notes+i,picker_notes+i+1,(picker_note_count-(unsigned)i-1)*sizeof(*picker_notes));--picker_note_count;}
    else if(picker_note_count<16)picker_notes[picker_note_count++]=detail_edit;
    else say("Maximum16 notes; deselect one first");
    picker_toggle_notes=1;
}
static void audition_picker(unsigned pad){
    stop_picker_audition();
    struct step_event e=detail_edit;e.time=0;e.type&=~0x300u;
    void*t=controller?FIELD(controller,0x248,void*):NULL;
    int result=workflow_event_preview(t?FIELD(t,0xb8,void*):NULL,&e,pad,(unsigned)picker_mode);
    if(result==1){picker_audition_active=1;picker_audition_pad=pad;picker_preview_wait=1;picker_probe("AUDIO_WAIT",pad);}
    else say(result==2?"Preview blocked: recording":"Preview input unavailable");
}
void step_native_pad_release(unsigned pad,uint64_t t){
    if(on_ui()&&current(t)&&(t&T_PICKER)&&picker_audition_active&&pad==picker_audition_pad){picker_preview_wait=1;picker_probe("AUDIO_WAIT",pad);workflow_event_preview_stop(0);picker_audition_active=0;}
}
static void picker_start(int kind,const struct step_event *source,int inserting)
{
    struct step_event selected=*source;
    if(!(event_categories(&selected)&(kind==15?1u:2u)) || (!inserting&&!choose_exact(&selected))){
        menu=0;invalidate();say("Event changed: select again");return;}
    picker_insert=inserting;
    detail_old=selected;
    picker_note_count=0;picker_toggle_notes=0;
    picker_has_choice=0;
    picker_chop=kind==15;picker_mode=picker_chop ? 8 : event_level_mode(&detail_old);picker_kind=level_params[picker_mode];
    detail_edit=detail_old;picker_value=(int)lround(level_value(&detail_old,picker_mode));
    picker_base=level_first(&detail_old,picker_mode);
    picker_slice_count=event_slices(&detail_old);if(picker_slice_count<1)picker_slice_count=128;
    picker_focus=picker_chop?2:1;
    sound=event_pad(&detail_old);read_sample_name(sample_name);
    if(picker_source_reports++<32)fprintf(stderr,"PAD_STEPS PICKER_SOURCE mask=%04x last=%u articulation=%u slice_count=%d\n",
            FIELD(&detail_old,0x14,uint16_t),detail_old.data[0x21],detail_old.data[0x23],event_slices(&detail_old));
    menu=3;
    void*things=FIELD(controller,0x248,void*);
    /* Native Sample1545074 and engine2115034 resolve CurrentThings+140.
     * Full-MPC EventAudition uses a different selection at+368. */
    preview_gate_set(!inserting&&things?FIELD(things,0x140,void*):NULL,inserting?NULL:&detail_old);
    invalidate();report("EVENT_PICKER_OPEN staged_no_sample_edit");
}
static void picker_begin(int kind,const struct step_event *source){picker_start(kind,source,0);}
static void picker_open(int kind)
{
    if(steps){
        picker_source_kind=kind;
        if(!collect_picker_sources(kind)){say("Too many events: use Events");return;}
        if(!picker_source_count){
            struct slot at[16];
            if(kind!=15 && atomic_load(&write_armed) && scan_page(cursor/16,sound,grid,at) && !at[cursor%16].count){
                unsigned note=(unsigned)FN(0x157f800,int(*)(void*,int))(controller,sound);
                struct step_event e=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))
                    (note,(int64_t)cursor*grid_ticks[grid],grid_ticks[grid],100,0,velocity/127.0f);
                event_choice_set(&e,0,0);picker_start(kind,&e,1);return;
            }
            say(kind==15?"No C event on this step":"No L event; Write On for empty step");return;
        }
        if(picker_source_count==1)picker_begin(kind,&picker_sources[0]);
        else{picker_source_index=0;menu=4;invalidate();report("STEP_EVENT_CHOOSER_OPEN");}
    }else{
        struct step_event source;
        if(!selected_event(&source) || !(event_categories(&source)&(kind==15?1u:2u))){
            say(kind==15?"Select a C event":"Select an L event");return;}
        picker_begin(kind,&source);
    }
}
static void picker_apply(void)
{
    stop_picker_audition();
    /* Re-resolve the complete original event, never a saved list index. */
    if(picker_insert){
        struct slot at[16];int step=(int)(detail_old.time/grid_ticks[grid]);
        if(!scan_page(step/16,sound,grid,at)||at[step%16].count){menu=0;say("Step changed: reopen picker");return;}
        if(!picker_has_choice){say("Select a note or Cancel");return;}
    }else if (!choose_exact(&detail_old)) {menu=0;say("Event changed: reopen picker");return;}
    if(picker_mode==1&&picker_toggle_notes){
        if(!picker_note_count){say("Select at least one note or Cancel");return;}
        if(!apply_picker_notes()){say("Chord could not be applied");return;}
        menu=0;invalidate();say("Selected notes applied");return;
    }
    if(picker_insert){if(!add_event(&detail_edit))return;choose_exact(&detail_edit);}
    else if (!(FN(0x19722e0,int(*)(const void*,const void*))(&detail_old,&detail_edit)&255)) {
        change(&detail_old,&detail_edit);choose_exact(&detail_edit);
    }
    menu=0;invalidate();say("Event choice applied");
}
struct add_context {
    struct native_tree removed,added,changed;
    const struct step_event *event;
    const struct step_event *replace_old,*replace_notes;
    unsigned replace_count;
    uintptr_t result[2];uint64_t title,merge;
    int page,sound,grid,slot,ready,mute_change,mute_value,unmute_sound;
    int64_t move_delta;
    void *replacements;
    unsigned expected;
};
extern void *step_mute_map(uintptr_t);
extern void step_mute_pair(void*,const struct step_event*,const struct step_event*);
extern void step_mute_free(void*);
extern void step_mute_result(void*,void*,void*,uint64_t*,uint64_t*,uintptr_t*,uintptr_t);
static int scan_page_into(int,int,int,struct slot[16],int,struct native_tree*);
static void add_command(void *opaque)
{
    struct add_context *c=opaque;
    if(c->replace_count){
        if(c->replace_old)FN(0x1574460,void(*)(void*,const void*))(&c->removed,c->replace_old);
        for(unsigned i=0;i<c->replace_count;++i)FN(0x1574460,void(*)(void*,const void*))(&c->added,c->replace_notes+i);
    }
    else if (c->event) FN(0x1574460,void(*)(void*,const void*))(&c->added,c->event);
    else {
        struct slot checked[16];
        /* Build an owned native multiset before dispatch. No mutation while
         * traversing borrowed source nodes; duplicates remain distinct. */
        if(c->unmute_sound){
            void*end=(char*)event_list+0x68;
            void*node=FIELD(event_list,0x78,void*);unsigned budget=65536;
            while(node!=end&&budget--){
                if(!node)return;
                const struct step_event*e=(void*)((char*)node+0x20);
                if(musical(e)&&event_pad(e)==c->sound&&event_muted(e))FN(0x1574460,void(*)(void*,const void*))(&c->removed,e);
                node=FN(0xb51738,void*(*)(void*))(node);
            }
            if(node!=end||!c->removed.size)return;
        }else if (!scan_page_into(c->page,c->sound,c->grid,checked,c->slot,&c->removed)
            || c->removed.size!=c->expected || !c->removed.size) return;
        if(c->mute_change||c->move_delta){
            c->replacements=step_mute_map(base);
            void *end=&c->removed.colour,*node=c->removed.left;
            while(node!=end){
                struct step_event e;memcpy(&e,(char*)node+0x20,sizeof(e));
                if(c->mute_change)e.type=(e.type&~0x200u)|(c->mute_value?0x200u:0u);
                if(c->move_delta){e.time=(e.time+c->move_delta)%length;if(e.time<0)e.time+=length;}
                step_mute_pair(c->replacements,(const struct step_event*)((char*)node+0x20),&e);
                node=FN(0xb51738,void*(*)(void*))(node);
            }
        }
    }
    FN(0xf535c0,void(*)(uint64_t*,const char*))(&c->title,c->replace_count ? "Set Chord Notes" : c->event ? "Add Event" : c->move_delta ? "Move Step Notes" : c->mute_change ? (c->mute_value ? "Mute Step Notes" : "Unmute Step Notes") : "Delete Step Notes");
    FN(0xf535c0,void(*)(uint64_t*,const char*))(&c->merge,"");
    if(c->mute_change||c->move_delta)
        step_mute_result(FIELD(controller,0x240,void*),event_list,c->replacements,&c->title,&c->merge,c->result,base+0x194fd60);
    else step_add_result(FIELD(controller,0x240,void*),event_list,&c->removed,&c->added,&c->changed,
                    &c->title,&c->merge,0,c->result,base+0x19500e0);
    c->ready=1;
}
static int run_set_command(struct add_context *c)
{
    tree_init(&c->removed);tree_init(&c->added);tree_init(&c->changed);
    int ok=step_try_call(add_command,c);
    release_command(c->result);
    step_mute_free(c->replacements);
    if(c->title) FN(0xf0a290,void(*)(uint64_t*))(&c->title);
    if(c->merge) FN(0xf0a290,void(*)(uint64_t*))(&c->merge);
    if(c->added.parent) FN(0x157df10,void(*)(void*))(c->added.parent);
    if(c->removed.parent) FN(0x157df10,void(*)(void*))(c->removed.parent);
    return ok && c->ready;
}
static int add_event(const struct step_event *e)
{
    if (!musical(e) || e->time<0 || e->time>=length) { say("Cannot insert this event"); return 0; }
    struct add_context c={0};c.event=e;
    if (!run_set_command(&c)) {say("Native add failed");return 0;}
    report("ADD_EVENT native_Undo inline_payload_preserved no_removal_set");return 1;
}
static int apply_picker_notes(void)
{
    if(picker_note_count==1&&!picker_insert){
        if(!(FN(0x19722e0,int(*)(const void*,const void*))(&detail_old,picker_notes)&255))change(&detail_old,picker_notes);
        choose_exact(picker_notes);return 1;
    }
    struct add_context c={0};c.replace_old=picker_insert?NULL:&detail_old;c.replace_notes=picker_notes;c.replace_count=picker_note_count;
    if(!run_set_command(&c))return 0;
    choose_exact(&picker_notes[picker_note_count-1]);
    report("CHORD_APPLIED separate_native_events single_Undo");return 1;
}
static void read_sample_name(char out[513])
{
    strcpy(out,"Empty");
    sample_tuning=0;
    if (!controller) return;
    void *things=FIELD(controller,0x248,void*),*program=things ? FIELD(things,0xb8,void*) : NULL;
    if (!program) return;
    /* Program instrument index is the pad index. MIDI note is only used
     * in SequenceEvent; a standard A03 is instrument2 but MIDI note38. */
    if (sound<0 || sound>=128) return;
    void *instrument=FN(0x286bbe0,void*(*)(void*,int))(program,sound);
    if (!instrument || !FIELD(instrument,0x22e8,void*)) return;
    uintptr_t sample[2]={0};
    step_layer_sample(instrument,0,sample,base+0x2803b90);
    if (sample[0]) {
        const char *name=FIELD((void*)sample[0],0xa8,const char*);
        size_t size=name ? strnlen(name,513) : 0;
        if (size && size<=512) {
            memcpy(out,name,size+1);
            /* 1.3.0.12 serializers2801aa4 and2815aa4 read signed
             * TuneCoarse bytes (1da0698 sign-extends its input).
             * Read this event's instrument/layer, never the last Sample UI. */
            void *layer=FIELD(instrument,0x22e8,void*);
            sample_tuning=FIELD(instrument,0xb98,int8_t)+FIELD(layer,0x60,int8_t);
        }
    }
    if (sample[1]) FN(0x123bca0,void(*)(void*))((void*)sample[1]);
}
static unsigned read_sound_presence(void)
{
    if(!controller)return 0;
    void *things=FIELD(controller,0x248,void*),*program=things?FIELD(things,0xb8,void*):NULL;
    if(!program)return 0;
    unsigned mask=0;
    for(unsigned pad=0;pad<16;++pad){
        void *instrument=FN(0x286bbe0,void*(*)(void*,int))(program,browse_bank*16+(int)pad);
        if(instrument && FN(0x2804760,int(*)(void*))(instrument))mask|=1u<<pad;
    }
    return mask;
}
static int event_slices(const struct step_event *e)
{
    if(!controller||!musical(e))return 0;
    void *things=FIELD(controller,0x248,void*),*program=things?FIELD(things,0xb8,void*):NULL;
    int pad=event_pad(e);if(!program||pad<0||pad>=128)return 0;
    void *instrument=FN(0x286bbe0,void*(*)(void*,int))(program,pad);
    if(!instrument||!FIELD(instrument,0x22e8,void*)||FIELD(instrument,0x22e8,void*)==FIELD(instrument,0x22f0,void*))return 0;
    uintptr_t sample[2]={0};step_layer_sample(instrument,0,sample,base+0x2803b90);
    int count=0;
    if(sample[0]&&FIELD((void*)sample[0],0x130,void*)){
        void *s=FN(0x2941ff0,void*(*)(void*))((void*)sample[0]);
        uintptr_t begin=FIELD(s,0x60,uintptr_t),end=FIELD(s,0x68,uintptr_t);
        if(begin&&end>=begin&&(end-begin)%8==0&&end-begin<=128*8)count=(int)((end-begin)/8);
    }
    if(sample[1])FN(0x123bca0,void(*)(void*))((void*)sample[1]);
    return count;
}
static int scan_page_into(int target_page,int target_sound,int target_grid,struct slot out[16],
                          int remove_slot,struct native_tree *removed)
{
    memset(out,0,16*sizeof(*out));
    if (!event_list) return 0;
    int64_t unit=grid_ticks[target_grid],start=(int64_t)target_page*16*unit;
    int64_t low=start-unit/2,high=start+16*unit-unit/2;
    int total=(int)((length+unit-1)/unit);
    if(start+16*unit>=length) high=length;
    if (low<0) low=0;
    void *tree=(char*)event_list+0x60,*end=(char*)event_list+0x68;
    void *node=FN(0x27d00f0,void*(*)(void*,int64_t))(tree,low);
    unsigned budget=4096;
    while (node!=end && budget--) {
        if (!node) return 0;
        struct step_event e; memcpy(&e,(char*)node+0x20,sizeof(e));
        if (e.time>=high || e.time>=length) return 1;
        if (e.time<low) return 0;
        int absolute=(int)((e.time+unit/2)/unit);
        if(absolute>=total) absolute=total-1;
        int step=absolute-target_page*16;
        if (step>=0 && step<16 && (target_sound<0?event_pad(&e)!=sound:event_pad(&e)==target_sound) && musical(&e)) {
            if (!out[step].count) out[step].first=e;
            ++out[step].count;
            out[step].muted+=(unsigned)event_muted(&e);
            out[step].categories|=event_categories(&e);
            if (removed && step==remove_slot) FN(0x1574460,void(*)(void*,const void*))(removed,&e);
        }
        node=FN(0xb51738,void*(*)(void*))(node);
    }
    return node==end;
}
static int scan_page(int p,int s,int g,struct slot out[16])
{ return scan_page_into(p,s,g,out,-1,NULL); }
static int collect_picker_sources(int kind)
{
    picker_source_count=0;
    /* Read the selected cursor directly: Follow can display another page. */
    if(!event_list)return 1;
    int64_t unit=grid_ticks[grid],low=(int64_t)cursor*unit-unit/2;
    int64_t high=cursor==count()-1?length:(int64_t)(cursor+1)*unit-unit/2;
    if(low<0)low=0;
    void *tree=(char*)event_list+0x60,*end=(char*)event_list+0x68;
    void *node=FN(0x27d00f0,void*(*)(void*,int64_t))(tree,low);
    unsigned budget=4096,required=kind==15?1u:2u;
    while(node && node!=end && budget--){
        struct step_event e;memcpy(&e,(char*)node+0x20,sizeof(e));
        if(e.time>=high || e.time>=length)return 1;
        if(e.time<low)return 0;
        if(musical(&e) && event_pad(&e)==sound && (event_categories(&e)&required)){
            if(picker_source_count==PICKER_SOURCES_MAX){picker_source_count=0;return 0;}
            picker_sources[picker_source_count++]=e;
        }
        node=FN(0xb51738,void*(*)(void*))(node);
    }
    if(node!=end){picker_source_count=0;return 0;}
    return 1;
}
static void hold(int step)
{
    cursor=clamp(step,0,count()-1); edit_tick=(int64_t)cursor*grid_ticks[grid];
    page=cursor/16; follow=0; choose_native(edit_tick,sound);
}
static void leave(void)
{
    if (steps && controller && event_list) choose_native(edit_tick,sound);
    steps=want_steps=0;menu=0;mute_latched=mute_held=mute_used=0;atomic_store(&mute_active,0);
    atomic_store(&write_armed,0);atomic_store(&toggle_armed,0);atomic_store(&requested_sound,0); invalidate(); publish(); repaint(); report("EVENTS_RESTORED");
}
int step_native_shortcut(uint64_t t)
{
    if (!on_ui()) return 0;
    step_native_tick();
    if (!current(t)) return 0;
    if (!(t&T_STEPS) && (t&T_ERASE) && !menu) return 1;
    if (!(t&T_STEPS) || !(t&T_SHIFT) || !steps) return 0;
    menu=0;atomic_store(&requested_sound,0);invalidate();showing=0;publish();
    report("SHIFT_PAD_NATIVE_SHORTCUT suspended_Steps");return 1;
}
static uint64_t step_palette_value(unsigned events,unsigned muted,int mute,int erase,int selected,int playing,uint64_t phase)
{
    int has=events!=0,all_muted=has&&muted==events,mixed=muted&&!all_muted;
    if(erase&&has)return pad_light(PAD_DANGER,pad_blink_on(phase)?PAD_BRIGHT_DANGER:.20f);
    if(mute&&has){
        /* Keep the mute status visible under both cursors. A mixed step
         * still has notes to mute, so show both states in turn. */
        uint32_t status=all_muted||(mixed&&phase%640>=320)?PAD_DANGER:PAD_FREE;
        return pad_light(status,playing?1.f:.75f);
    }
    int chosen=music_pad_colour((unsigned)sound);
    uint32_t content=chosen?music_identity_rgb(chosen):PAD_CONTENT;
    /* Called only for valid steps: keep the writable range visible even
     * when empty; positions beyond sequence length stay completely off. */
    int muted_phase=all_muted||(mixed&&phase%640>=320);
    uint32_t rgb=!has?0xffffffu:muted_phase?(music_pad_lighting_mode()?PAD_MUTED:PAD_DANGER):content;
    float brightness=!has?.06f:muted_phase?.32f:.80f;
    /* Alternate white with the semantic colour to keep both selection
     * and mute/delete status visible. Playback has a separate cyan cursor. */
    if(selected&&(!has||music_cursor_on(phase,150))){rgb=PAD_SELECT;brightness=.90f;}
    if(playing){rgb=all_muted?(music_pad_lighting_mode()?PAD_MUTED:PAD_DANGER):(music_pad_lighting_mode()?PAD_PLAYHEAD:PAD_FREE);brightness=all_muted?.65f:.95f;}
    return pad_light(rgb,brightness);
}
/* Sound selection overlays white briefly, then reveals the assigned hue.
 * This is a UI-only palette phase; the sound/bank selection is unchanged. */
static uint64_t sound_palette_value(unsigned pad,uint64_t phase){
    static int last_sound=-1,last_bank=-1;static uint64_t since;
    if(sound!=last_sound||browse_bank!=last_bank||phase<since){last_sound=sound;last_bank=browse_bank;since=phase;}
    int chosen=music_pad_colour((unsigned)browse_bank*16+pad);
    int present=!!(sound_present&(1u<<pad));
    uint32_t rgb=present?music_identity_rgb((unsigned)chosen):PAD_EMPTY;
    float brightness=present?PAD_BRIGHT_CONTENT:PAD_BRIGHT_EMPTY;
    if(sound/16==browse_bank&&(unsigned)(sound%16)==pad){
        return pad_focus_light(rgb,brightness,phase-since);
    }
    return pad_light(rgb,brightness);
}
static void palette_at(uint64_t phase)
{
    int mute_mode=atomic_load(&mute_active)!=0;
    uint64_t tune_lights=0;
    if(menu==3&&picker_mode==1){
        struct music_sample settings;
        music_get_sample(sample_name,&settings);
        tune_lights=music_light_snapshot(&settings,sample_tuning);
    }
    int selected_pad=menu==3&&picker_has_choice ? level_nearest(&detail_edit,picker_mode,picker_base) : -1;
    int original_pad=menu==3&&!picker_insert ? level_nearest(&detail_old,picker_mode,picker_base) : -1;
    if(menu==3&&(picker_mode==1||picker_mode==8)){
        int original_value=(int)lround(level_value(&detail_old,picker_mode));
        if(original_value<picker_base||original_value>picker_base+15)original_pad=-1;
    }
    int chop_colour=0;
    if(menu==3&&picker_mode==8){int pad=event_pad(&detail_old);if(pad>=0&&pad<128)chop_colour=music_pad_colour((unsigned)pad);}
    for (unsigned i=0;i<16;++i) {
        uint32_t rgb=0; float brightness=0;
        int slot=pad_step(i),n=page*16+slot;
        if (menu==3) {
            int value=picker_base+(int)i;
            if ((picker_mode!=8 || value<=picker_slice_count) && (picker_mode!=1 || value<=120)) {
                rgb=0xffffff;brightness=.25f;
                if(picker_mode==1){
                    int replace=0;uint64_t light=music_pitch_light(tune_lights,value,&replace);
                    if(replace){rgb=(uint32_t)light;uint32_t bits=(uint32_t)(light>>32);memcpy(&brightness,&bits,4);}
                }
                if(picker_mode==8){rgb=chop_colour?music_identity_rgb(chop_colour):PAD_CONTENT;brightness=PAD_BRIGHT_CONTENT;}
                int original=(int)i==original_pad,chosen=(int)i==selected_pad;
                if(picker_mode==1&&picker_toggle_notes){struct step_event candidate;level_candidate(&detail_old,&candidate,1,(int)i,picker_base);chosen=picker_note_index(&candidate)>=0;}
                if(original){rgb=PAD_ORIGINAL;brightness=PAD_BRIGHT_ORIGINAL;}
                if(chosen){
                    atomic_store(&lights[i],pad_selection_light(rgb,brightness,phase));continue;
                }
            }
        }
        else if (atomic_load(&requested_sound)) {
            atomic_store(&lights[i],sound_palette_value(i,phase));continue;
        }
        else if (n<count()) {
            int playing=was_playing&&(int)(play_tick/grid_ticks[grid])==n;
            if(!mute_mode&&show_other_sounds&&(other_sound_steps&(1u<<slot))&&!slots[slot].count&&(!cursor_visible||n!=cursor)&&!playing){
                rgb=PAD_CONTEXT;brightness=.20f;
                uint32_t f;memcpy(&f,&brightness,4);atomic_store(&lights[i],(uint64_t)rgb|((uint64_t)f<<32));continue;
            }
            atomic_store(&lights[i],step_palette_value(slots[slot].count,slots[slot].muted,
                mute_mode,atomic_load(&requested_erase)!=0,cursor_visible&&n==cursor,
                was_playing&&(int)(play_tick/grid_ticks[grid])==n,phase));
            continue;
        }
        uint32_t f; memcpy(&f,&brightness,4); atomic_store(&lights[i],(uint64_t)rgb|((uint64_t)f<<32));
    }
}
static void palette(void){palette_at(milliseconds());}
int step_native_light(unsigned pad,uint64_t *light)
{
    uint64_t t=atomic_load(&ticket);
    if(pad>=16) return 0;
    if(!music_pad_lighting_mode()&&!(t&(T_STEPS|T_PICKER)))return 0;
    /* Shift alone changes knob/button meanings, not Sample pad ownership.
     * Real bank/workflow overlays are handled before this receiver. */
    if(!(t&(T_STEPS|T_PICKER))&&atomic_load(&sample_palette_enabled)&&!atomic_load(&requested_erase)){
        *light=atomic_load(&sample_palette[pad]);return 1;
    }
    if (!(t&T_STEPS) && (!atomic_load(&requested_shift) || (t&T_PICKER)) && (atomic_load(&playback_mask)&(1u<<pad))) {
        int bank=atomic_load(&playback_colour_bank);
        if(bank>=0&&atomic_load(&requested_erase))return 0;
        int chosen=bank>=0&&bank<8?music_pad_colour((unsigned)bank*16+pad):0;
        uint32_t rgb=(atomic_load(&playback_muted_mask)&(1u<<pad))?PAD_MUTED:bank>=0?music_identity_rgb((unsigned)chosen):PAD_PLAYHEAD;
        float brightness=rgb==PAD_MUTED?.65f:.9f;uint32_t f;memcpy(&f,&brightness,4);*light=(uint64_t)rgb|((uint64_t)f<<32);return 1;
    }
    if (!(t&(T_STEPS|T_PICKER))) return 0;
    *light=atomic_load(&lights[pad]); return 1;
}
int step_native_tick(void)
{
    if (!on_ui()) return 0;
    if(picker_preview_wait&&!workflow_event_preview_pending()){picker_preview_wait=0;picker_probe("PAD_DONE",picker_audition_pad);}
    if (atomic_exchange(&dead,0)) { root=NULL;controller=NULL;steps=menu=showing=0;reset_mute();invalidate(); }
    if (atomic_exchange(&project_reset,0)) { atomic_store(&write_armed,0);atomic_store(&toggle_armed,0);sound_initialized=0;clip_valid=0;steps=want_steps=menu=0;last_sequence=NULL;reset_mute();invalidate(); }
    if (atomic_exchange(&queue_fault,0)) { atomic_store(&write_armed,0);atomic_store(&toggle_armed,0);steps=want_steps=menu=0;reset_mute();invalidate();say("Steps stopped: input queue full"); }
    if(menu!=3){stop_picker_audition();preview_gate_set(NULL,NULL);}
    int previous_showing=showing;
    showing=root && FIELD(root,0,uintptr_t)==base+SCREEN_VPTR
        && FN(0x112fd50,int(*)(void*))(root) && !FN(0x112f6b0,int(*)(void*))(root);
    if (!showing) { stop_picker_audition();preview_gate_set(NULL,NULL);if(previous_showing) invalidate();menu=0;
        mute_latched=mute_held=mute_used=0;atomic_store(&mute_active,0);
        atomic_store(&event_edit_mask,0);publish();return root!=NULL; }
    if (message[0] && milliseconds()>=message_until) {message[0]=0;repaint();}
    controller=FIELD(root,0x108,void*);
    if (!controller || FIELD(controller,0,uintptr_t)!=base+CONTROLLER_VPTR) { stop_picker_audition();preview_gate_set(NULL,NULL);showing=0;publish();return 0; }
    void *things=FIELD(controller,0x248,void*);
    sequence=things ? FIELD(things,0x1c8,void*) : NULL;
    void *program=things ? FIELD(things,0xb8,void*) : NULL;
    event_list=FIELD(controller,0xc0,void*);
    length=sequence ? FN(0x29571f0,int64_t(*)(void*))(sequence) : 0;
    if (!event_list || !program || length<=0 || length>960*128*64) { stop_picker_audition();preview_gate_set(NULL,NULL);showing=0;steps=menu=0;publish();return 0; }
    if(menu || !steps)reset_mute();
    if (last_sequence!=event_list || last_program!=program) {
        stop_picker_audition();
        preview_gate_set(NULL,NULL);
        last_sequence=event_list;last_program=program;menu=0;invalidate();
        mute_latched=mute_held=mute_used=0;atomic_store(&mute_active,0);
        edit_tick=clamp_tick(FIELD(controller,0x70,int64_t)); cursor=(int)(edit_tick/grid_ticks[grid]);page=cursor/16;
    }
    shift=(int)atomic_load(&requested_shift);
    if (!steps) edit_tick=clamp_tick(FIELD(controller,0x70,int64_t));
    /* CurrentThings+0x88 is not the transport facade. Use the running
     * snapshot captured from the same engine as the position. */
    was_playing=!!atomic_load(&clock_running);
    unsigned serial=atomic_load(&clock_serial);
    if (serial!=last_clock_serial) last_clock_serial=serial;
    play_tick=(int64_t)atomic_load(&clock_tick);
    if (!atomic_load(&clock_valid) || play_tick<0) was_playing=0;
    if (play_tick>=length) play_tick%=length;
    if (steps) {
        int bank=native_bank();
        if (bank>=0 && bank!=last_native_bank) {browse_bank=bank;message[0]=0;repaint();}
        last_native_bank=bank;
        cursor=clamp(cursor,0,count()-1);edit_tick=(int64_t)cursor*grid_ticks[grid];
        page=clamp(page,0,pages()-1);
        if (follow && was_playing && !menu) page=(int)(play_tick/grid_ticks[grid])/16;
        char name[513];read_sample_name(name);
        if (strcmp(name,sample_name)) {memcpy(sample_name,name,strlen(name)+1);repaint();}
        scan_ok=scan_page(page,sound,grid,slots);
        other_sound_steps=0;
        if(show_other_sounds){struct slot other[16];if(scan_page(page,-1,grid,other))for(unsigned i=0;i<16;++i)if(other[i].count)other_sound_steps|=1u<<i;}
        if(atomic_load(&requested_sound))sound_present=read_sound_presence();
        palette();
    }
    else if (menu==3) palette();
    struct step_event selected;
    struct slot cursor_slots[16];
    unsigned categories=menu==3?event_categories(&detail_old):menu?0u:
        steps?(cursor/16==page?(scan_ok?slots[cursor%16].categories:0u):
            scan_page(cursor/16,sound,grid,cursor_slots)?cursor_slots[cursor%16].categories:0u):
        selected_event(&selected)?event_categories(&selected):0u;
    atomic_store(&event_edit_mask,category_buttons(categories));
    publish();
    uint64_t visual=(uint64_t)(unsigned)page|((uint64_t)(unsigned)cursor<<16)
        |((uint64_t)(unsigned)(play_tick/grid_ticks[grid])<<32)|((uint64_t)(unsigned)shift<<60)
        |((uint64_t)(unsigned)was_playing<<61)
        |((uint64_t)atomic_load(&requested_sound)<<59)|((uint64_t)atomic_load(&requested_erase)<<58)
        |((uint64_t)atomic_load(&write_armed)<<57)|((uint64_t)atomic_load(&mute_active)<<56)
        |((uint64_t)atomic_load(&toggle_armed)<<55);
    if (visual!=last_visual) { last_visual=visual; repaint(); }
    return 1;
}

static void operate(void)
{
    struct step_event selected;
    int have=steps ? scan_ok && cursor/16==page && slots[cursor%16].count==1
        : selected_event(&selected);
    if (steps && have) selected=slots[cursor%16].first;
    if (menu_index==0) {
        if (have && musical(&selected)) { clip=selected;clip_offset=selected.time-edit_tick;clip_sound=event_pad(&selected);clip_valid=1;say("Event copied"); }
        else say("Select one note in Events");
        menu=0;
    } else if (menu_index==1 || menu_index==2) {
        if (!clip_valid) {say("Clipboard empty");menu=0;return;}
        struct step_event e=clip;e.time=edit_tick+clip_offset;
        int src=menu_index==2 ? sound : clip_sound;
        e.data[0x20]=(unsigned char)FN(0x157f800,int(*)(void*,int))(controller,src);
        if (e.time<0 || e.time>=length) {say("Outside sequence length");menu=0;return;}
        struct slot check[16];int pg=(int)(e.time/grid_ticks[grid])/16;
        if (!scan_page(pg,src,grid,check)) {say("Too many events on page");menu=0;return;}
        /* Native event comparison also ignores no data: use the complete
         * copied tag-3 payload, including tune, slice and note duration. */
        int slot=(int)((e.time+grid_ticks[grid]/2)/grid_ticks[grid])%16;
        if (check[slot].count==1 && (FN(0x19722e0,int(*)(const void*,const void*))(&check[slot].first,&e)&255)) say("Identical event already exists");
        else if (add_event(&e)) {sound=src;choose_exact(&e);say("Event pasted");}
        menu=0;
    } else if (have) {menu=2;menu_index=0;say("K3 / encoder: note length");}
    else {menu=0;say("Select one note in Events");}
}
static int choose(unsigned v,int value,int lo,int hi,int relative)
{ int n=relative ? value+(v>64 ? (int)v-128 : (int)v) : lo+(int)((v&127)*(unsigned)(hi-lo+1)/128); return clamp(n,lo,hi); }
int step_native_control(void *event,uint64_t t)
{
    if (!on_ui()) return 1;
    step_native_tick(); if (!current(t)) return 1;
    unsigned cc=FIELD(event,8,unsigned char),v=FIELD(event,9,unsigned char);
    int event_shift=!!(t&T_SHIFT);
    int rel=cc==0x64;
    if(menu==4){
        if(rel || cc==0x11)picker_source_index=(unsigned)choose(v,(int)picker_source_index,0,(int)picker_source_count-1,rel);
        repaint();return 1;
    }
    if (menu==3) {
        if(cc>=0x10&&cc<=0x12)picker_focus=(int)cc-0x10+1;
        if(cc==0x10 && !picker_chop){
            int next_mode=choose(v,picker_mode,0,9,0);
            if(next_mode!=picker_mode){picker_note_count=0;picker_toggle_notes=0;picker_has_choice=0;}
            picker_mode=next_mode;picker_kind=level_params[picker_mode];
            picker_base=level_first(&detail_edit,picker_mode);
        }else if(cc==0x11){picker_note_count=0;picker_toggle_notes=0;picker_select((unsigned)choose(v,0,0,15,0));}
        else if(cc==0x12 && (picker_mode==1 || picker_mode==8)){
            picker_base=picker_mode==8 ? 1+16*choose(v,0,0,(picker_slice_count-1)/16,0) : clamp(-120+12*choose(v,0,0,19,0),-120,105);
        }else if (rel) {
            int delta=v>64 ? (int)v-128 : (int)v;
            int selected=level_nearest(&detail_edit,picker_mode,picker_base);
            picker_note_count=0;picker_toggle_notes=0;
            picker_select((unsigned)clamp(selected+delta,0,15));
        }
        step_native_tick();repaint();return 1;
    }
    if (!steps && !menu) {
        if (cc==0x12 && event_shift) {
            struct step_event old;
            if (!selected_event(&old) || !event_categories(&old)) {say("Select a C/L event");return 1;}
            int mode=event_level_mode(&old),first=level_first(&old,mode);
            struct step_event next=old;
            int pad=choose(v,0,0,15,0);level_candidate(&old,&next,mode,pad,first);
            if (!(FN(0x19722e0,int(*)(const void*,const void*))(&old,&next)&255)) {
                change(&old,&next);choose_exact(&next);
            }
            char s[64];snprintf(s,sizeof(s),"%s: %g",level_names[mode],level_value(&next,mode));say(s);return 1;
        }
        if (cc!=0x64) return 0;
        int delta=v>64 ? (int)v-128 : (int)v;
        int64_t unit=FIELD(controller,0x78,int64_t);
        if (unit<=0 || unit>length || !delta) return 1;
        struct step_event old;
        int have=event_shift && selected_event(&old);
        int64_t time=((have ? old.time : edit_tick)+(int64_t)delta*unit)%length;
        if (time<0) time+=length;
        if (have) {
            struct step_event next=old;next.time=time;
            if (time!=old.time) {
                change(&old,&next);choose_exact(&next);
                say("Event moved: Shift+encoder");
            }
        } else {
            FN(0x15814a0,void(*)(void*,int64_t))(controller,time);
            if (event_shift) say("No selected note: cursor moved");
        }
        repaint();return 1;
    }
    if (menu==1) {if (rel || cc==0x12) menu_index=choose(v,menu_index,0,3,rel);repaint();return 1;}
    if (menu==2) {
        if (cc==0x12) {
            struct step_event e;
            int have=steps ? cursor/16==page && slots[cursor%16].count==1 : selected_event(&e);
            if (steps && have) e=slots[cursor%16].first;
            if (have) {
                struct step_event next=e;
                int n=(int)((FIELD(&e,0x28,int64_t)+grid_ticks[grid]/2)/grid_ticks[grid]);
                n=choose(v,n,1,64,rel);FIELD(&next,0x28,int64_t)=(int64_t)n*grid_ticks[grid];
                if (memcmp(&next,&e,sizeof(e))) change(&e,&next);
            }
        }
        repaint();return 1;
    }
    if (cc==0x14) return 1; /* Native fader target is not the Step selection. */
    if (cc!=0x10 && cc!=0x11 && cc!=0x12 && !rel) return 0;
    if (cc==0x10 && event_shift) return 1;
    if (!rel) focus=(int)cc-0x10+1;
    if (rel) {
        int d=v>64 ? (int)v-128 : (int)v;
        int64_t time=edit_tick+(int64_t)d*grid_ticks[grid];
        if(event_shift){
            if(!d)return 1;
            time%=length;if(time<0)time+=length;
            struct slot selected[16];
            if(!scan_page(cursor/16,sound,grid,selected)){say("Cannot read selected step");return 1;}
            if(selected[cursor%16].count){
                struct add_context c={0};c.page=cursor/16;c.sound=sound;c.grid=grid;c.slot=cursor%16;
                c.expected=selected[cursor%16].count;c.move_delta=(int64_t)d*grid_ticks[grid];
                if(!run_set_command(&c)){say("Native move failed");return 1;}
                say("Step notes moved: Shift+encoder");
            }else say("Empty step: cursor moved");
        }
        hold((int)(clamp_tick(time)/grid_ticks[grid]));
    } else if (cc==0x10) {
        int bars=sequence?FIELD(sequence,0x200,int):0;
        int next=choose(v,bars,1,128,rel);
        message[0]=0;
        if(bars>=1&&bars<=128&&next!=bars){
            /* Event command factory 19500e0 dereferences this context's
             * +60 before dispatching to the native Undo manager. */
            void *context=FIELD(controller,0x240,void*);
            void *manager=context?FIELD(context,0x60,void*):NULL;
            if(!step_set_bars(base,manager,sequence,next))say("Cannot change sequence length");
        }
    } else if (cc==0x11) {
        grid=event_shift ? grid%5+5*choose(v,grid>=5,0,1,rel)
            : choose(v,grid%5,0,4,rel)+(grid>=5 ? 5 : 0);
        cursor=(int)(edit_tick/grid_ticks[grid]);page=cursor/16;message[0]=0;
    } else if(event_shift) {
        show_other_sounds=v>=64;say(show_other_sounds?"Other sounds: on":"Other sounds: off");
    } else {
        velocity=choose(v,velocity,1,127,rel);
        if (scan_ok && cursor/16==page && slots[cursor%16].count==1) {
            struct step_event e=slots[cursor%16].first,next=e;
            FIELD(&next,0x10,float)=velocity/127.0f;
            if (memcmp(&e,&next,sizeof(e))) change(&e,&next);
        }
    }
    step_native_tick();repaint();return 1;
}
int step_native_reset_knob(unsigned column,unsigned shifted){
 if(!showing||!steps)return 0;
 if(menu||column>2)return 1;
 if(column==1){grid=shifted?grid%5:2+(grid>=5?5:0);cursor=(int)(edit_tick/grid_ticks[grid]);page=cursor/16;}
 else if(column==2){if(shifted)show_other_sounds=1;else velocity=100;}
    /* Length is a structural edit; reset gestures do not shorten a sequence. */
 step_native_tick();repaint();return 1;
}
int step_native_button_at(void *event,uint64_t t,uint64_t when_ms)
{
    if (!on_ui()) return 1;
    unsigned code=FIELD(event,8,unsigned char),down=FIELD(event,9,unsigned char)!=0;
    if (code>=112) return 0;
    if(code==BUTTON_MUTE && !down && atomic_load(&owned_buttons[code])){
        atomic_store(&owned_buttons[code],0);atomic_fetch_sub(&held_buttons,1);
        if(showing && steps && !menu && mute_held){
            if(!mute_used && when_ms>=mute_down_ms && when_ms-mute_down_ms<300)mute_latched=!mute_latched;
            else mute_latched=0; /* A hold always ends on release, even if previously latched. */
            mute_held=0;atomic_store(&mute_active,mute_latched);
            say(mute_latched?"Mute mode On":"Mute mode Off");
        }else {mute_held=0;atomic_store(&mute_active,0);}
        step_native_tick();repaint();return 1;
    }
    /* A native Erase down must have a native release even if a pad opens a
     * confirmation or another screen takes ownership in between. */
    if (code==BUTTON_ERASE && !down && atomic_exchange(&events_erase_forwarded,0)) {
        step_native_tick();
        return 2; /* Native modifier release; never erase an Events row. */
    }
    if (atomic_load(&owned_buttons[code])) {
        if (!down) {atomic_store(&owned_buttons[code],0);atomic_fetch_sub(&held_buttons,1);}
        return 1;
    }
    step_native_tick(); if (!current(t)) return 0;
    if (!down) return 0;
    int event_shift=!!(t&T_SHIFT);
    if (code==BUTTON_ERASE && !steps && !menu) {
        atomic_store(&events_erase_forwarded,1);
        return 2; /* retain stock Erase modifier for Erase+pad */
    }
    int owned=0;
    if (code==0x0d) {
        owned=1;
        if (menu) menu=0;
        else if (steps && event_shift) {follow=!follow;say(follow ? "Follow on" : "Hold page");}
        else if (steps) leave();
        else {
            steps=want_steps=1;menu=0;focus=0;follow=1;
            int bank=native_bank();
            if (bank>=0) browse_bank=bank;
            if (!sound_initialized) {
                void *things=FIELD(controller,0x248,void*),*program=things ? FIELD(things,0xb8,void*) : NULL;
                void *instrument=things ? FIELD(things,0x60,void*) : NULL;
                int selected=program && instrument ? FN(0x286bc20,int(*)(void*,void*))(program,instrument) : -1;
                if(selected>=0 && selected<128) sound=selected;
                sound_initialized=1;
            }
            atomic_store(&write_armed,0);atomic_store(&toggle_armed,0);
            edit_tick=clamp_tick(FIELD(controller,0x70,int64_t));cursor=(int)(edit_tick/grid_ticks[grid]);page=cursor/16;
            message[0]=0;invalidate();report("STEPS_OPEN green_notes_amber_edit_cyan_transport");
        }
    } else if (menu && code==0x0c) {menu=0;owned=1;}
    else if (menu && code==BUTTON_RECORD) {owned=1;}
    else if (steps && code==BUTTON_RECORD) {
        owned=1;unsigned armed=!atomic_load(&write_armed);atomic_store(&write_armed,armed);atomic_store(&toggle_armed,0);
        say(armed ? "Write On: pads add/select" : "Write Off: pads select only");
    }
    else if(steps && code==BUTTON_SAMPLE_RECORD && !event_shift){
        owned=1;
        if(!menu){
            unsigned armed=!atomic_load(&toggle_armed);
            atomic_store(&write_armed,0);atomic_store(&toggle_armed,armed);
            say(armed?"Toggle On: pads add/remove":"Toggle Off: pads select only");
        }
    }
    else if(steps && !menu && event_shift && code==BUTTON_MUTE){
        struct add_context c={0};c.sound=sound;c.mute_change=1;c.unmute_sound=1;
        reset_mute();owned=1;
        if(run_set_command(&c))say("All notes of this sound unmuted");
        else say("No muted notes / unmute unavailable");
    }
    else if(steps && !menu && !event_shift && code==BUTTON_MUTE){
        mute_held=1;mute_used=0;mute_down_ms=when_ms;atomic_store(&mute_active,1);
        owned=1;say("Mute: press a populated step");
    }
    else if (steps && (code==0x0c || code==BUTTON_ERASE)) {owned=1;} /* Hold B1+pad: sound select. */
    else if (!event_shift && (code==BUTTON_LEVELS || code==BUTTON_CHOP)) {
        owned=1;if(!menu) picker_open(code==BUTTON_LEVELS ? 0 : 15);
    }
    else if (code==0x0e) {
        owned=1;if (menu==1) operate();else if (menu==2) menu=0;else if(menu==3) picker_apply();
        else if(menu==4 && picker_source_count)picker_begin(picker_source_kind,&picker_sources[picker_source_index]);
        else if (steps || event_shift) {menu=1;menu_index=0;follow=0;}
        else {struct step_event selected;if(selected_event(&selected)){remove_event(&selected);say("Selected event removed");}}
    } else if (menu && (code==BUTTON_MINUS || code==BUTTON_PLUS)) {
        if(menu==4)picker_source_index=(unsigned)clamp((int)picker_source_index+(code==BUTTON_MINUS?-1:1),0,(int)picker_source_count-1);
        else if (menu==3 && (picker_mode==8 || picker_mode==1)) picker_base=clamp(picker_base+(code==BUTTON_MINUS ? -1 : 1)*(picker_mode==8 ? 16 : 12),
            picker_mode==8 ? 1 : -120,picker_mode==8 ? 1+16*((picker_slice_count-1)/16) : 105);
        else menu_index=clamp(menu_index+(code==BUTTON_MINUS ? -1 : 1),0,3);
        owned=1;
    }
    else if (code==0x6f && (steps || menu)) {
        owned=1;if (menu==1) operate();else if (menu==2) menu=0;
        else if(menu==4 && picker_source_count)picker_begin(picker_source_kind,&picker_sources[picker_source_index]);
        else if(steps&&!menu){cursor_visible=!cursor_visible;say(cursor_visible?"Cursor: on":"Cursor: off");}
    } else if (steps && !event_shift && (code==BUTTON_MINUS || code==BUTTON_PLUS)) {
        int p=clamp(page+(code==BUTTON_MINUS ? -1 : 1),0,pages()-1);
        hold(clamp(p*16+cursor%16,0,count()-1));owned=1;
    }

    if (owned) {atomic_store(&owned_buttons[code],1);atomic_fetch_add(&held_buttons,1);}
    step_native_tick();repaint();return owned;
}
int step_native_button(void *event,uint64_t t)
{return step_native_button_at(event,t,milliseconds());}
void step_native_pad(unsigned pad,uint64_t t)
{
    if (!on_ui() || pad>=16) return;
    step_native_tick();if (!current(t)) return;
    if (menu==3 && (t&T_PICKER)) {
        ++picker_probe_serial;picker_probe("PAD_BUSY",pad);
        int valid=(picker_mode!=8||picker_base+(int)pad<=picker_slice_count)&&(picker_mode!=1||picker_base+(int)pad<=120);
            if(valid){if(picker_mode==1)picker_toggle(pad);else picker_select(pad);picker_probe("PAD_SELECTED",pad);audition_picker(pad);}step_native_tick();repaint();
        picker_probe(picker_preview_wait?"AUDIO_WAIT":"PAD_DONE",pad);
        return;
    }
    if (!steps || !(t&T_STEPS) || menu) return;
    unsigned target_sound=(unsigned)(t>>T_SOUND_SHIFT)&127u;
    int target_grid=(int)((t>>T_GRID_SHIFT)&15u),target_page=(int)((t>>T_PAGE_SHIFT)&65535u);
    if (target_grid>=10) return;
    if (t&T_SHIFT) return;
    if (t&T_SOUND_SELECT) {sound=(int)(((t>>T_BANK_SHIFT)&7u)*16+pad);message[0]=0;step_native_tick();repaint();return;}
    unsigned slot=(unsigned)pad_step(pad);
    int step=target_page*16+(int)slot;
    int64_t time=(int64_t)step*grid_ticks[target_grid];
    if (time>=length) return;
    struct slot target[16];
    if (!scan_page(target_page,(int)target_sound,target_grid,target)) {say("Too many events on page");return;}
    sound=(int)target_sound;grid=target_grid;hold(step);
    /* Mute is a UI-owned gesture processed in FIFO order, just like Write.
     * A producer ticket captured just after release may still carry the old
     * held bit; do not apply it after the queued release has been processed. */
    if (!(t&T_ERASE) && atomic_load(&mute_active)){
        mute_used=1;
        if(target[slot].count){
            struct add_context c={0};c.page=target_page;c.sound=(int)target_sound;c.grid=target_grid;
            c.slot=(int)slot;c.expected=target[slot].count;c.mute_change=1;
            c.mute_value=target[slot].muted<target[slot].count;
            if(run_set_command(&c))say(c.mute_value?"Step notes muted":"Step notes unmuted");
            else say("Native mute failed");
        }else say("Empty step: nothing to mute");
    }
    else if (((t&T_ERASE)||atomic_load(&toggle_armed)) && target[slot].count>1) {
        struct add_context c={0};c.page=target_page;c.sound=(int)target_sound;c.grid=target_grid;
        c.slot=(int)slot;c.expected=target[slot].count;
        if (run_set_command(&c)) {report("DELETE_STEP_GROUP native_Undo");say("Step notes removed");}
        else say("Native delete failed");
    }
    else if (target[slot].count) {
        if ((t&T_ERASE)||atomic_load(&toggle_armed)) {remove_event(&target[slot].first);say("Note removed");}
        else say("Step selected");
    }
    else if (!(t&T_ERASE) && (atomic_load(&write_armed)||atomic_load(&toggle_armed))) {
        unsigned note=(unsigned)FN(0x157f800,int(*)(void*,int))(controller,sound);
        struct step_event e=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))
            (note,time,grid_ticks[grid],100,0,velocity/127.0f);
        if (add_event(&e)) {choose_exact(&e);say("Note added");}
    }
    else if (!(t&T_ERASE)) say("Step selected: Write Off");
    step_native_tick();repaint();
}

static void colour(void *g,uint32_t c) { FN(0x102d5e0,void(*)(void*,uint32_t))(g,c); }
static void rect(void *g,int x,int y,int w,int h)
{ FN(0x102e000,void(*)(void*,int,int,int,int))(g,x,y,w,h); }
static void text_align(void *g,const char *s,int x,int y,int w,int h,float height,int alignment)
{
    uint64_t string=0,font=0;FN(0xf535c0,void(*)(uint64_t*,const char*))(&string,s);
    FN(0x1082810,void(*)(uint64_t*,int,float))(&font,0,height);
    FN(0x102dca0,void(*)(void*,const uint64_t*))(g,&font);
    FN(0x10817b0,void(*)(void*,const uint64_t*,int,int,int,int,int,int,float))(g,&string,x,y,w,h,alignment,1,.75f);
    FN(0x1070d60,void(*)(uint64_t*))(&font);FN(0xf0a290,void(*)(uint64_t*))(&string);
}
static void text(void*g,const char*s,int x,int y,int w,int h,float height){text_align(g,s,x,y,w,h,height,0x24);}
/* Shift badge as the native K panel draws it (values_ui.S): a 10x12 frame
 * with "S", white, red while Shift is held. bx = column*106+94, top = y. */
static void shift_badge(void *g,int bx,int top,int on)
{
    colour(g,on ? 0xffff4747 : 0xffffffff);
    rect(g,bx,top,10,1); rect(g,bx,top+11,10,1); rect(g,bx,top,1,12); rect(g,bx+9,top,1,12);
    text(g,"S",bx+1,top+1,8,10,10.0f);
}
static void left_text(void*g,const char*s,int x,int y,int w,int h,float height){text_align(g,s,x,y,w,h,height,0x21);}
static void right_text(void*g,const char*s,int x,int y,int w,int h,float height){text_align(g,s,x,y,w,h,height,0x22);}
static void outline(void *g,int x,int y,int w,int h)
{rect(g,x,y,w,1);rect(g,x,y+h-1,w,1);rect(g,x,y,1,h);rect(g,x+w-1,y,1,h);}
static void cell(void *g,int x,const char *name,const char *value,int selected,int index,int sections)
{
    colour(g,selected ? AMBER : WHITE);text(g,name,x+3,199,100,13,12);text(g,value,x+3,213,100,19,18);
    if (selected) {rect(g,x,198,106,2);rect(g,x,198,2,32);rect(g,x+104,198,2,32);}
    if (!sections) {
        colour(g,0xff373737);rect(g,x+4,234,98,3);
        colour(g,selected ? AMBER : WHITE);rect(g,x+4,234,clamp(index,0,127)*98/127,3);
    } else for(int i=0;i<sections;++i){colour(g,i==index ? (selected ? AMBER : WHITE) : 0xff373737);rect(g,x+4+i*98/sections,234,98/sections-2,2);}
}
static void steps_grid_paint(void*g)
{
    char s[24];
    /* LEDs use physical pad indices; cursor, slots and step numbers use
     * chronological indices. Convert at this boundary just as palette()
     * does, so the outline and fill identify the same physical pad. */
    for(int row=0;row<4;++row)for(int col=0;col<4;++col){
        int i=(3-row)*4+col,x=4+col*79,y=82+row*28,w=75,h=25;
        int slot=pad_step((unsigned)i);
        uint64_t light=atomic_load(&lights[i]);uint32_t rgb=(uint32_t)light;float bright;uint32_t bits=(uint32_t)(light>>32);memcpy(&bright,&bits,4);
        if(!(bright>=0.f&&bright<=1.f))bright=0.f;
        int lit=bright>=.2f&&rgb;
        int valid=page*16+slot<count();
        uint32_t fill=valid?0xff242424:0xff070707;
        if(lit){float k=bright<.55f?.55f:bright;
            fill=0xff000000u|((uint32_t)(((rgb>>16)&255)*k)<<16)|((uint32_t)(((rgb>>8)&255)*k)<<8)|(uint32_t)((rgb&255)*k);}
        colour(g,fill);rect(g,x,y,w,h);
        int step=page*16+slot+1;snprintf(s,sizeof(s),"%d",step);
        unsigned luma=((fill>>16)&255)*3+((fill>>8)&255)*6+(fill&255);
        colour(g,!valid?0xff353535:lit&&luma>1100?0xff000000:scan_ok&&slots[slot].count?WHITE:GREY);text(g,s,x,y+1,w,h-2,14);
        if(cursor_visible&&cursor/16==page&&cursor%16==slot){colour(g,WHITE);outline(g,x-1,y-1,w+2,h+2);outline(g,x-2,y-2,w+4,h+4);}
    }
}
void step_native_paint(void *p,void *g)
{
    if (!ui_known) {ui_thread=pthread_self();ui_known=1;}
    if (!on_ui() || FIELD(p,0,uintptr_t)!=base+SCREEN_VPTR) return;
    if (atomic_exchange(&dead,0)) {root=NULL;controller=NULL;steps=menu=0;reset_mute();invalidate();}
    if (root!=p) {
        root=p;atomic_store(&root_identity,(uintptr_t)p);steps=want_steps;menu=0;invalidate();
        report("STEP_SCREEN_OBSERVED r13g");
    }
    if (!step_native_tick() || !showing) return;
    if (!steps) {
        colour(g,0xff000000);rect(g,107,5,106,28);colour(g,WHITE);text(g,"Steps",109,6,102,22,14);
        colour(g,0xff000000);rect(g,213,5,107,28);colour(g,shift?WHITE:RED);text(g,shift ? "Copy/Paste" : "Erase",215,6,103,22,14);
        if (message[0] && milliseconds()<message_until) {
            colour(g,0xff000000);rect(g,4,168,312,26);colour(g,AMBER);outline(g,4,168,312,26);text(g,message,8,171,304,20,12);
        }
    } else {
        /* Leave the native sequence-progress strip at y0..4 untouched. */
        colour(g,0xff000000);rect(g,0,5,320,235);
        char bank_title[16];snprintf(bank_title,sizeof(bank_title),"Bank %c",'A'+browse_bank);
        /* Header: B1-B3 as blocks (same as Chords). */
        const char*heads[3]={atomic_load(&requested_sound)?bank_title:"Sound",shift?"Follow":"Events","Edit"};
        for(int i=0;i<3;++i){int x=i*107;colour(g,0xff202020);rect(g,x+1,6,105,26);
            colour(g,i==0&&atomic_load(&requested_sound)?AMBER:i==1&&shift&&follow?AMBER:WHITE);text(g,heads[i],x+1,7,105,24,14);}
        char s[64],name[8];pad_name(sound,name);
        /* Info: sound and sample, mode on the right; then step, positions. */
        int pad_colour=music_pad_colour((unsigned)sound);
        colour(g,pad_colour?0xff000000u|music_identity_rgb(pad_colour):AMBER);left_text(g,name,6,35,40,22,17);
        ui_caption(s,sizeof(s),sample_name,18);colour(g,!strcmp(sample_name,"Empty") ? GREY : WHITE);left_text(g,s,46,35,170,22,15);
        colour(g,atomic_load(&requested_erase)?0xffff8000:atomic_load(&mute_active)?RED:atomic_load(&toggle_armed)?AMBER:atomic_load(&write_armed) ? RED : GREY);
        right_text(g,atomic_load(&requested_erase)?"ERASE":atomic_load(&mute_active)?"MUTE":atomic_load(&toggle_armed)?"TOGGLE":atomic_load(&write_armed)?"WRITE ON":"WRITE OFF",216,35,98,22,13);
        struct bbt e=position(edit_tick),play=position(play_tick<0 ? 0 : play_tick);
        unsigned kinds=atomic_load(&event_edit_mask);
        int cursor_muted=scan_ok&&cursor/16==page&&slots[cursor%16].muted;
        if(message[0]&&milliseconds()<message_until){colour(g,AMBER);ui_caption(s,sizeof(s),message,39);left_text(g,s,6,58,308,20,13);}
        else{
            colour(g,AMBER);snprintf(s,sizeof(s),"Step %d%s%s  %d.%d.%03d",cursor+1,kinds&2u?" C":"",kinds&1u?" L":"",e.bar+1,e.beat+1,e.tick);left_text(g,s,6,58,150,20,15);
            colour(g,cursor_muted?RED:GREY);snprintf(s,sizeof(s),"%s%s",follow?"Follow":"Hold",cursor_muted?" M":"");text(g,s,156,58,60,20,13);
            colour(g,was_playing?CYAN:GREY);
            if(was_playing)snprintf(s,sizeof(s),"%d.%d.%03d",play.bar+1,play.beat+1,play.tick);else snprintf(s,sizeof(s),"Page %d/%d",page+1,pages());
            right_text(g,s,216,58,98,20,14);
        }
        steps_grid_paint(g);
        char value[24];
        int bars=clamp(sequence?FIELD(sequence,0x200,int):1,1,128);
        if(shift)snprintf(value,sizeof(value),"--");else snprintf(value,sizeof(value),"%d",bars);
        cell(g,0,shift ? "--" : "Bars",value,!shift&&focus==1,shift?0:bars-1,shift?1:0);
        cell(g,106,shift ? "Triplet" : "Grid",shift ? (grid>=5 ? "On" : "Off") : grid_names[grid],focus==2,shift ? grid>=5 : grid%5,shift ? 2 : 5);
        int vel=scan_ok && cursor/16==page && slots[cursor%16].count==1
            ? (int)lroundf(FIELD(&slots[cursor%16].first,0x10,float)*127) : velocity;
        if(shift)cell(g,213,"Other Sounds",show_other_sounds?"On":"Off",focus==3,show_other_sounds,2);
        else{snprintf(value,sizeof(value),"%d",vel);cell(g,213,"Velocity",value,focus==3,vel,0);}
        shift_badge(g,94,200,shift);shift_badge(g,200,200,shift);shift_badge(g,306,200,shift);
    }
    if (menu) {
        colour(g,0xff000000);rect(g,0,5,320,235);
        {const char*heads[3]={"Back","","Do It"};for(int i=0;i<3;++i){int x=i*107;colour(g,0xff202020);rect(g,x+1,6,105,26);colour(g,WHITE);text(g,heads[i],x+1,7,105,24,14);}}
        if (menu==3) {
            colour(g,0xff000000);rect(g,0,5,320,235);colour(g,WHITE);
            text(g,"Cancel",0,6,106,23,14);text(g,"Apply",213,6,107,23,14);
            /* Same hierarchy for both pickers: identity, event context,
             * before/after and physical-pad selection. No duplicate pad grid. */
            char title[80],name[8];pad_name(event_pad(&detail_old),name);
            colour(g,AMBER);snprintf(title,sizeof(title),"%s  |  %s",name,picker_chop?"CHOP":"16 LEVELS");
            text(g,title,8,35,304,18,14);
            colour(g,WHITE);text(g,sample_name,8,56,304,24,18);
            colour(g,0xff373737);rect(g,8,85,304,1);
            struct bbt at=position(detail_old.time);
            snprintf(title,sizeof(title),"EVENT %d.%d.%03d  |  Velocity %.0f",at.bar+1,at.beat+1,at.tick,FIELD(&detail_old,0x10,float)*127.f);
            colour(g,GREY);text(g,title,8,92,304,16,11);
            colour(g,AMBER);text(g,picker_insert?"NEW EVENT":"ORIGINAL",8,115,146,13,10);colour(g,CYAN);text(g,"SELECTED",166,115,146,13,10);
            char choice[64],formatted[32];level_text(&detail_old,picker_mode,formatted);
            colour(g,AMBER);text(g,picker_insert?"--":formatted,8,130,146,24,19);
            if(picker_mode==1&&picker_toggle_notes)snprintf(formatted,sizeof(formatted),"%u note%s",picker_note_count,picker_note_count==1?"":"s");
            else level_text(&detail_edit,picker_mode,formatted);
            colour(g,CYAN);text(g,picker_has_choice?formatted:"--",166,130,146,24,19);
            colour(g,0xff373737);rect(g,8,158,304,1);
            int selected_pad=level_nearest(&detail_edit,picker_mode,picker_base);
            colour(g,WHITE);snprintf(choice,sizeof(choice),"%s  |  Pad %d / 16",level_names[picker_mode],selected_pad+1);
            text(g,choice,8,164,304,16,12);
            colour(g,GREY);text(g,picker_mode==1?"Pads: select / deselect | B3: apply":"Pads: preview | B3: apply | B1/B2: cancel",2,183,316,12,10);
            cell(g,0,"Type",picker_chop?"Chop":level_names[picker_mode],!picker_chop&&picker_focus==1,picker_mode,picker_chop?0:10);
            snprintf(choice,sizeof(choice),"%d",selected_pad+1);cell(g,106,"Choice",choice,picker_focus==2,selected_pad,16);
            if(picker_mode==8 || picker_mode==1){
                snprintf(choice,sizeof(choice),"%d..%d",picker_base,clamp(picker_base+15,picker_base,picker_mode==8?picker_slice_count:120));
                cell(g,213,"Range",choice,picker_focus==3,picker_mode==8?(picker_base-1)/16:(picker_base+120)/12,picker_mode==8?(picker_slice_count+15)/16:20);
            }
        } else if(menu==4){
            colour(g,0xff000000);rect(g,0,5,320,235);colour(g,WHITE);
            text(g,"Cancel",0,6,106,23,14);text(g,"Select",213,6,107,23,14);
            text(g,picker_source_kind==15?"Choose Chop event":"Choose Levels event",4,34,312,23,16);
            unsigned first=picker_source_index/4*4;
            for(unsigned i=first;i<picker_source_count && i<first+4;++i){
                const struct step_event *e=&picker_sources[i];
                int mode=picker_source_kind==15?8:event_level_mode(e);
                char row[80],value[32];level_text(e,mode,value);
                struct bbt p=position(e->time);
                snprintf(row,sizeof(row),"%u %d.%d.%03d %s %s v%.0f%s",i+1,p.bar+1,p.beat+1,p.tick,
                         level_names[mode],value,FIELD(e,0x10,float)*127.0f,event_muted(e)?" M":"");
                int y=65+(int)(i-first)*28;
                colour(g,i==picker_source_index?AMBER:WHITE);
                if(i==picker_source_index)outline(g,4,y,312,26);
                text(g,row,8,y+2,304,22,12);
            }
            char status[48];snprintf(status,sizeof(status),"Event %u / %u",picker_source_index+1,picker_source_count);
            colour(g,GREY);text(g,status,4,182,312,18,12);
            text(g,"Encoder / +/-: choose",4,207,312,16,12);
            text(g,"B3 / press: open   B1: cancel",4,224,312,13,10);
        } else if (menu==2) {
            text(g,"Note Length",5,52,310,25,18);text(g,"K3: length",5,122,310,21,15);
            struct step_event e;int have=steps ? cursor/16==page && slots[cursor%16].count==1 : selected_event(&e);
            if (steps && have) e=slots[cursor%16].first;
            char value[40];snprintf(value,sizeof(value),"%lld ticks",(long long)(have ? FIELD(&e,0x28,int64_t) : 0));colour(g,AMBER);text(g,value,5,85,310,25,18);
        } else {
            char a[8],b[8],s[48];pad_name(sound,a);pad_name(clip_valid ? clip_sound : sound,b);
            for(int i=0;i<4;++i) {
                if(i==0) snprintf(s,sizeof(s),"Copy event");
                else if(i==1){ if(clip_valid) snprintf(s,sizeof(s),"Paste %s",b); else snprintf(s,sizeof(s),"Paste (nothing copied)"); }
                else if(i==2) snprintf(s,sizeof(s),"Paste to %s",a);
                else snprintf(s,sizeof(s),"Note Length");
                colour(g,i==menu_index ? AMBER : (i==1 || i==2) && !clip_valid ? GREY : WHITE);
                if(i==menu_index) outline(g,10,49+i*32,300,29);
                text(g,s,14,51+i*32,292,25,16);
            }
            colour(g,GREY);text(g,"Encoder: choose  |  Do It / press: run",4,199,312,20,12);
        }
    }
}
/* Native ListBoxModel columns are 24-byte records: Identifier, width,
 * alignment, normal ink, selected ink (15c2d44..15c2fd4). Change only the
 * first column during this row's own native paint, then restore the shared
 * style. Native scrolling, background, C/L text and other columns stay native. */
static int event_name_pad(const char*s)
{
    if(!s)return -1;
    if(!strncmp(s,"Pad ",4))s+=4;
    size_t n=strnlen(s,16);
    if(n<3||n==16||s[0]<'A'||s[0]>'H'||s[1]<'0'||s[1]>'1'||s[2]<'0'||s[2]>'9'||(s[3]&&s[3]!=' '))return -1;
    int number=(s[1]-'0')*10+s[2]-'0';
    return number>=1&&number<=16?(s[0]-'A')*16+number-1:-1;
}
static void event_row_paint(void*model,int row,void*g,int width,int height,int selected)
{
    uint32_t *inks=NULL,normal=0,chosen=0;
    if(music_pad_lighting_mode()&&root&&model&&row>=0&&!atomic_load(&dead)&&music_pad_colours_used()
        &&FIELD(model,0xa0,void*)==(char*)root+0x11c8
        &&FIELD(root,0,uintptr_t)==base+SCREEN_VPTR&&model==FIELD(root,0x1288,void*)){
        uintptr_t first=FIELD(model,0x20,uintptr_t),end=FIELD(model,0x28,uintptr_t);
        if(first&&end>first&&end-first<=32*24&&(end-first)%24==0){
            uint64_t label=0;step_table_cell_string(model,row,(void*)first,&label,base+0x3b774b0);
            int pad=event_name_pad((const char*)(uintptr_t)label);
            int colour=pad>=0?music_pad_colour((unsigned)pad):0;
            FN(0xf0a290,void(*)(uint64_t*))(&label);
            if(colour){inks=(uint32_t*)(first+16);normal=inks[0];chosen=inks[1];inks[0]=inks[1]=0xff000000u|music_identity_rgb(colour);}
        }
    }
    FN(0x3b72210,void(*)(void*,int,void*,int,int,int))(model,row,g,width,height,selected);
    if(inks){inks[0]=normal;inks[1]=chosen;}
}
int step_native_install(void (*paint)(void*,void*))
{
    uintptr_t *slot=(uintptr_t*)(base+SCREEN_VPTR+0xd8);
    uintptr_t *row_slot=(uintptr_t*)(base+0x706d840);
    if (*slot!=base+0x10e2e10 || *row_slot!=base+0x3b72210 || sysconf(_SC_PAGESIZE)!=4096) return 0;
    void *p=(void*)((uintptr_t)slot&~(uintptr_t)4095);
    void *rows=(void*)((uintptr_t)row_slot&~(uintptr_t)4095);
    if(mprotect(p,4096,PROT_READ|PROT_WRITE)) return 0;
    if(mprotect(rows,4096,PROT_READ|PROT_WRITE)){if(mprotect(p,4096,PROT_READ))_exit(121);return 0;}
    *slot=(uintptr_t)paint;
    *row_slot=(uintptr_t)event_row_paint;
    if(mprotect(rows,4096,PROT_READ)) _exit(121);
    if(mprotect(p,4096,PROT_READ)) _exit(121);
    return 1;
}
