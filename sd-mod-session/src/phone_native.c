#define _GNU_SOURCE
#include "phone_input.h"
#include "phone_events.h"
#include "phone_hooks.h"
#ifdef MPC_COMPRESSOR
#include "compressor.h"
#endif
#ifdef MPC_FAST_RESAMPLE
#include "resample_fast_native.h"
#endif
#include "note_native.h"
#include "music_settings.h"
#include "music_archive.h"
#ifdef MPC_MIXER_ARCHIVE
#include "mixer_archive.h"
#endif
#include "music_ui.h"
#ifdef MPC_PAD_EQ
#include "filter_spectrum.h"
#endif
#include "project_exit.h"
#include "values_bipolar.h"
#ifdef MPC_POWER_BUTTON
#include "power_control.h"
#endif
#ifdef MPC_POLY_RECORD
#include "poly_record.h"
#include "voice_guard.h"
#include "pad_eq.h"
#endif
#include "music_lighting.h"
#ifdef MPC_CHORDS
#include "chords.h"
#include "chords_ui.h"
#include "takeover_native.h"
#endif
#include "step_native.h"
#include "workflow_ui.h"
#include "batch_copy.h"
#ifdef MPC_RESAMPLE_TRIAL
#include "resample_ui.h"
#endif
#ifdef MPC_PAD_EQ
#include "pad_eq_native.h"
#include "pad_eq_history.h"
#endif
#include <link.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

/* All editor calls run on the thread which paints the naming screen. No audio
 * thread is allowed to touch a Component or the text model. Firmware is pinned
 * by the launcher; the bridge table is additionally checked before activation. */
static uintptr_t base;
static void *editor, *screen;
static pthread_t ui_thread;
static _Atomic int ui_known;
static _Thread_local int internal;
static int timer_created, timer_running;
static struct phone_input state;
static _Atomic unsigned consumed, passed;
static _Atomic unsigned consumed_step,consumed_workflow;
#ifdef MPC_RESAMPLE_TRIAL
static _Atomic unsigned consumed_resample,resample_sequence_pending;
static _Alignas(8) unsigned char resample_entry_down[40],resample_entry_up[40];
static unsigned resample_entry_mask;
static uint64_t resample_entry_ticket;
static void resample_sequence_tick(void);
static void resample_pad_ui(const struct phone_event*e){
    if(e->pad==10){
        if(e->workflow_ticket!=resample_entry_ticket){resample_entry_mask=0;resample_entry_ticket=e->workflow_ticket;}
        if(e->down){memcpy(resample_entry_down,e->native,16);memcpy(resample_entry_down+16,e->midi,24);resample_entry_mask|=1u;}
        if(e->up){memcpy(resample_entry_up,e->native,16);memcpy(resample_entry_up+16,e->midi,24);resample_entry_mask|=2u;}
    }
    resample_ui_pad(e->pad,(int)e->down,e->workflow_ticket);
}
#endif
static int resample_running(void){
#ifdef MPC_RESAMPLE_TRIAL
    return resample_ui_active();
#else
    return 0;
#endif
}
static _Atomic unsigned shortcut_pads;
static unsigned shortcut_forwarded; /* UI only; pairing survives screen changes. */
static _Atomic unsigned led_enabled, led_numeric, led_uppercase;
static _Atomic unsigned led_pressed;
static uint64_t feedback_until[16]; /* UI only; minimum visible pulse for quick taps. */
#define FEEDBACK_MIN_MS 100
static _Atomic unsigned naming_active, input_fault;
static _Atomic unsigned requested_pads;
static _Atomic unsigned ui_dispatching;
static _Atomic uint64_t generation, consumed_generation[16];
static _Atomic(uintptr_t) editor_identity;
static _Atomic int editor_destroyed;
static _Atomic(uintptr_t) sender;
static _Atomic uint64_t original_lights[16];
static _Atomic unsigned light_mask;
static _Atomic unsigned log_count;
struct native_timer { uintptr_t *vtable; int64_t id; int32_t interval; int32_t padding; };
static struct native_timer timer;
static unsigned process_events(void);
static void queue_failed(void);
static void send_step_lights(void);
static void send_record_light(void);
static _Atomic(uintptr_t) button_sender;
static _Atomic unsigned native_button_lights[128];
/* Native LED state of a button (Knob FX lit = its knob overlay is active). */
unsigned phone_native_button_light(unsigned code){return code<128?atomic_load(&native_button_lights[code]):0;}

#define FN(off,type) ((type)(base + (off)))
#define ORIG(id,type) FN(phone_hooks[id].trampoline, type)
#define FIELD(p,off,type) (*(type *)((char *)(p)+(off)))

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static void note(const char *message)
{
    /* Bounded, RAM-only; no names or samples in the diagnostic. */
    if (log_count++ < 64) fprintf(stderr, "PHONE_INPUT %s\n", message);
}

/* Match a native button-down with its native release across a workflow
 * transition. A release is not an action owned by the newly opened window. */
static _Atomic unsigned char native_buttons_down[112],gesture_released[112];
static _Atomic unsigned native_shift_requested,native_mute_mode;
static void observe_mute_indicator(unsigned value){
 /* Shift uses the Mute LED as a secondary-function hint. Update on actual
  * LED notifications, never sample a stale hint at the next UI tick. */
 if(!atomic_load(&native_shift_requested)&&!atomic_load(&native_buttons_down[0x31]))
  atomic_store(&native_mute_mode,value>=2);
}
static pthread_mutex_t gesture_mutex=PTHREAD_MUTEX_INITIALIZER;
static unsigned char gesture_down_events[2][16];
#ifdef MPC_POWER_BUTTON
static unsigned char power_stop_template[16];
static unsigned power_stop_ready;
int power_control_operation_busy(void){return workflow_jobs_busy()||resample_ui_shutdown_busy();}
#endif
static int on_ui(void);
static void forward_buttons(void*event){
 unsigned code=FIELD(event,8,unsigned char),down=FIELD(event,9,unsigned char)!=0;
 if(code==0x51&&down)workflow_button(event,0); /* Stop preview, then native sequence Stop. */
 if(code<112){
  if(!down&&atomic_exchange(&gesture_released[code],0))return;
  if(down&&(code==0x31||code==0x09)){pthread_mutex_lock(&gesture_mutex);memcpy(gesture_down_events[code==0x31?0:1],event,16);pthread_mutex_unlock(&gesture_mutex);}
  atomic_store(&native_buttons_down[code],(unsigned char)down);
 }
 /* The native app handles buttons on its MIDI input (audio) thread. Queued
  * buttons reach it from the UI thread instead; hold the render lock so its
  * engine work (Stop flushing events and voices) never overlaps a render. */
#ifdef MPC_CHORDS
 int guard=on_ui();if(guard)voice_guard_lock();
#endif
 ORIG(H_BUTTONS,void(*)(void*))(event);
#ifdef MPC_CHORDS
 if(guard)voice_guard_unlock();
#endif
}
#ifdef MPC_RESAMPLE_TRIAL
static void resample_sequence_tick(void){
    if(resample_ui_take_sequence())atomic_store(&resample_sequence_pending,1);
    if(!atomic_load(&resample_sequence_pending)||resample_entry_mask!=3u)return;
    /* Replay the exact captured stock Shift+pad11 gesture only after owner
     * cleanup. No invented engine-mode id or record/transport command. */
    unsigned char shift_event[16];int injected=!atomic_load(&native_buttons_down[0x31]);
    if(injected){
        pthread_mutex_lock(&gesture_mutex);memcpy(shift_event,gesture_down_events[0],16);pthread_mutex_unlock(&gesture_mutex);
        if(shift_event[8]!=0x31){note("RESAMPLE_SEQ_MISSING_SHIFT_GESTURE");atomic_store(&resample_sequence_pending,0);resample_entry_mask=0;return;}
        shift_event[9]=1;forward_buttons(shift_event);
    }
    ORIG(H_PAD,void(*)(void*))(resample_entry_down);
    ORIG(H_PAD,void(*)(void*))(resample_entry_up);
    if(injected){shift_event[9]=0;forward_buttons(shift_event);}
    atomic_store(&resample_sequence_pending,0);resample_entry_mask=0;note("RESAMPLE_NATIVE_SEQ_GESTURE_HANDOFF");
}
#endif
/* On B2, hand the entry chord back to native controls immediately. The
 * two native release callbacks retain Copy's modal host until our own Exit. */
void phone_release_copy_gesture(void){
 const unsigned codes[2]={0x31,0x09};
 for(unsigned i=0;i<2;++i)if(atomic_load(&native_buttons_down[codes[i]])){
  unsigned char event[16];pthread_mutex_lock(&gesture_mutex);memcpy(event,gesture_down_events[i],16);pthread_mutex_unlock(&gesture_mutex);
  event[9]=0;forward_buttons(event);atomic_store(&gesture_released[codes[i]],1);
  note_native_button_requested(codes[i],0);step_native_requested(codes[i],0);
 }
 note("BATCH_ENTRY_GESTURE_RELEASED");
}
static void workflow_buttons(void*event,uint64_t ticket){
 unsigned code=FIELD(event,8,unsigned char),down=FIELD(event,9,unsigned char)!=0;
 int handled=workflow_button(event,ticket);
 if(!handled){forward_buttons(event);return;}
 if(!down&&code<112&&atomic_load(&native_buttons_down[code])){
  note(code==0x09?"WORKFLOW_PAIRED_ERASE_RELEASE":"WORKFLOW_PAIRED_NATIVE_RELEASE");
  forward_buttons(event);
 }
}

static int on_ui(void) { return ui_known && pthread_equal(ui_thread, pthread_self()); }

static int valid_editor(void *p)
{
    if (!p || FIELD(p,0,uintptr_t) != base + 0x6e18e20) return 0;
    int max = FIELD(p,0x2c8,int);
    uintptr_t begin = FIELD(p,0x2e0,uintptr_t), end = FIELD(p,0x2e8,uintptr_t);
    return max == 128 && begin && end >= begin && end - begin >= 128 * 8;
}

static int visible(void)
{
    if (!on_ui() || !editor || !screen || !valid_editor(editor)) return 0;
    return FN(0x112fd50,int(*)(void*))(editor)
        && !FN(0x112f6b0,int(*)(void*))(editor);
}

static int read_native(void)
{
    int cursor = FIELD(editor,0x2c0,int);
    const char *text = FIELD(editor,0x2d0,const char*);
    if (cursor < 0 || cursor >= 128 || !text || strnlen(text, 513) > 512) return 0;
    return phone_sync(&state, text, (size_t)cursor);
}

static void repaint(void)
{
    if (!screen) return;
    uint64_t size = FIELD(screen,0x28,uint64_t);
    FN(0x1149880,void(*)(void*,uint64_t,uint64_t))(screen,0,size);
}

static void set_cursor(void)
{
    if (!valid_editor(editor)) return;
    size_t caret=state.enabled&&state.pending?state.pending_position+1:state.cursor;
    int pos = caret < 128 ? (int)caret : 127;
    FIELD(editor,0x2c0,int) = pos;
    uintptr_t chars = FIELD(editor,0x2e0,uintptr_t);
    int index = FIELD((void*)chars,(size_t)pos * 8,int);
    FIELD(editor,0x2c4,int) = index >= 0 ? index : 0;
    FIELD(editor,0x2d8,unsigned char) = index >= 0
        ? FIELD((void*)chars,(size_t)pos * 8 + 4,unsigned char) : (unsigned char)state.uppercase;
    int range[2] = {pos,pos + !state.enabled};
    FN(0x11685a0,void(*)(void*,const int*))(editor,range);
}

static void write_native(void)
{
    char text[PHONE_MAX_CHARS * 4 + 1];
    uint64_t str = 0;
    phone_utf8(&state,text,sizeof(text));
    ++internal;
    FN(0xf535c0,void(*)(uint64_t*,const char*))(&str,text);
    FN(0x11c4ec0,void(*)(void*,const uint64_t*,int))(editor,&str,1);
    ORIG(H_REBUILD,void(*)(void*))(editor);
    FN(0xf0a290,void(*)(uint64_t*))(&str);
    set_cursor();
    --internal;
}

static void restore_lights(void)
{
    uintptr_t s = atomic_load(&sender);
    unsigned mask = atomic_load(&light_mask);
    if (!s) return;
    for (unsigned i = 0; i < 16; ++i) if (mask & (1u << i)) {
        uint32_t colour = (uint32_t)atomic_load(&original_lights[i]);
        ORIG(H_RGB,void(*)(void*,uint32_t,unsigned))((void*)s,colour,i);
    }
}

static unsigned feedback_mask(uint64_t when)
{
    unsigned mask = state.held;
    for (unsigned i=0; i<16; ++i) {
        if (when < feedback_until[i]) mask |= 1u << i;
        else feedback_until[i]=0;
    }
    return mask;
}

static void send_lights(unsigned mask)
{
    uintptr_t s = atomic_load(&sender);
    if (!s) return;
    struct phone_input colours = {0};
    colours.numeric = (int)atomic_load(&led_numeric);
    colours.uppercase = (int)atomic_load(&led_uppercase);
    colours.held = atomic_load(&led_pressed);
    for (unsigned i=0; i<16; ++i) if (mask & (1u<<i)) {
        uint64_t light = phone_light(&colours,i+1);
        uint32_t colour = FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&light);
        ORIG(H_RGB,void(*)(void*,uint32_t,unsigned))((void*)s,colour,i);
    }
}

static void feedback_refresh(uint64_t when)
{
    if (!state.enabled) return;
    unsigned pressed = feedback_mask(when);
    unsigned changed = atomic_exchange(&led_pressed,pressed) ^ pressed;
    if (changed) send_lights(changed);
}

static void lights(void)
{
    atomic_store(&led_numeric, (unsigned)state.numeric);
    atomic_store(&led_uppercase, (unsigned)state.uppercase);
    atomic_store(&led_enabled, (unsigned)state.enabled);
    if (!state.enabled) {
        atomic_store(&led_pressed,0);
        memset(feedback_until,0,sizeof(feedback_until));
        restore_lights(); return;
    }
    atomic_store(&led_pressed,feedback_mask(now_ms()));
    send_lights(0xffff);
}

static void stop(void)
{
    atomic_store(&naming_active,0);
    atomic_store(&requested_pads,0);
    atomic_fetch_add(&generation,1);
    phone_enable(&state,0);
    atomic_store(&led_enabled,0);
    atomic_store(&led_pressed,0);
    memset(feedback_until,0,sizeof(feedback_until));
    atomic_store(&input_fault,0);
    atomic_store(&editor_identity,0);
    restore_lights();
    editor = 0;
    screen = 0;
    /* consumed survives: release of a typing pad must not reach the engine. */
}

static void levels_state_tick(void)
{
    unsigned enabled=0,mode=0,origin=0;
    int found=music_lighting_levels_state(&enabled,&mode,&origin);
    step_native_levels_state(enabled,mode,origin);
    static unsigned previous=~0u,reports;
    unsigned current=enabled|(mode<<8)|(origin<<16)|((unsigned)found<<24);
    if(current!=previous){previous=current;if(reports++<32)
        fprintf(stderr,"LEVELS_STATE live=%d enabled=%u mode=%u origin=%u\n",found,enabled,mode,origin);}
}
static void tick(void *unused)
{
    (void)unused;
    if (!on_ui()) { atomic_store(&requested_pads,0); atomic_store(&input_fault,1); return; }
    if (atomic_exchange(&editor_destroyed,0)) stop();
    /* Publish the page/ownership resulting from a queued action before
     * allowing native input to bypass this batch. Nested ticks count too. */
    atomic_fetch_add(&ui_dispatching,1);
    unsigned action_batch=process_events();
    if(!action_batch)atomic_fetch_sub(&ui_dispatching,1);
#ifdef MPC_POWER_BUTTON
    if(power_control_take_stop()){
        unsigned char e[16];unsigned ready;
        pthread_mutex_lock(&gesture_mutex);memcpy(e,power_stop_template,16);ready=power_stop_ready;pthread_mutex_unlock(&gesture_mutex);
        if(ready){e[8]=0x51;e[9]=1;forward_buttons(e);e[9]=0;forward_buttons(e);}
    }
    power_control_tick();
#endif
    levels_state_tick();
    int note_running=note_native_tick();
    int music_running=music_ui_tick();
    int step_running=step_native_tick();
    int workflow_running=workflow_tick();
#ifdef MPC_COMPRESSOR
    int compressor_running=compressor_ui_tick();
#endif
#ifdef MPC_RESAMPLE_TRIAL
    /* music_ui_active is true on the ordinary Sample screen too, so it is
     * not a modal exclusion. Resample owns its controls while open. */
    unsigned transport_state=step_native_transport_state();
    resample_ui_transport(transport_state&1u,transport_state&2u);
    int resample_active=resample_ui_tick(atomic_load(&naming_active)||step_native_active()||workflow_owns(PHONE_PAD,10));
    unsigned char stop_request[16];
    if(resample_ui_take_stop(stop_request)){forward_buttons(stop_request);stop_request[9]=0;forward_buttons(stop_request);}
    resample_sequence_tick();
#else
    int resample_active=0;
#endif
#ifdef MPC_CHORDS
    chords_ui_poll(atomic_load(&naming_active)||step_native_active());
#endif
#ifdef MPC_PAD_EQ
    /* EQ page spectrum: ~15 repaints per second while audio or falling bars. */
    {static uint64_t spectrum_ms;uint64_t t=now_ms();if(t-spectrum_ms>=66&&pad_eq_spectrum_dirty()){spectrum_ms=t;note_native_repaint_sample();}}
#endif
    step_native_native_mute(atomic_load(&native_mute_mode));
    step_native_playback_context(note_native_context(),music_transposition(),
        (note_native_sample_visible()
#ifdef MPC_COMPRESSOR
         ||compressor_running
#endif
        )?1:music_ui_sequence_visible()&&atomic_load(&native_mute_mode)?2:0);
#ifdef MPC_POLY_RECORD
    poly_record_context(note_native_context());
#endif
    send_step_lights();send_record_light();
    music_lighting_tick();
    if(action_batch)atomic_fetch_sub(&ui_dispatching,1);
    if (editor && !visible()) stop();
    if (!editor) {
        if (!note_running && !music_running && !step_running && !workflow_running && !resample_active && !step_native_active() && !phone_events_pending()
#ifdef MPC_COMPRESSOR
            && !compressor_running
#endif
#ifdef MPC_POWER_BUTTON
            && !power_control_busy()
#endif
            && timer_running) {
            FN(0xff99b0,void(*)(void*,int))(&timer,0);
            timer_running = 0;
        }
        return;
    }
    if (atomic_load(&input_fault) == 1) {
        atomic_store(&input_fault,2);
        atomic_store(&requested_pads,0);
        phone_enable(&state,0); lights(); repaint();
        note("UI_QUEUE_FAILED encoder_fallback");
    }
    if (state.enabled && phone_tick(&state,now_ms())) {
        ++internal; set_cursor(); --internal;
    }
    feedback_refresh(now_ms());
}

static void timer_noop(void *p) { (void)p; }
static uintptr_t timer_vtable[3];

static void start_timer(void)
{
    if (!timer_created) {
        FN(0xff1a00,void(*)(void*))(&timer);
        timer_vtable[0] = (uintptr_t)timer_noop;
        timer_vtable[1] = (uintptr_t)timer_noop;
        timer_vtable[2] = (uintptr_t)tick;
        timer.vtable = timer_vtable;
        timer_created = 1;
    }
    if (!timer_running) {
        FN(0xff9370,void(*)(void*,int))(&timer,50);
        timer_running = 1;
    }
}

#ifdef MPC_POWER_BUTTON
/* The native modal question returns on the JUCE message thread. A screen
 * without custom controls can have stopped our polling timer beforehand. */
int power_control_wake(void)
{
    if (!on_ui()) return 0;
    start_timer();
    return 1;
}
#endif

static void activate(void *p, size_t editor_offset)
{
    void *e = (char*)p + editor_offset;
    if (!valid_editor(e)) return;
    if (!ui_known) { ui_thread = pthread_self(); ui_known = 1; }
    if (!on_ui()) return;
    if (screen != p || editor != e) {
        if (editor) stop();
        screen = p; editor = e;
        atomic_store(&editor_identity,(uintptr_t)e);
        atomic_store(&editor_destroyed,0);
        atomic_fetch_add(&generation,1);
        phone_init(&state,PHONE_MAX_CHARS);
        if (!read_native()) { editor = 0; screen = 0; atomic_store(&editor_identity,0); note("NAMING_TEXT_REJECTED"); return; }
        note("NAMING_READY r10b_UI_FIFO feedback100ms encoder_and_plus_minus_preserved");
    }
    if (visible()) { atomic_store(&naming_active,1); start_timer(); }
}

static int note_footer(void *p,void *g)
{
    if (!ui_known) { ui_thread=pthread_self(); ui_known=1; }
    if (!on_ui()) return 0;
    int painted=note_native_footer(p,g);
    if (painted) start_timer();
    return painted;
}

static void sample_dtor(void *p)
{
#ifdef MPC_RESAMPLE_TRIAL
    resample_ui_destroyed(p);
#endif
    music_ui_destroyed(p);
    workflow_destroyed(p);
    note_native_destroyed(p);
    ORIG(H_SAMPLE_DTOR,void(*)(void*))(p);
}
static void project_dtor(void *p)
{
    music_ui_destroyed(p);
    ORIG(H_PROJECT_DTOR,void(*)(void*))(p);
}
#ifdef MPC_COMPRESSOR
void compressor_phone_wake(void){
    if(!ui_known){ui_thread=pthread_self();ui_known=1;}
    if(on_ui())start_timer();
}
#endif
static void music_sample_paint(void *p,void *g)
{
    if (!ui_known) { ui_thread=pthread_self(); ui_known=1; }
    if (!on_ui()) return;
    music_ui_sample(p,g); workflow_sample_paint(p,g); note_native_sample_overlay(p,g);
#ifdef MPC_RESAMPLE_TRIAL
    resample_ui_paint(p,g);
#endif
#ifdef MPC_POWER_BUTTON
    power_control_paint(p,g);
#endif
    start_timer();
}
static void music_project_paint(void *p,void *g)
{
    if (!ui_known) { ui_thread=pthread_self(); ui_known=1; }
    if (!on_ui()) return;
    music_ui_project(p,g);
#ifdef MPC_POWER_BUTTON
    power_control_paint(p,g);
#endif
    start_timer();
}
static void music_tab_paint(void *p,void *g)
{
    if (!ui_known) { ui_thread=pthread_self(); ui_known=1; }
    if (on_ui()) music_ui_tab(p,g);
}
static void step_paint(void *p,void *g)
{
    if (!ui_known) {ui_thread=pthread_self();ui_known=1;}
    if (!on_ui()) return;
    step_native_paint(p,g);
#ifdef MPC_POWER_BUTTON
    power_control_paint(p,g);
#endif
    start_timer();
}
static void step_dtor(void *p)
{ step_native_destroyed(p);ORIG(H_STEP_DTOR,void(*)(void*))(p); }
static void workflow_files(void*p,void*g){
 if(!ui_known){ui_thread=pthread_self();ui_known=1;}if(on_ui()){workflow_files_paint(p,g);
#ifdef MPC_POWER_BUTTON
 power_control_paint(p,g);
#endif
 start_timer();}
}
static void workflow_copy(void*p,void*g){
 if(!ui_known){ui_thread=pthread_self();ui_known=1;}if(on_ui()){workflow_copy_paint(p,g);
#ifdef MPC_POWER_BUTTON
 power_control_paint(p,g);
#endif
 start_timer();}
}
static void files_dtor(void*p){workflow_destroyed(p);ORIG(H_FILES_DTOR,void(*)(void*))(p);}
static void*files_ctor(void*p,void*services){void*r=ORIG(H_FILES_CTOR,void*(*)(void*,void*))(p,services);workflow_registry_bind(services);workflow_files_constructed(p);return r;}
static void master_volume_ctor(void*p,void*parameter,void*control){ORIG(H_MASTER_VOLUME_CTOR,void(*)(void*,void*,void*))(p,parameter,control);workflow_master_volume_bind(parameter,control);}
static void file_preview(void*p,const int*mode,const void*file){if(mode&&workflow_preview_prepare(p,*mode))ORIG(H_FILE_PREVIEW,void(*)(void*,const int*,const void*))(p,mode,file);}
static void private_preview_worker(void*payload){
 if(workflow_preview_worker(payload)){ORIG(H_PRIVATE_PREVIEW_WORKER,void(*)(void*))(payload);workflow_preview_played(payload);}
 else{
  /*20bd5d0 normally releases this ref at20bd69c. A cancelled queued play
   * must release exactly once too, without setting playing=true. */
  void*reference=FIELD(payload,0,void*);
  __atomic_fetch_sub((unsigned*)((char*)reference+8),1,__ATOMIC_RELEASE);
 }
}
static void copy_dtor(void*p){workflow_destroyed(p);ORIG(H_COPY_DTOR,void(*)(void*))(p);}
static void copy_context_ctor(void*p,void*a,void*b,void*c){ORIG(H_COPY_CONTEXT_CTOR,void(*)(void*,void*,void*,void*))(p,a,b,c);workflow_copy_context(p,a);
#ifdef MPC_RESAMPLE_TRIAL
    resample_ui_bind(p);
#endif
}
static void copy_context_dtor(void*p){
#ifdef MPC_RESAMPLE_TRIAL
    resample_ui_context_dead(p);
#endif
    workflow_copy_context_dead(p);ORIG(H_COPY_CONTEXT_DTOR,void(*)(void*))(p);}
static void now_tick(void *p)
{ ORIG(H_NOW_TICK,void(*)(void*))(p);step_native_clock(p); }
static void audition_ctor(void*p,void*a,void*b,void*c){ORIG(H_AUDITION_CTOR,void(*)(void*,void*,void*,void*))(p,a,b,c);step_native_audition_created(p);}
static void bank_ctor(void *p,void *a,void *b,void *c,void *d,void *e,void *f,void *g)
{
    ORIG(H_BANK_CTOR,void(*)(void*,void*,void*,void*,void*,void*,void*,void*))(p,a,b,c,d,e,f,g);
    step_native_bank_created(p);
}
static void bank_dtor(void *p)
{step_native_bank_destroyed(p);ORIG(H_BANK_DTOR,void(*)(void*))(p);}
static int sixteen_refresh(void *p,unsigned pad)
{
    void *properties=FIELD(p,0xe8,void*);
    if (properties && (FIELD(properties,0x2f8,unsigned char)&1)
        && FIELD(properties,0x88,int)==1) {
        int replace;
        uint64_t light=music_pad_light(music_published(),pad,FIELD(properties,0x228,int),&replace);
        if (replace) {
            /* Enter through the existing SixteenLevelLighting layer. Other
             * layers (press feedback, mute, Step Edit) retain native priority.
             * This is only called by native lighting refresh, never UI polling. */
            FN(0x1a3ffb0,void(*)(void*,unsigned,uint64_t))(FIELD(p,0x38,void*),pad,light);
            return 1;
        }
    }
    return ORIG(H_SIXTEEN_REFRESH,int(*)(void*,unsigned))(p,pad);
}
static int chop_refresh(void *p,unsigned pad)
{
    /* Native Slice lighting stays intact. Mode state is sampled by the UI
     * owner even when native refresh skips this layer on exit. */
    return ORIG(H_CHOP_REFRESH,int(*)(void*,unsigned))(p,pad);
}
static void *sixteen_ctor(void *p,void *a,void *b,void *properties)
{
    void *result=ORIG(H_SIXTEEN_CTOR,void*(*)(void*,void*,void*,void*))(p,a,b,properties);
    music_lighting_created(p);
    return result;
}
static void sixteen_dtor(void *p)
{
    music_lighting_destroyed(p);
    ORIG(H_SIXTEEN_DTOR,void(*)(void*))(p);
}
static void sixteen_process(void *properties,int pad,void *event)
{
    if(batch_event_preview_override(properties,event))return;
    ORIG(H_SIXTEEN_PROCESS,void(*)(void*,int,void*))(properties,pad,event);
    int transpose=music_transposition();
    int original_pad=FIELD(properties,0x228,int);
    if (transpose && FIELD(properties,0x88,int)==1 && original_pad>=0 && original_pad<16) {
        /* Match native signed remainder for sentinel/negative pad indices.
         * DSP reads one atomic snapshot: no allocation, file I/O or mutex. */
        int pitch=(pad%16)-original_pad+transpose;
        FN(0x27e4400,void(*)(void*,int,int,int))((char*)event+0x14,0,0,pitch);
    }
}
static void music_save_base(void *p,void *a)
{ music_archive_save(p,a,ORIG(H_MUSIC_SAVE,void(*)(void*,void*))); }
static void music_save(void *p,void *a)
{
#ifdef MPC_MIXER_ARCHIVE
    mixer_archive_save(p,a,music_save_base);
#else
    music_save_base(p,a);
#endif
}
static void music_load_base(void *p,void *a)
{ music_archive_load(p,a,ORIG(H_MUSIC_LOAD,void(*)(void*,void*))); }
static void music_load(void *p,void *a)
{
#ifdef MPC_RESAMPLE_TRIAL
    resample_ui_project_changing();
#endif
#ifdef MPC_MIXER_ARCHIVE
    mixer_archive_load(p,a,music_load_base);
#else
    music_load_base(p,a);
#endif
}
static int music_read_base(void *p,void *j)
{ return music_archive_read(p,j,ORIG(H_MUSIC_READ,int(*)(void*,void*))); }
static int music_read(void *p,void *j)
{
#ifdef MPC_RESAMPLE_TRIAL
    resample_ui_project_changing();
#endif
#ifdef MPC_MIXER_ARCHIVE
    int result=mixer_archive_read(p,j,music_read_base);
#else
    int result=music_read_base(p,j);
#endif
    if (result) step_native_project_changed();
    return result;
}
static void music_key(void *p,void *a,const void *pair)
{ music_archive_key(p,a,pair,ORIG(H_MUSIC_KEY,void(*)(void*,void*,const void*))); }
static void new_project(void *p,int *choice)
{
    int confirmed=choice && *choice==1;
#ifdef MPC_RESAMPLE_TRIAL
    if(confirmed)resample_ui_project_changing();
#endif
    if (confirmed) step_native_project_changed();
    ORIG(H_NEW_PROJECT,void(*)(void*,int*))(p,choice);
    if (confirmed) {
        music_reset();
#ifdef MPC_MIXER_ARCHIVE
        mixer_archive_reset();
#endif
    }
}

static void draw_text(void *g, const char *text, int y, int h, float height, uint32_t colour)
{
    uint64_t str = 0, font = 0;
    FN(0xf535c0,void(*)(uint64_t*,const char*))(&str,text);
    FN(0x1082810,void(*)(uint64_t*,int,float))(&font,0,height);
    FN(0x102dca0,void(*)(void*,const uint64_t*))(g,&font);
    FN(0x102d5e0,void(*)(void*,uint32_t))(g,colour);
    FN(0x10817b0,void(*)(void*,const uint64_t*,int,int,int,int,int,int,float))
        (g,&str,3,y,100,h,0x24,1,0.75f);
    FN(0x1070d60,void(*)(uint64_t*))(&font);
    FN(0xf0a290,void(*)(uint64_t*))(&str);
}

static void paint_mode(void *p, void *g, size_t editor_offset)
{
    activate(p,editor_offset);
    if (!editor || screen != p) return;
    draw_text(g,"Input (K1)",199,13,12.0f,0xffffffff);
    draw_text(g,atomic_load(&input_fault) ? "Encoder only" : state.enabled ? (state.numeric ? "Pads: 123" : "Pads: ABC") : "Encoder",
              214,20,15.0f,state.enabled ? 0xff00b8ff : 0xffffffff);
}
static void name_cursor_paint(void*p,void*g){
    FN(0x1134ad0,void(*)(void*,void*))(p,g);
    if(!on_ui()||p!=editor||!state.enabled||!visible())return;
    int w=FIELD(p,0x28,int),h=FIELD(p,0x2c,int);
    if(w<24||w>320||h<12||h>100)return;
    /* Same size as the stock name field (about 0.86 of its height). The
     * window of characters is sized from the monospace advance (~0.46 of the
     * font height) so the caret is always inside and nothing gets squeezed. */
    float font_h=h*.86f;if(font_h<16.f)font_h=16.f;if(font_h>56.f)font_h=56.f;
    int columns=(int)((float)(w-14)/(font_h*.47f));if(columns<2)columns=2;
    char display[PHONE_MAX_CHARS*4+8];phone_view(&state,(size_t)columns,display,sizeof(display));
    FN(0x102d5e0,void(*)(void*,uint32_t))(g,0xff000000);
    FN(0x102e000,void(*)(void*,int,int,int,int))(g,3,2,w-6,h-4);
    uint64_t str=0,font=0;FN(0xf535c0,void(*)(uint64_t*,const char*))(&str,display);
    FN(0x1082810,void(*)(uint64_t*,int,float))(&font,0,font_h);
    FN(0x102dca0,void(*)(void*,const uint64_t*))(g,&font);FN(0x102d5e0,void(*)(void*,uint32_t))(g,0xffffffff);
    FN(0x10817b0,void(*)(void*,const uint64_t*,int,int,int,int,int,int,float))(g,&str,7,2,w-14,h-4,0x21,1,1.f);
    FN(0x1070d60,void(*)(uint64_t*))(&font);FN(0xf0a290,void(*)(uint64_t*))(&str);
}
static int install_name_cursor(void){
    uintptr_t*slot=(uintptr_t*)(base+0x6e18e20+0xd8);
    if(*slot!=base+0x1134ad0)return 0;
    void*page=(void*)((uintptr_t)slot&~(uintptr_t)4095);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE))return 0;
    *slot=(uintptr_t)name_cursor_paint;
    if(mprotect(page,4096,PROT_READ))_exit(121);
    return 1;
}

static void sample_paint(void *p,void *g)
{
    ORIG(H_SAMPLE_PAINT,void(*)(void*,void*))(p,g);
    paint_mode(p,g,0xa10);
}
static void project_paint(void *p,void *g)
{
    ORIG(H_PROJECT_PAINT,void(*)(void*,void*))(p,g);
    paint_mode(p,g,0x9f8);
}

static int before_native(void *p)
{
    if (internal || !on_ui() || editor != p || !visible()) return 0;
    if (state.pending) {
        phone_native_boundary(&state);
        ++internal; set_cursor(); --internal;
    } else if (!read_native()) {
        phone_enable(&state,0); queue_failed(); lights(); repaint();
        return 0;
    }
    return 1;
}
static void after_native(int boundary)
{
    if (boundary && editor && !read_native()) {
        phone_enable(&state,0); queue_failed(); lights(); repaint(); note("NATIVE_TEXT_NOT_SUPPORTED");
    }
}

/* Preserve argument registers and call the original exactly once. Wrapping the
 * complete native action, rather than only its inner setText call, keeps cursor
 * movement and character selection in the same order as stock. */
#define WRAP1(name,id) static void name(void *p,int a) { \
    int b=a ? before_native(p) : 0; ++internal; ORIG(id,void(*)(void*,int))(p,a); \
    --internal; after_native(b); }
WRAP1(char_increment,H_CHAR_INCREMENT)
WRAP1(plus_action,H_PLUS)
WRAP1(minus_action,H_MINUS)
WRAP1(scroll_action,H_SCROLL)
WRAP1(right_action,H_RIGHT)
#define WRAP0(name,id) static void name(void *p) { \
    int b=before_native(p); ++internal; ORIG(id,void(*)(void*))(p); \
    --internal; after_native(b); }
WRAP0(clear_action,H_CLEAR)
WRAP0(erase_action,H_ERASE)
static void case_change(void *p,int a)
{
    int b=before_native(p); ++internal;
    ORIG(H_CASE,void(*)(void*,int))(p,a);
    --internal; after_native(b);
}
static void move_action(void *p,int a,int restore_case)
{
    int b=before_native(p); ++internal;
    ORIG(H_MOVE,void(*)(void*,int,int))(p,a,restore_case);
    --internal; after_native(b);
}

static void rebuild(void *p)
{
    ORIG(H_REBUILD,void(*)(void*))(p);
    if (!internal && on_ui() && p == editor) {
        state.pending = 0;
        if (!read_native()) { phone_enable(&state,0); queue_failed(); lights(); }
    }
}

static void editor_dtor(void *p)
{
    if ((uintptr_t)p == atomic_load(&editor_identity)) {
        if (on_ui()) stop();
        else {
            atomic_store(&naming_active,0); atomic_store(&requested_pads,0);
            atomic_fetch_add(&generation,1); atomic_store(&editor_identity,0);
            atomic_store(&editor_destroyed,1);
        }
    }
    ORIG(H_DTOR,void(*)(void*))(p);
}
static void editor_delete(void *p)
{
    if ((uintptr_t)p == atomic_load(&editor_identity)) {
        if (on_ui()) stop();
        else {
            atomic_store(&naming_active,0); atomic_store(&requested_pads,0);
            atomic_fetch_add(&generation,1); atomic_store(&editor_identity,0);
            atomic_store(&editor_destroyed,1);
        }
    }
    ORIG(H_DELETE,void(*)(void*))(p);
}

static void controller_ui(void *event,uint64_t route)
{
    unsigned cc = FIELD(event,8,unsigned char), value = FIELD(event,9,unsigned char);
    if (on_ui() && editor && visible()) {
        if (cc == 0x10) {
            before_native(editor);
            int enable = value >= 64;
            if (atomic_load(&input_fault)) enable = 0;
            if (enable != state.enabled) {
                if (!read_native()) { enable = 0; queue_failed(); }
                phone_enable(&state,enable);
                lights(); repaint();
                note(enable ? "PADS_ON" : "PADS_OFF");
                if (enable && !atomic_load(&sender)) note("RGB_SENDER_NOT_CAPTURED");
            }
            return; /* The previously unused K1 selects the input method. */
        }
        if (cc == 0x64 && state.enabled) {
            phone_move(&state,value>64?(int)value-128:(int)value);
            ++internal;set_cursor();--internal;repaint();return;
        }
        if (cc == 0x64) before_native(editor);
    }
#ifdef MPC_COMPRESSOR
    if(on_ui()&&compressor_ui_owns(PHONE_CONTROLLER,cc)&&compressor_ui_control(event,compressor_ui_ticket()))return;
#endif
#ifdef MPC_CHORDS
    if(on_ui()&&chords_ui_control(event))return;
#endif
    if (on_ui() && step_native_owns(cc) && step_native_control(event,route)) return;
    if (on_ui() && (music_ui_control(event) || note_native_control(event))) return;
    ORIG(H_CONTROLLER,void(*)(void*))(event);
    if (cc == 0x64 && on_ui() && editor && visible()) after_native(1);
}

static void buttons_ui(void *event,uint64_t route)
{
    int naming=on_ui()&&editor&&visible();
    unsigned code=FIELD(event,8,unsigned char),down=FIELD(event,9,unsigned char)!=0;
    if(naming&&state.enabled&&(code==0x36||code==0x37)){
        /* Pads input has one insertion cursor. Native +/- edit their own
         * character-picker cache; mixing that cache with pad/encoder moves
         * can reset the position or replace text. Consume both edges here. */
        if(down){phone_move(&state,code==0x36?1:-1);++internal;set_cursor();--internal;repaint();}
        return;
    }
#ifdef MPC_COMPRESSOR
    if(!naming&&on_ui()&&compressor_ui_owns(PHONE_BUTTON,code)&&compressor_ui_button(event,compressor_ui_ticket()))return;
#endif
#ifdef MPC_CHORDS
    if(!naming&&on_ui()&&chords_ui_button(event))return;
#endif
    if (!naming && on_ui() && step_native_active() && step_native_button(event,route)==1) return;
    if (!naming && on_ui() && note_native_button(event)) return;
    if (!naming && on_ui() && music_ui_button(event)) return;
    int shift_event=FIELD(event,8,unsigned char)==0x31;
    if(naming&&shift_event)state.shift=FIELD(event,9,unsigned char)!=0;
    int boundary = naming && (FIELD(event,9,unsigned char) || shift_event);
    if (boundary && !(shift_event&&state.enabled)) before_native(editor);
    int keep_pad_text=naming&&shift_event&&state.enabled;
    struct phone_input saved_text;
    if(keep_pad_text)saved_text=state;
    forward_buttons(event);
    /* Native Shift also changes naming cursor/case presentation. Rebuild
     * the glyph model from the exact text after BOTH edges, then restore
     * the pad keyboard's cursor. Never run Sample/Lights actions over a
     * visible naming modal. */
    if(naming&&shift_event&&editor&&visible()){
        /* Pad text is authoritative while this keyboard is enabled.
         * Native Shift must not replace it with its character-picker cache. */
        if(keep_pad_text){state=saved_text;write_native();}
        else {++internal;ORIG(H_REBUILD,void(*)(void*))(editor);set_cursor();--internal;}
        repaint();
    }
    if (!naming && on_ui()) { note_native_tick(); music_ui_tick();step_native_tick(); }
    if (boundary && on_ui() && editor && visible()) after_native(1);
}

static void queue_failed(void)
{
#ifdef MPC_RESAMPLE_TRIAL
    resample_ui_fault();
#endif
    step_native_queue_fault();workflow_fault();
    atomic_store(&requested_pads,0);
    atomic_store(&input_fault,1);
}

static int queue_native(void *native,unsigned kind)
{
    struct phone_event e = {0};
    e.kind = kind; e.generation = atomic_load(&generation); e.when_ms = now_ms();
    e.step_ticket=step_native_ticket();e.workflow_ticket=workflow_ticket();
    memcpy(e.native,native,sizeof(e.native));
    /* Ownership is numeric producer state. If a queued root-note movement
     * outlives Tune, drop it rather than applying it to the native pitch. */
    int naming=atomic_load(&naming_active)!=0;
    if (kind==PHONE_CONTROLLER&&!naming) {
        unsigned cc=FIELD(native,8,unsigned char);
        e.pad=(unsigned)note_native_owns(cc) | (music_ui_owns(cc) ? 2u : 0u);
        if (!atomic_load(&naming_active) && step_native_owns(cc)) e.pad|=4u;
    }
    if (!naming&&kind==PHONE_BUTTON && music_ui_button_owns(FIELD(native,8,unsigned char))) e.pad=2u;
    if (!naming&&kind==PHONE_BUTTON && step_native_button_owns(FIELD(native,8,unsigned char))) e.pad|=4u;
    if(!naming&&workflow_owns(kind,FIELD(native,8,unsigned char)))e.pad|=8u;
#ifdef MPC_RESAMPLE_TRIAL
    if(!naming&&resample_ui_owns(kind,FIELD(native,8,unsigned char))){e.pad=16u;e.workflow_ticket=resample_ui_ticket();}
#endif
#ifdef MPC_CHORDS
    if(!naming&&kind==PHONE_BUTTON)chords_ui_button_input(FIELD(native,8,unsigned char),FIELD(native,9,unsigned char)!=0);
    if(!naming&&!(e.pad&24u)&&
       ((kind==PHONE_CONTROLLER&&chords_ui_owns(FIELD(native,8,unsigned char)))||
        (kind==PHONE_BUTTON&&chords_ui_button_owns(FIELD(native,8,unsigned char)))))e.pad=32u;
#endif
#ifdef MPC_COMPRESSOR
    if(!naming&&compressor_ui_owns(kind,FIELD(native,8,unsigned char))){e.pad=64u;e.workflow_ticket=compressor_ui_ticket();}
#endif
    if (phone_events_push(&e)) return 1;
    queue_failed();
    if (e.pad) return 1;
    return 0;
}

static int forward_main_volume(void*event)
{
    if(FIELD(event,8,unsigned char)!=0x13)return 0;
    ORIG(H_CONTROLLER,void(*)(void*))(event);return 1;
}
static void controller(void *event)
{
#ifdef MPC_CHORDS
    { static _Atomic unsigned raw_reports;
      unsigned cc=FIELD(event,8,unsigned char),v=FIELD(event,9,unsigned char);
      if(cc>=0x10&&cc<=0x12&&atomic_fetch_add(&raw_reports,1)<1024)
          fprintf(stderr,"TAKEOVER RAW t=%llu cc=%u value=%u queued=%u replay=%u\n",
              (unsigned long long)now_ms(),cc,v,phone_events_pending(),atomic_load(&ui_dispatching));
    }
#endif
    /* Physical Main Volume is global, including custom search/Batch views.
     * Never queue it behind modal ownership or discard it on a new ticket. */
    if(forward_main_volume(event))return;
#ifdef MPC_POWER_BUTTON
    if(power_control_busy())return;
#endif
    /* Observing Sample/Seq is not ownership of its native parameter knobs.
     * Keep native K1-K3 on the original input thread, without the UI timer's
     * 50ms batching or coalescing. Custom controls still cross to the UI.
     * An outstanding FIFO action is a barrier (page/Shift/ownership may
     * change there); never let a fresh movement overtake that action. */
    unsigned native_cc=FIELD(event,8,unsigned char);
    int custom_owned=atomic_load(&naming_active)||resample_running()||
        note_native_owns(native_cc)||music_ui_owns(native_cc)||
        step_native_owns(native_cc)||workflow_owns(PHONE_CONTROLLER,native_cc)
#ifdef MPC_CHORDS
        ||chords_ui_owns(native_cc)
#endif
#ifdef MPC_COMPRESSOR
        ||compressor_ui_owns(PHONE_CONTROLLER,native_cc)
#endif
        ;
    if(native_cc>=0x10&&native_cc<=0x12&&!custom_owned&&
       !phone_events_pending()&&!atomic_load(&ui_dispatching)){
        ORIG(H_CONTROLLER,void(*)(void*))(event);return;
    }
    if (custom_owned || atomic_load(&ui_dispatching) || atomic_load(&naming_active) || note_native_active() || music_ui_active() || step_native_active() || workflow_active() || resample_running() || phone_events_pending()
#ifdef MPC_COMPRESSOR
        ||compressor_ui_owns(PHONE_CONTROLLER,FIELD(event,8,unsigned char))
#endif
        ) {
        unsigned cc=FIELD(event,8,unsigned char), value=FIELD(event,9,unsigned char);
        /* Route keys immediately after K1 even if its UI event is still queued. */
        if (cc==0x10 && atomic_load(&naming_active))
            atomic_store(&requested_pads,value>=64 && !atomic_load(&input_fault));
        if (queue_native(event,PHONE_CONTROLLER)) return;
    }
    ORIG(H_CONTROLLER,void(*)(void*))(event);
}

static void buttons(void *event)
{
    unsigned native_code=FIELD(event,8,unsigned char),native_down=FIELD(event,9,unsigned char)!=0;
#ifdef MPC_POWER_BUTTON
    pthread_mutex_lock(&gesture_mutex);memcpy(power_stop_template,event,16);power_stop_ready=1;pthread_mutex_unlock(&gesture_mutex);
    if(power_control_button(native_code,(int)native_down)){
        if(!native_down&&native_code<112&&atomic_load(&native_buttons_down[native_code]))forward_buttons(event);
        return;
    }
#endif
    if(native_code==0x31)atomic_store(&native_shift_requested,native_down);
    if(native_code==0x09&&native_down)fprintf(stderr,"ERASE_ROUTE naming=%u workflow_active=%d owns=%d\n",atomic_load(&naming_active),workflow_active(),workflow_owns(PHONE_BUTTON,0x09));
#ifdef MPC_RESAMPLE_TRIAL
    resample_ui_requested(native_code,(int)native_down);
    if(resample_running()&&(native_code==0x51||native_code==0x52)){forward_buttons(event);return;}
#endif
    if(native_code<112&&native_down)atomic_store(&gesture_released[native_code],0);
    /* A modal may reject Note Repeat/Erase while notes are held. Do not
     * publish their requested native state until it actually passes them. */
#ifdef MPC_CHORDS
    int chord_owned=chords_ui_button_owns(native_code);
    if(!chord_owned){
#endif
        note_native_button_requested(native_code,native_down);
        music_ui_requested(native_code,native_down);
        step_native_requested(native_code,native_down);
#ifdef MPC_CHORDS
    }
#endif
    if(native_code<112&&!native_down&&atomic_exchange(&gesture_released[native_code],0))return;
    if ((atomic_load(&naming_active) || note_native_active() || music_ui_active() || step_native_active() || workflow_active() || resample_running() || phone_events_pending()
#ifdef MPC_RESAMPLE_TRIAL
        ||resample_ui_owns(PHONE_BUTTON,native_code)
#endif
#ifdef MPC_CHORDS
        ||chord_owned
#endif
#ifdef MPC_COMPRESSOR
        ||compressor_ui_owns(PHONE_BUTTON,native_code)
#endif
        )
        && queue_native(event,PHONE_BUTTON)) return;
    forward_buttons(event);
}

static void pad_event(void *event)
{
#ifdef MPC_POWER_BUTTON
    if(power_control_busy())return;
#endif
    unsigned pad = FIELD(event,8,unsigned char);
    void *msg = (char*)event + 0x10;
    if (pad >= 16) { ORIG(H_PAD,void(*)(void*))(event); return; }
    unsigned bit = 1u << pad;
    int down = FN(0xddfed0,int(*)(void*,int))(msg,0);
    int up = FN(0xddff20,int(*)(void*,int))(msg,1);
    if (passed & bit) {
        if (up) passed &= ~bit;
        ORIG(H_PAD,void(*)(void*))(event); return;
    }
#ifdef MPC_CHORDS
    if((down||up)&&chords_parameter(pad,down))return;
#endif
    if(music_ui_exit_saving())return;
#ifdef MPC_RESAMPLE_TRIAL
    if((atomic_load(&consumed_resample)&bit)||resample_ui_claim_pad(pad,down,atomic_load(&naming_active)||step_native_active()||workflow_owns(PHONE_PAD,pad)||atomic_load(&resample_sequence_pending))){
        if(down)atomic_fetch_or(&consumed_resample,bit);
        int size=FIELD(msg,0x10,int);
        if((down||up)&&size>0&&size<=8){struct phone_event e={0};e.kind=PHONE_RESAMPLE_PAD;e.pad=pad;e.down=(unsigned)down;e.up=(unsigned)up;e.workflow_ticket=resample_ui_ticket();memcpy(e.native,event,16);memcpy(e.midi,msg,24);if(!phone_events_push(&e))queue_failed();}
        else if(down||up)queue_failed();
        if(up)atomic_fetch_and(&consumed_resample,~bit);
        return;
    }
#endif
    if((atomic_load(&consumed_workflow)&bit)||(!atomic_load(&naming_active)&&workflow_owns(3,pad))){
        if(down)atomic_fetch_or(&consumed_workflow,bit);
        if(down||up){struct phone_event e={0};e.kind=PHONE_WORKFLOW_PAD;e.pad=pad;e.down=(unsigned)down;e.when_ms=now_ms();e.workflow_ticket=workflow_ticket();if(!phone_events_push(&e))queue_failed();}
        if(up)atomic_fetch_and(&consumed_workflow,~bit);
        return;
    }
    unsigned owned = consumed & bit;
    uint64_t route=step_native_ticket();
    int pad_route=step_native_pad_route(route);
    if ((atomic_load(&shortcut_pads)&bit) ||
        (down && !owned && !atomic_load(&naming_active) && (pad_route==2 || pad_route==3))) {
        if (down) atomic_fetch_or(&shortcut_pads,bit);
        int size=FIELD(msg,0x10,int);
        if (size>0 && size<=8) {
            struct phone_event e={0};e.kind=PHONE_STEP_SHORTCUT_PAD;e.pad=pad;
            e.down=(unsigned)down;e.up=(unsigned)up;e.step_ticket=route;
            memcpy(e.native,event,16);memcpy(e.midi,msg,24);
            if (!phone_events_push(&e)) queue_failed();
        } else queue_failed();
        if (up) atomic_fetch_and(&shortcut_pads,~bit);
        return;
    }
    int naming = atomic_load(&naming_active) && atomic_load(&requested_pads);
    int step = !atomic_load(&naming_active) && pad_route==1;
    int active = naming || step;
    if (active || owned) {
        if (down && !owned) {
            atomic_store(&consumed_generation[pad],atomic_load(&generation));
            consumed |= bit;
            if (step) atomic_fetch_or(&consumed_step,bit);
        }
        if ((down||up) && (atomic_load(&consumed_step)&bit) && step) {
            struct phone_event e={0};e.kind=PHONE_STEP_PAD;e.pad=pad;e.down=(unsigned)down;
            e.when_ms=now_ms();e.step_ticket=route;
            if (!phone_events_push(&e)) queue_failed();
        } else if ((down || up) && !(atomic_load(&consumed_step)&bit) && atomic_load(&naming_active)) {
            struct phone_event e = {0};
            e.kind=PHONE_PAD; e.pad=pad; e.down=(unsigned)down; e.when_ms=now_ms();
            e.generation=atomic_load(&consumed_generation[pad]);
            if (!phone_events_push(&e)) queue_failed();
        }
        if (up) {consumed &= ~bit;atomic_fetch_and(&consumed_step,~bit);}
        return; /* Includes poly-pressure; nothing reaches the musical signal. */
    }
    if (down) passed |= bit;
    ORIG(H_PAD,void(*)(void*))(event);
}

static unsigned process_events(void)
{
    static unsigned seen;
    atomic_fetch_add(&ui_dispatching,1);
    struct phone_event e;
    unsigned budget = PHONE_EVENT_CAPACITY,processed=0;
    while (budget-- && phone_events_pop(&e)) {
        ++processed;
        int current = !atomic_load(&editor_destroyed) && editor &&
            e.generation == atomic_load(&generation) && visible();
        if (e.kind == PHONE_CONTROLLER) {
            if (current && !(seen&1)) { seen|=1; note("FIFO_CONTROLLER_ON_UI"); }
            if (e.pad) {
                /* Never replay a claimed metadata edit as native audio pitch. */
#ifdef MPC_COMPRESSOR
                if(e.pad&64u)compressor_ui_control(e.native,e.workflow_ticket);
                else
#endif
#ifdef MPC_CHORDS
                if(e.pad&32u)chords_ui_control(e.native);
                else
#endif
#ifdef MPC_RESAMPLE_TRIAL
                if(e.pad&16u){if(!resample_ui_control(e.native,e.workflow_ticket))ORIG(H_CONTROLLER,void(*)(void*))(e.native);}
                else
#endif
                if (e.pad&8u){if(!workflow_control(e.native,e.workflow_ticket))ORIG(H_CONTROLLER,void(*)(void*))(e.native);}
                else if (e.pad&4u) step_native_control(e.native,e.step_ticket);
                else if (e.pad&2u) music_ui_control(e.native);
                else note_native_control(e.native);
            } else if (current || note_native_active() || music_ui_active() || step_native_active()
#ifdef MPC_COMPRESSOR
                       ||compressor_ui_owns(PHONE_CONTROLLER,FIELD(e.native,8,unsigned char))
#endif
                       ) controller_ui(e.native,e.step_ticket);
            else ORIG(H_CONTROLLER,void(*)(void*))(e.native);
        } else if (e.kind == PHONE_BUTTON) {
            if (current && !(seen&2)) { seen|=2; note("FIFO_BUTTON_ON_UI"); }
#ifdef MPC_COMPRESSOR
            if(e.pad&64u)compressor_ui_button(e.native,e.workflow_ticket);
            else
#endif
#ifdef MPC_CHORDS
            if(e.pad&32u){
                if(!chords_ui_button(e.native)){
                    unsigned code=FIELD(e.native,8,unsigned char),down=FIELD(e.native,9,unsigned char)!=0;
                    note_native_button_requested(code,down);music_ui_requested(code,down);step_native_requested(code,down);
                    buttons_ui(e.native,e.step_ticket);
                }
            }
            else
#endif
#ifdef MPC_RESAMPLE_TRIAL
            if(e.pad&16u){if(!resample_ui_button(e.native,e.workflow_ticket))forward_buttons(e.native);}
            else
#endif
            if (e.pad&8u){workflow_buttons(e.native,e.workflow_ticket);}
            else if (e.pad&4u) {
                if (step_native_button_at(e.native,e.step_ticket,e.when_ms)==2)
                    forward_buttons(e.native);
            }
            else if (e.pad&2u) music_ui_button(e.native);
            else if (current || note_native_active() || music_ui_active() || step_native_active()
#ifdef MPC_COMPRESSOR
                    ||compressor_ui_owns(PHONE_BUTTON,FIELD(e.native,8,unsigned char))
#endif
                    ) buttons_ui(e.native,e.step_ticket);
            else forward_buttons(e.native);
        }
#ifdef MPC_RESAMPLE_TRIAL
        else if(e.kind==PHONE_RESAMPLE_PAD){
            resample_pad_ui(&e);
        }
#endif
        else if(e.kind==PHONE_WORKFLOW_PAD){workflow_pad(e.pad,(int)e.down,e.when_ms,e.workflow_ticket);
        } else if (e.kind==PHONE_STEP_PAD) {
            if(e.down)step_native_pad(e.pad,e.step_ticket);else step_native_pad_release(e.pad,e.step_ticket);
        } else if (e.kind==PHONE_STEP_SHORTCUT_PAD && e.pad<16) {
            unsigned bit=1u<<e.pad;
            if (e.down && !(shortcut_forwarded&bit) && step_native_shortcut(e.step_ticket)) shortcut_forwarded|=bit;
            if (shortcut_forwarded&bit) {
                /* Native Shift events precede this in the same UI FIFO.
                 * Message data is inline; no borrowed buffer is retained. */
                _Alignas(8) unsigned char native[40];
                memcpy(native,e.native,16);memcpy(native+16,e.midi,24);
                ORIG(H_PAD,void(*)(void*))(native);
                if (e.up) shortcut_forwarded&=~bit;
            }
        } else if (e.kind == PHONE_PAD && current && state.enabled && !atomic_load(&input_fault)) {
            if (!(seen&4)) { seen|=4; note("FIFO_PAD_ON_UI"); }
            phone_pad(&state,e.pad+1,(int)e.down,e.when_ms);
            if (e.down) feedback_until[e.pad]=now_ms()+FEEDBACK_MIN_MS;
            write_native();
            /* The caret and text are drawn by our field paint: repaint after
             * every pad, or a cursor move (pads 1/3) stays invisible until
             * something else redraws the screen. */
            if (e.pad==3 || e.pad==11) lights();
            else feedback_refresh(now_ms());
            repaint();
        }
        /* Close the session between queued actions, even if a later native
         * button opens another naming window before the next timer tick. */
        if (atomic_exchange(&editor_destroyed,0)) stop();
        else if (editor && !visible()) stop();
    }
    atomic_fetch_sub(&ui_dispatching,1);
    return processed;
}

/* Hook the shared final sender, including callers which inline 0x159f910.
 * Conversion and SysEx packaging still use the original native functions. */
#ifdef MPC_SEQUENCE_COLOURS
static _Atomic uint64_t sequence_palette[16];
static _Atomic unsigned sequence_palette_enabled;
/* UI-only playhead read, using the same validated native owner chain and
 * 88-byte PositionInfo ABI as the Chords arpeggiator. No RGB/audio callback
 * reads Components or tempo services. Engine ticks are 960 per quarter. */
struct sequence_timing {double bpm;unsigned beat,bar;};
struct sequence_clock {uint64_t ms,serial;int64_t tick;double stopped;int valid,running;};
static struct sequence_timing sequence_timing(void){
    struct sequence_timing t={120,960,3840};
    void*app=FIELD((void*)base,0x71e3dc8,void*);
    if(!app||FIELD(app,0,uintptr_t)!=base+0x6e07288)return t;
    void*audio=FIELD(app,0xd8,void*);
    if(!audio||FIELD(audio,0,uintptr_t)!=base+0x6e0c430)return t;
    void*core=FIELD(audio,0x148,void*);
    if(!core||FIELD(core,0,uintptr_t)!=base+0x6e0cf38)return t;
    void*playhead=FIELD(core,0x490,void*);
    if(!playhead||FIELD(playhead,0,uintptr_t)!=base+0x6d8a418)return t;
    _Alignas(8) unsigned char position[88]={0};
    if(!FN(0x13ad070,int(*)(void*,void*))(playhead,position))return t;
    double bpm=FIELD(position,0,double);
    if(isfinite(bpm)&&bpm>=20&&bpm<=999)t.bpm=bpm;
    unsigned n=FIELD(position,8,unsigned),d=FIELD(position,12,unsigned);
    if(n>=1&&n<=32&&d>=1&&d<=64&&!(d&(d-1))){t.beat=3840/d;t.bar=t.beat*n;}
    return t;
}
static double sequence_clock_time(struct sequence_clock*c,uint64_t now,int running,
                                  uint64_t serial,int64_t tick,double bpm){
    if(!c->valid||c->running!=running){
        c->valid=1;c->running=running;c->serial=serial;c->tick=tick;c->ms=now;c->stopped=0;
    }
    if(running){
        if(c->serial!=serial){c->serial=serial;c->tick=tick;c->ms=now;}
        return (double)c->tick+(double)(now-c->ms)*bpm*960./60000.;
    }
    c->stopped+=(double)(now-c->ms)*bpm*960./60000.;c->ms=now;
    return c->stopped;
}
static unsigned sequence_brightness(int occupied,unsigned index,int selected,int playing,
                                    double when,int queued,const struct sequence_timing*t){
    int current=(int)index==(playing>=0?playing:selected);
    if(!occupied&&!current&&!queued)return 0;
    /* Other occupied slots keep the same level before/after Play. Neither a
     * native dark phase nor a playback transition dims the assigned palette. */
    if(!current&&!queued)return 192;
    double period=queued?t->beat*.5:playing>=0?t->beat:t->bar;
    double width=fmin(period/3.,t->bpm*960.*.090/60.);
    double phase=fmod(when,period);if(phase<0)phase+=period;
    /* Short full-colour flash over a visible background, never fully off. */
    return phase<width?255:96;
}
static int sequence_light(unsigned pad,uint64_t *out){
    if(pad>=16||!atomic_load(&sequence_palette_enabled))return 0;
    *out=atomic_load(&sequence_palette[pad]);return 1;
}
static void sequence_palette_tick(void){
    int bank=step_native_browser_bank(-1);
    /* While recording the native app keeps the Sample pad layout on the Seq
     * screen (no sequence selection until Stop): show its lights as they are. */
    int enabled=music_ui_sequence_visible()&&bank>=0&&bank<8&&!(step_native_transport_state()&4u)&&
        !atomic_load(&native_mute_mode)&&!atomic_load(&native_buttons_down[0x23]);
    static struct sequence_clock clock;
    static int previous_bank=-1,previous_transport=-1,previous_selected=-1;
    if(!enabled){clock.valid=0;previous_bank=-1;atomic_store(&sequence_palette_enabled,0);return;}
    unsigned present=music_ui_sequence_presence((unsigned)bank);
    int selected=music_ui_sequence_selected();
    int transport=(step_native_transport_state()&3u)==3u;
    uint64_t serial;unsigned running=0;int64_t tick=step_native_clock_ticks(&serial,&running);
    uint64_t now=now_ms();struct sequence_timing timing=sequence_timing();
    if(!transport&&selected!=previous_selected)clock.valid=0;
    double when=sequence_clock_time(&clock,now,transport,serial,tick,timing.bpm);
    previous_selected=selected;
    /* Native queued pulse: a pad's own brightness changed at least three
     * times within 1.5 s (a single change is a refresh). */
    static unsigned last_native[16];static uint64_t changed_ms[16][3];
    int pulsing[16];
    for(unsigned i=0;i<16;++i){
        unsigned old=(unsigned)atomic_load(&original_lights[i]);
        unsigned v=(old>>16)&255;if(((old>>8)&255)>v)v=(old>>8)&255;if((old&255)>v)v=old&255;
        if(bank!=previous_bank||transport!=previous_transport){
            last_native[i]=v;memset(changed_ms[i],0,sizeof(changed_ms[i]));
        }
        if(v!=last_native[i]){last_native[i]=v;
            changed_ms[i][2]=changed_ms[i][1];changed_ms[i][1]=changed_ms[i][0];changed_ms[i][0]=now;}
        /* breathing: changed in the last 0.4 s, three changes within 1.2 s */
        pulsing[i]=changed_ms[i][2]&&now-changed_ms[i][0]<400&&now-changed_ms[i][2]<1200;
    }
    /* Native semantics (lab trace): the context's sequence is the one that
     * plays; a pad selected while playing is only queued, and the native app
     * breathes it until the switch at the end of the bar. */
    previous_bank=bank;previous_transport=transport;
    int playing=transport?selected:-1;
    for(unsigned i=0;i<16;++i){
        unsigned old=(unsigned)atomic_load(&original_lights[i]);
        unsigned index=(unsigned)bank*16+i;
        int queued=transport&&pulsing[i]&&(int)index!=selected;
        unsigned brightness=sequence_brightness(!!(present&(1u<<i)),index,selected,playing,when,queued,&timing);
        int chosen=music_sequence_colour((unsigned)bank*16+i);
        unsigned colour=music_sequence_colours[chosen];
        if(chosen==0){ /* Stock: the hue the native app gave this pad when lit */
            static unsigned stock_hue[16];
            unsigned top=(old>>16)&255;if(((old>>8)&255)>top)top=(old>>8)&255;if((old&255)>top)top=old&255;
            /* Follow the native hue as it is now (green selected/queued,
             * yellow recorded): only a dark phase keeps the last one. */
            if(top>=48)stock_hue[i]=((((old>>16)&255)*255/top)<<16)|((((old>>8)&255)*255/top)<<8)|((old&255)*255/top);
            if(stock_hue[i])colour=stock_hue[i];
        }
        /* Assigned hue is retained in both pulse and background phases. */
        unsigned red=((colour>>16)&255)*brightness/255;
        unsigned green=((colour>>8)&255)*brightness/255;
        unsigned blue=(colour&255)*brightness/255;
        float intensity=1;uint32_t bits;memcpy(&bits,&intensity,4);
        atomic_store(&sequence_palette[i],(uint64_t)((red<<16)|(green<<8)|blue)|((uint64_t)bits<<32));
    }
    atomic_store(&sequence_palette_enabled,1);
}
#endif
/* UI Timer publishes screen ownership; RGB callbacks must not inspect native
 * Components, which may belong to another thread. */
static _Atomic unsigned pad_colour_sample_layout,pad_colour_native_workflow,pad_colour_cursor_white;
static _Atomic int pad_colour_bank;
/* Sample pad layout: the user's Pad Colour (Mix, K2) keeps the
 * native brightness of each pad (dim loaded, bright playing) and replaces
 * only its hue. Stock (0) passes the native colour through. */
static int pad_colour_light(unsigned pad,uint64_t *out){
    if(pad>=16||!atomic_load(&pad_colour_sample_layout)||!music_pad_colours_used()||atomic_load(&led_enabled))return 0;
    int bank=atomic_load(&pad_colour_bank);if(bank<0||bank>=8)return 0;
    int chosen=music_pad_colour((unsigned)bank*16+pad);if(!chosen)return 0;
    if(atomic_load(&pad_colour_native_workflow)){
        unsigned raw=(unsigned)atomic_load(&original_lights[pad]);
        unsigned r=(raw>>16)&255,g=(raw>>8)&255,b=raw&255,top=r>g?r:g;if(b>top)top=b;
        if(r>80&&r>g*2&&r>b*2)return 0; /* Native overwrite warning. */
        if(r==g&&g==b&&top>=192&&atomic_load(&pad_colour_cursor_white))return 0;
        unsigned value=top?(top<160?160:top):0,colour=music_sequence_colours[chosen];
        *out=((((colour>>16)&255)*value/255)<<16)|((((colour>>8)&255)*value/255)<<8)|((colour&255)*value/255);
        *out|=UINT64_C(0x3f800000)<<32;return 1;
    }
    /* Temporary native indications keep priority: Mute mode, held Mute
     * (0x23) and held Erase (0x09) show the native colours as they are. */
    if(atomic_load(&native_mute_mode)||atomic_load(&native_buttons_down[0x23])||atomic_load(&native_buttons_down[0x09]))return 0;
    unsigned enabled=0,mode=0,origin=0;music_lighting_levels_state(&enabled,&mode,&origin);
    if(enabled||(step_native_active()&&!step_native_sound_layout())
#ifdef MPC_CHORDS
       ||chords_active()
#endif
       )return 0;
    unsigned old=(unsigned)atomic_load(&original_lights[pad]);
    unsigned level=(old>>16)&255;if(((old>>8)&255)>level)level=(old>>8)&255;if((old&255)>level)level=old&255;
    if(!level)return 0;
    unsigned colour=music_sequence_colours[chosen];
    unsigned red=((colour>>16)&255)*level/255,green=((colour>>8)&255)*level/255,blue=(colour&255)*level/255;
    float intensity=1;uint32_t bits;memcpy(&bits,&intensity,4);
    *out=(uint64_t)((red<<16)|(green<<8)|blue)|((uint64_t)bits<<32);return 1;
}
static void rgb(void *s,uint32_t colour,unsigned pad)
{
    if (pad < 16) {
        atomic_store(&sender,(uintptr_t)s);
        atomic_store(&original_lights[pad],colour);
        atomic_fetch_or(&light_mask,1u << pad);
        uint64_t workflow_colour;
#ifdef MPC_STOCK_RESAMPLE
        if(resample_ui_light(pad,&workflow_colour)){
            uint32_t packed=FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&workflow_colour);
            ORIG(H_RGB,void(*)(void*,uint32_t,unsigned))(s,packed,pad);return;
        }
#endif
        if(workflow_light(pad,&workflow_colour)){
            uint32_t packed=FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&workflow_colour);
            ORIG(H_RGB,void(*)(void*,uint32_t,unsigned))(s,packed,pad);return;
        }
        if (atomic_load(&led_enabled)) {
            struct phone_input colours = {0};
            colours.numeric = (int)atomic_load(&led_numeric);
            colours.uppercase = (int)atomic_load(&led_uppercase);
            colours.held = atomic_load(&led_pressed);
            uint64_t replacement = phone_light(&colours,pad + 1);
            uint32_t packed = FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&replacement);
            ORIG(H_RGB,void(*)(void*,uint32_t,unsigned))(s,packed,pad);
            return;
        }
        uint64_t replacement;
        if (workflow_light(pad,&replacement)
#ifdef MPC_CHORDS
            || chords_light(pad,&replacement)
#endif
            || step_native_light(pad,&replacement)
#ifdef MPC_SEQUENCE_COLOURS
            || sequence_light(pad,&replacement)
#endif
            || pad_colour_light(pad,&replacement)
        ) {
            uint32_t packed=FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&replacement);
            ORIG(H_RGB,void(*)(void*,uint32_t,unsigned))(s,packed,pad);return;
        }
    }
    ORIG(H_RGB,void(*)(void*,uint32_t,unsigned))(s,colour,pad);
}
static void send_step_lights(void)
{
    /* The compressor still plays the current Sample bank. Its algorithm and
     * Bypass do not change pad ownership. Other FX keep native status colours. */
    unsigned native_workflow=workflow_native_pad_colours();
    atomic_store(&pad_colour_native_workflow,native_workflow);
    atomic_store(&pad_colour_bank,step_native_browser_bank(-1));
    atomic_store(&pad_colour_cursor_white,now_ms()%640<240);
    atomic_store(&pad_colour_sample_layout,native_workflow||step_native_sound_layout()||
#ifdef MPC_COMPRESSOR
        compressor_ui_active()||
#endif
        (music_ui_sequence_visible()?!!(step_native_transport_state()&4u):!!note_native_sample_visible()));
#ifdef MPC_SEQUENCE_COLOURS
    sequence_palette_tick();
#endif
    static unsigned owned;
    static uint64_t previous[16];
    static uint64_t workflow_generation;
    uintptr_t s=atomic_load(&sender);
    if (!s) {owned=0;return;}
    uint64_t palette_epoch=workflow_palette_generation();
    int force_palette=palette_epoch!=workflow_generation;workflow_generation=palette_epoch;
    uint64_t workflow_probe;int workflow_colours=workflow_light(0,&workflow_probe);
#ifdef MPC_STOCK_RESAMPLE
    static uint64_t resample_generation;
    uint64_t resample_epoch=resample_ui_palette_generation();
    force_palette|=resample_epoch!=resample_generation;resample_generation=resample_epoch;
    workflow_colours|=resample_ui_light(0,&workflow_probe);
#endif
    if (atomic_load(&led_enabled)&&!workflow_colours) {owned=0;return;}
    /* After a mode change (Chords, 16 Levels, Sample/Seq screens) the native
     * app repaints only pads it thinks changed. Resend its own colours for
     * every pad we do not override for a few ticks, so no pad keeps a colour
     * from the mode just left. */
    static unsigned last_mode=~0u,resync;
    {
        unsigned enabled=0,mode=0,origin=0;music_lighting_levels_state(&enabled,&mode,&origin);
        unsigned m=(enabled?1u:0u)|((mode&7u)<<1)|(note_native_sample_visible()?16u:0u)|
                   (music_ui_sequence_visible()?32u:0u)|(step_native_active()?64u:0u)
#ifdef MPC_CHORDS
                   |(chords_active()?128u:0u)
#endif
#ifdef MPC_COMPRESSOR
                   |(compressor_ui_active()?256u:0u)
#endif
                   |(atomic_load(&native_mute_mode)?512u:0u)
                   ;
        if(m!=last_mode){last_mode=m;resync=6;}
    }
    for(unsigned i=0;i<16;++i) {
        uint64_t light;unsigned bit=1u<<i;
        if (
#ifdef MPC_STOCK_RESAMPLE
            resample_ui_light(i,&light) ||
#endif
            workflow_light(i,&light)
#ifdef MPC_CHORDS
            || chords_light(i,&light)
#endif
            || step_native_light(i,&light)
#ifdef MPC_SEQUENCE_COLOURS
            || sequence_light(i,&light)
#endif
            || pad_colour_light(i,&light)
        ) {
            if (force_palette || !(owned&bit) || previous[i]!=light) {
                previous[i]=light;
                uint32_t packed=FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&light);
                ORIG(H_RGB,void(*)(void*,uint32_t,unsigned))((void*)s,packed,i);
            }
            owned|=bit;
        } else if ((owned&bit)||resync) {
            if(atomic_load(&light_mask)&bit)
                ORIG(H_RGB,void(*)(void*,uint32_t,unsigned))((void*)s,(uint32_t)atomic_load(&original_lights[i]),i);
            owned&=~bit;
        }
    }
    if(resync)--resync;
}
static void button_light(void *s,unsigned value,unsigned code)
{
    /* B0 CC0..15 are button LEDs (Knob FX, Sample, Seq, Erase...), not pads. */
    if (code<128) {
        if(code==0x2b)observe_mute_indicator(value);
        atomic_store(&button_sender,(uintptr_t)s);atomic_store(&native_button_lights[code],value);
        unsigned replacement;if(
#ifdef MPC_CHORDS
            chords_ui_button_light(code,&replacement)||
#endif
            step_native_button_light(code,&replacement)) value=replacement;
    }
    ORIG(H_BUTTON_LIGHT,void(*)(void*,unsigned,unsigned))(s,value,code);
}
static void send_record_light(void)
{
    static unsigned owned[4],last[4];const unsigned codes[4]={0x50,0x28,0x4b,0x2b};
    uintptr_t s=atomic_load(&button_sender);unsigned value;
    if (!s) return;
    for(unsigned i=0;i<4;++i) {
        if (
#ifdef MPC_CHORDS
            chords_ui_button_light(codes[i],&value)||
#endif
            step_native_button_light(codes[i],&value)) {
            if (!owned[i] || value!=last[i]) ORIG(H_BUTTON_LIGHT,void(*)(void*,unsigned,unsigned))((void*)s,value,codes[i]);
            owned[i]=1;last[i]=value;
        } else if (owned[i]) {
            ORIG(H_BUTTON_LIGHT,void(*)(void*,unsigned,unsigned))((void*)s,atomic_load(&native_button_lights[codes[i]]),codes[i]);
            owned[i]=0;
        }
    }
}
static int find_main(struct dl_phdr_info *info,size_t size,void *out)
{
    (void)size;
    if (info->dlpi_name && *info->dlpi_name) return 0;
    for (unsigned i=0;i<info->dlpi_phnum;++i) {
        const ElfW(Phdr) *p=&info->dlpi_phdr[i];
        if (p->p_type==PT_LOAD && PHONE_TABLE_VA>=p->p_vaddr
            && PHONE_TABLE_VA + 4096 <= p->p_vaddr+p->p_memsz) {
            *(uintptr_t*)out = info->dlpi_addr;
            return 1;
        }
    }
    return 0;
}

#ifdef PHONE_RGB_TEST
__attribute__((unused))
#else
__attribute__((constructor))
#endif
static void load(void)
{
    uintptr_t found=0;
    if (!dl_iterate_phdr(find_main,&found)) return; /* setarch also sees LD_PRELOAD. */
    base=found;
#ifdef MPC_COMPRESSOR
    compressor_ui_init(base);
#endif
    note_native_init(base);
    music_reset(); music_archive_init(base); music_ui_init(base);
#ifdef MPC_MIXER_ARCHIVE
    mixer_archive_init(base);
#endif
    music_lighting_init(base);
#ifdef MPC_CHORDS
    chords_ui_init(base);
#endif
    step_native_init(base);workflow_init(base);batch_copy_init(base);
#ifdef MPC_RESAMPLE_TRIAL
    resample_ui_init(base);
#endif
    step_native_levels_processor(base+phone_hooks[H_SIXTEEN_PROCESS].trampoline);
    unsigned char *table=(unsigned char*)(base+PHONE_TABLE_VA);
    if (memcmp(table,PHONE_BRIDGE_MAGIC,sizeof(PHONE_BRIDGE_MAGIC)-1)) return;
    static void (*const callbacks[H_COUNT])(void)={
        [H_REBUILD]=(void(*)(void))rebuild,[H_DTOR]=(void(*)(void))editor_dtor,
        [H_DELETE]=(void(*)(void))editor_delete,[H_PAD]=(void(*)(void))pad_event,
        [H_CONTROLLER]=(void(*)(void))controller,[H_BUTTONS]=(void(*)(void))buttons,
        [H_SAMPLE_PAINT]=(void(*)(void))sample_paint,[H_PROJECT_PAINT]=(void(*)(void))project_paint,
        [H_RGB]=(void(*)(void))rgb,[H_CHAR_INCREMENT]=(void(*)(void))char_increment,
        [H_CASE]=(void(*)(void))case_change,[H_MOVE]=(void(*)(void))move_action,
        [H_CLEAR]=(void(*)(void))clear_action,[H_ERASE]=(void(*)(void))erase_action,
        [H_PLUS]=(void(*)(void))plus_action,[H_MINUS]=(void(*)(void))minus_action,
        [H_SCROLL]=(void(*)(void))scroll_action,[H_RIGHT]=(void(*)(void))right_action,
        [H_SAMPLE_DTOR]=(void(*)(void))sample_dtor,[H_PROJECT_DTOR]=(void(*)(void))project_dtor,
        [H_SIXTEEN_REFRESH]=(void(*)(void))sixteen_refresh,[H_SIXTEEN_PROCESS]=(void(*)(void))sixteen_process,
        [H_MUSIC_SAVE]=(void(*)(void))music_save,[H_MUSIC_LOAD]=(void(*)(void))music_load,
        [H_MUSIC_READ]=(void(*)(void))music_read,[H_MUSIC_KEY]=(void(*)(void))music_key,
        [H_NEW_PROJECT]=(void(*)(void))new_project,[H_SIXTEEN_CTOR]=(void(*)(void))sixteen_ctor,
        [H_SIXTEEN_DTOR]=(void(*)(void))sixteen_dtor,[H_STEP_DTOR]=(void(*)(void))step_dtor,
        [H_NOW_TICK]=(void(*)(void))now_tick,[H_BANK_CTOR]=(void(*)(void))bank_ctor,
        [H_BANK_DTOR]=(void(*)(void))bank_dtor,[H_BUTTON_LIGHT]=(void(*)(void))button_light,
        [H_AUDITION_CTOR]=(void(*)(void))audition_ctor,
        [H_FILES_CTOR]=(void(*)(void))files_ctor,
        [H_MASTER_VOLUME_CTOR]=(void(*)(void))master_volume_ctor,[H_FILE_PREVIEW]=(void(*)(void))file_preview,
        [H_PRIVATE_PREVIEW_WORKER]=(void(*)(void))private_preview_worker,
        [H_CHOP_REFRESH]=(void(*)(void))chop_refresh,[H_NOTE_FOOTER]=(void(*)(void))note_footer,
        [H_MUSIC_PROJECT]=(void(*)(void))music_ui_request_project,
        [H_EVENT_LABEL]=(void(*)(void))step_native_event_label,
        [H_FILES_DTOR]=(void(*)(void))files_dtor,[H_COPY_DTOR]=(void(*)(void))copy_dtor,
        [H_COPY_CONTEXT_CTOR]=(void(*)(void))copy_context_ctor,[H_COPY_CONTEXT_DTOR]=(void(*)(void))copy_context_dtor,
        [H_COPY_SOURCE]=(void(*)(void))batch_copy_source,
        [H_COPY_RELEASE_EXIT]=(void(*)(void))batch_copy_native_release_exit,
        [H_PREVIEW_EVENT_GATE]=(void(*)(void))step_native_preview_suppressed
    };
    for (unsigned i=0;i<H_COUNT;++i) {
        uint64_t *entry=(uint64_t*)(table+64+i*16);
        if (!callbacks[i] || entry[0]!=phone_hooks[i].entry || entry[1]) { note("BRIDGE_REJECTED"); return; }
    }
    if (mprotect(table,4096,PROT_READ|PROT_WRITE)) { note("BRIDGE_NOT_WRITABLE"); return; }
    if (!music_ui_install(music_sample_paint,music_project_paint,music_tab_paint)) _exit(121);
    if (!step_native_install(step_paint)) _exit(121);
    if (!workflow_install(workflow_files,workflow_copy)) _exit(121);
    if (!install_name_cursor()) _exit(121);
    if (!project_exit_install(base)) _exit(121);
#ifdef MPC_COMPRESSOR
    if (!compressor_ui_install()) _exit(121);
#endif
#ifdef MPC_POWER_BUTTON
    if (!power_control_install(base)) _exit(121);
#endif
#ifdef MPC_POLY_RECORD
    if (!poly_record_install(base)) _exit(121);
#endif
#ifdef MPC_CHORDS
    /* Optional: without it Stop-Stop keeps the stock unguarded release. */
    if (!voice_guard_install(base)) note("VOICE_GUARD UNAVAILABLE");
    if (!takeover_native_check(base)) _exit(121);
#endif
    if (!values_bipolar_install(base)) note("VALUES_BIPOLAR UNAVAILABLE");
    if (!values_shift_pairs_install(base)) note("VALUES_SHIFT_PAIRS UNAVAILABLE");
    if (!values_loop_lock_install(base)) note("VALUES_LOOP_LOCK UNAVAILABLE");
#ifdef MPC_PAD_EQ
    if (!filter_spectrum_install(base,music_ui_filter_spectrum_visible)) _exit(121);
    if (!pad_eq_native_install(base)) _exit(121);
    if (!pad_eq_history_install(base)) _exit(121);
#endif
    for (unsigned i=0;i<H_COUNT;++i) FIELD(table,64+i*16+8,uintptr_t)=(uintptr_t)callbacks[i];
    if (mprotect(table,4096,PROT_READ|PROT_EXEC)) _exit(120);
#ifdef MPC_FAST_RESAMPLE
    /* Constructor runs before main/audio startup. Later Render commands only
     * check readiness; they must never revoke X on running input handlers. */
    if (!resample_fast_native_install(base)) _exit(121);
#ifdef MPC_POWER_BUTTON
    note("BRIDGE_READY r23_power_button_sd_copy");
#else
    note("BRIDGE_READY r21c_lighting_power_audit");
#endif
#elif defined(MPC_RESAMPLE_TRIAL)
    note("BRIDGE_READY r17d_resample_selection_naming_cursor");
#else
    note("BRIDGE_READY r16d_sample_input_event_preview");
#endif
}
