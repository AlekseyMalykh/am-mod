#define _GNU_SOURCE
#include "phone_input.h"
#include "phone_events.h"
#ifdef MPC_KNOB_RESET
#include "knob_reset.h"
#endif
#include "phone_hooks.h"
#include "sample_load_trace.h"
#ifdef MPC_REAR_ROUTING
#include "rear_native.h"
static _Atomic unsigned consumed_rear;
#endif
#ifdef MPC_LOOP_FEEDBACK
#include "sample_playing.h"
#endif
#ifdef MPC_SONG_UI
#include "song_ui.h"
#endif
#ifdef MPC_COMPRESSOR
#include "compressor.h"
#endif
#ifdef MPC_FAST_RESAMPLE
#include "resample_fast_native.h"
#endif
#include "note_native.h"
#include "music_settings.h"
#include "pad_palette.h"
#include "pad_workflow_colour.h"
#include "music_archive.h"
#ifdef MPC_MIXER_ARCHIVE
#include "mixer_archive.h"
#endif
#ifdef MPC_MIXER_LIVE
#include "mixer_live.h"
#include "mixer_input.h"
#include "mixer_session.h"
#include "mixer_leds.h"
#ifdef MPC_MIXER_AUTOBIND
#include "mixer_runtime.h"
#include "mixer_sources.h"
#endif
#endif
#include "music_ui.h"
#ifdef MPC_PAD_EQ
#include "filter_spectrum.h"
#endif
#include "project_exit.h"
#ifdef MPC_PROJECT_RECOVERY
#include "project_recovery.h"
#endif
#ifdef MPC_STARTUP_SPLASH
#include "startup_splash.h"
#ifdef MPC_EARLY_STARTUP
#include "startup_project.h"
#endif
#endif
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
static void start_timer(void);
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

#ifdef MPC_MIXER_LIVE
static _Atomic uint32_t last_sent_rgb[16];
static _Atomic unsigned last_sent_mask, rgb_collision, rgb_sending[16];
#endif
static void send_pad_rgb(void *s,uint32_t colour,unsigned pad)
{
#ifdef MPC_MIXER_LIVE
    if(pad<16 && atomic_fetch_add(&rgb_sending[pad],1))atomic_fetch_or(&rgb_collision,1u<<pad);
#endif
    ORIG(H_RGB,void(*)(void*,uint32_t,unsigned))(s,colour,pad);
#ifdef MPC_MIXER_LIVE
    if(pad<16){
        atomic_store(&last_sent_rgb[pad],colour);atomic_fetch_or(&last_sent_mask,1u<<pad);
        atomic_fetch_sub(&rgb_sending[pad],1);
    }
#endif
}

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
/* PAD BANK (0x23) held: the native bank picker owns the pads only after a
 * real hold. A short press (bank change) used to flash the picker's green
 * over the Seq/Sample colours (device report). Set by the UI timer. */
static _Atomic unsigned pad_bank_held_long;
#ifdef MPC_KNOB_RESET
static _Atomic unsigned bank_pad_owned;
#endif
static _Atomic unsigned native_shift_requested,native_mute_mode;
static _Atomic unsigned tail_layout,tail_pads,stop_requested;
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
int power_control_operation_busy(void){return workflow_jobs_busy()||resample_ui_shutdown_busy()
#ifdef MPC_REAR_ROUTING
||rear_native_busy()
#endif
;}
#endif
static int on_ui(void);
/* Weak no-op defaults keep host tests that include this file without
 * music_lighting.c / note_native.c linkable; the real units override them. */
__attribute__((weak)) void music_lighting_request_refresh(void){}
__attribute__((weak)) void note_native_mix_shift_tick(void){}
__attribute__((weak)) int workflow_copy_mode_id(void){return -2;}
__attribute__((weak)) int workflow_reset_knob(unsigned column,unsigned shift){(void)column;(void)shift;return 0;}
extern int rear_ui_reset_knob(unsigned column,unsigned shift) __attribute__((weak));
extern int startup_project_loading(void) __attribute__((weak));
extern int workflow_startup_loading(void) __attribute__((weak));
extern int workflow_browser_shown(void) __attribute__((weak));
extern unsigned workflow_native_muted(void) __attribute__((weak));
static _Atomic unsigned pad_colour_native_muted;
/* Startup project load (after the app is up): one running pad, left to
 * right and top to bottom (13..16, 9..12, 5..8, 1..4), in the Play
 * position colour, like the Steps playhead. Published by the UI timer; the RGB hook only reads it. */
static _Atomic unsigned startup_pads;
/* Weak declarations (not definitions): some host fixtures include this file
 * and define these themselves, others link without them (address NULL). */
extern int music_set_sequence_colour(unsigned sequence,unsigned colour) __attribute__((weak));
extern int music_sequence_colour(unsigned sequence) __attribute__((weak));
extern unsigned music_ui_sequence_presence(unsigned bank) __attribute__((weak));
/* Physical STOP press, replayed as a second press so Stop+pad cuts the pad's
 * sounding voice (stock double-Stop all-sounds-off) before its tail plays. */
static pthread_mutex_t stop_template_mutex=PTHREAD_MUTEX_INITIALIZER;
static unsigned char stop_template[16];static int stop_template_ready;
static void forward_buttons(void*event);
static unsigned tail_mode(void){
 if(!music_tail_ms((unsigned)music_mod_setting(MUSIC_SETTING_TAIL)))return 0; /* Preview from end: Off */
 if(!note_native_sample_visible()||!music_ui_chords_available()||music_ui_sequence_visible()
    ||workflow_blocks_sample_controls()||step_native_active()||(step_native_transport_state()&4u)
    ||atomic_load(&naming_active)||atomic_load(&native_mute_mode)||atomic_load(&native_shift_requested))return 0;
#ifdef MPC_COMPRESSOR
 if(compressor_ui_active())return 0;
#endif
#ifdef MPC_CHORDS
 if(chords_active())return 0;
#endif
#ifdef MPC_RESAMPLE_TRIAL
 if(resample_ui_active()||resample_running())return 0;
#endif
 unsigned enabled=0,mode=0,origin=0;
 music_lighting_levels_state(&enabled,&mode,&origin);
 return enabled?(mode==8?2u:0u):1u;
}
static void forward_controller(void*event){
#ifdef MPC_CHORDS
 takeover_native_forward(event,ORIG(H_CONTROLLER,void(*)(void*)));
#else
 ORIG(H_CONTROLLER,void(*)(void*))(event);
#endif
}
static void forward_buttons(void*event){
 unsigned code=FIELD(event,8,unsigned char),down=FIELD(event,9,unsigned char)!=0;
 if((code==0x51||code==0x52)&&down)workflow_button(event,0);
 if(code<112){
  if(!down&&atomic_exchange(&gesture_released[code],0))return;
  if(down&&(code==0x31||code==0x09)){pthread_mutex_lock(&gesture_mutex);memcpy(gesture_down_events[code==0x31?0:1],event,16);pthread_mutex_unlock(&gesture_mutex);}
  atomic_store(&native_buttons_down[code],(unsigned char)down);
 }
 /* The native app handles buttons on its MIDI input thread. Queued buttons
  * reach it from the UI thread instead, which native code tolerates in the
  * same way. Only Stop is serialised with the render: its release-all of
  * Chords voices raced the audio thread (r24f). Holding the render lock
  * across any other native command risks a deadlock when that command waits
  * for the audio thread, e.g. Shift+REC Recall with the Resample input
  * (r27q device hang). release_all() keeps its own bounded lock. */
#ifdef MPC_CHORDS
 int guard=on_ui()&&code==0x51;if(guard)voice_guard_lock();
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
static void music_buttons(void *event){
 /* A menu may close earlier in this FIFO batch. Its producer-side claim
  * is only a hint: if the UI owner declines, preserve the native click. */
 unsigned code=FIELD(event,8,unsigned char),down=FIELD(event,9,unsigned char)!=0;
 if(!music_ui_button(event)||(!down&&code<112&&atomic_load(&native_buttons_down[code])))
  forward_buttons(event);
}

static int on_ui(void) { return ui_known && pthread_equal(ui_thread, pthread_self()); }

#ifdef MPC_MIXER_LIVE
/* Only the UI owns the session pointer and the gesture state. The input
 * producer carries a binding number and retains ownership until release. */
enum { MIXER_READY = 1, MIXER_OPEN = 2 };
static struct mixer_session *mixer_ui_session;
static struct mixer_entry mixer_entry;
static uint64_t mixer_binding;
static _Atomic uint64_t mixer_route, mixer_pending;
static _Atomic unsigned mixer_input_fault, mixer_project_invalidated;
static uint64_t mixer_pad_pairs[16], mixer_button_pairs[112];
static unsigned mixer_native_pads, mixer_mode_forwarded;
static unsigned char mixer_native_buttons[112];
static _Alignas(8) unsigned char mixer_entry_down[40];

static int mixer_ui_blocked(void)
{
    return atomic_load(&mixer_project_invalidated) || !note_native_sample_visible() ||
        FN(0x10f87a0,void*(*)(int))(0) || atomic_load(&naming_active) ||
        workflow_blocks_sample_controls() ||
        step_native_active() || resample_running()
#ifdef MPC_COMPRESSOR
        || compressor_ui_active()
#endif
#ifdef MPC_CHORDS
        || chords_active()
#endif
#ifdef MPC_POWER_BUTTON
        || power_control_busy()
#endif
        ;
}

static void mixer_close_ui(void)
{
    mixer_leds_disable();
    struct mixer_control *control = mixer_session_control(mixer_ui_session);
    if (mixer_control_active(control)) {
        mixer_control_close(control);
        note_native_repaint_sample();
    }
    mixer_entry_cancel(&mixer_entry);
}

/* Lab diagnostic: why Mixer is (not) ready, one line per change. */
static unsigned mixer_block_reasons(void)
{
    unsigned r = 0;
    if (atomic_load(&mixer_project_invalidated)) r |= 1;
    if (!note_native_sample_visible()) r |= 2;
    if (FN(0x10f87a0,void*(*)(int))(0)) r |= 4;
    if (atomic_load(&naming_active)) r |= 8;
    if (workflow_blocks_sample_controls()) r |= 16;
    if (step_native_active()) r |= 32;
    if (resample_running()) r |= 64;
#ifdef MPC_COMPRESSOR
    if (compressor_ui_active()) r |= 128;
#endif
#ifdef MPC_CHORDS
    if (chords_active()) r |= 256;
#endif
#ifdef MPC_POWER_BUTTON
    if (power_control_busy()) r |= 512;
#endif
    return r;
}
static void mixer_publish_route(void)
{
    struct mixer_control *control = mixer_session_control(mixer_ui_session);
    {
        static unsigned last = ~0u, lines;
        unsigned now = (mixer_session_status(mixer_ui_session) == 1 ? 0u : 0x10000u) | mixer_block_reasons();
        if (now != last && lines < 120) {
            ++lines; last = now;
            char m[80]; snprintf(m,sizeof(m),"MIXER_ROUTE session=%d blocked=0x%x",
                mixer_session_status(mixer_ui_session),now & 0xffffu);
            note(m);
        }
    }
    unsigned state = mixer_session_status(mixer_ui_session) == 1 && !mixer_ui_blocked()
        ? MIXER_READY : 0;
    if (state && mixer_control_active(control)) state |= MIXER_OPEN;
    atomic_store(&mixer_route, state ? (mixer_binding << 2) | state : 0);
    struct mixer_view_state view = {0};
    mixer_control_view(control,now_ms(),&view);
    if ((state & MIXER_OPEN) && (view.page == MIXER_ASSIGN || view.page == MIXER_INSPECT)) {
        /* Use the existing shared bank model so native playback indications
         * refer to the same sixteen sounds as the Mixer palette. */
        int bank = step_native_browser_bank(-1);
        if (bank >= 0 && bank < 8 && bank != (int)view.bank) {
            step_native_browser_bank((int)view.bank);
            bank = step_native_browser_bank(-1);
        }
        struct mixer_native_binding binding;
        int observed = mixer_live_native_binding(&binding);
        if (bank == (int)view.bank && observed < 0) return; /* Keep last palette through a busy snapshot. */
        if (bank == (int)view.bank && observed == 1 && binding.epoch == view.epoch) {
            uint32_t rgb[16];
            for (unsigned i=0;i<16;++i) mixer_control_led(control,i,&rgb[i]);
            mixer_leds_publish(binding.program,view.bank,rgb);
            return;
        }
    }
    mixer_leds_disable();
}

int mixer_phone_bind_ui(struct mixer_session *session)
{
    if (!on_ui()) return 0;
    if (mixer_ui_session == session) return 1;
    mixer_leds_disable();
    atomic_store(&mixer_route, 0);
    atomic_store(&mixer_pending, 0);
    mixer_close_ui();
    /* Drop only old Mixer ownership. Native replay ups still use their ledger. */
    struct mixer_control *old = mixer_session_control(mixer_ui_session);
    if (old) {
        for (unsigned pad = 0; pad < 16; ++pad)
            mixer_control_pad(old, pad, 0, mixer_control_ticket(old));
        for (unsigned code = 0; code < 112; ++code)
            mixer_input_button(old, code, 0, mixer_control_ticket(old));
    }
    mixer_ui_session = session;
    atomic_store(&mixer_project_invalidated,0);
    ++mixer_binding;
    mixer_entry_init(&mixer_entry, MIXER_ENTRY_HOLD_MS);
    if (session) start_timer();
    return 1;
}

static void mixer_project_changing(void)
{
    mixer_leds_reset();
    atomic_store(&mixer_project_invalidated,1);
    atomic_store(&mixer_route,0);
#ifdef MPC_MIXER_AUTOBIND
    mixer_live_native_invalidate();
#endif
}
static void mixer_fail_input(void)
{
    atomic_store(&mixer_route, 0);
    atomic_store(&mixer_input_fault, 1);
}

static uint64_t mixer_capture_route(void)
{
    uint64_t route = atomic_load(&mixer_route);
    if (atomic_load(&mixer_input_fault)) return 0;
    return (route & MIXER_OPEN) || (route && atomic_load(&mixer_pending) == route >> 2)
        ? route >> 2 : 0;
}

static int mixer_queue_pair(struct phone_event *e, uint64_t *held)
{
    if (e->down && *held) return 1;
    if (!e->down && !*held) return 0;
    if (!e->down) {
        e->generation = *held;
        *held = 0;
        if (e->generation == UINT64_MAX) return 1;
    }
    if (phone_events_push_pair(e, e->down)) {
        if (e->down) *held = e->generation;
    } else {
        if (e->down) *held = UINT64_MAX;
        mixer_fail_input();
    }
    return 1;
}

static int mixer_queue_pad(void *raw, unsigned pad, int down, int up)
{
    uint64_t route = atomic_load(&mixer_route);
    uint64_t binding = mixer_capture_route();
    uint64_t *held = &mixer_pad_pairs[pad];
    if (!*held && (!down || atomic_load(&mixer_input_fault))) return 0;
    int entry = down && !*held && !binding && (route & MIXER_READY) &&
        pad == 8 && atomic_load(&native_shift_requested);
    if (down && pad == 8) {
        static unsigned lines;
        if (lines++ < 40) fprintf(stderr,"MIXER_ENTRY_PAD9 route=%llx shift=%u entry=%d held=%d\n",
            (unsigned long long)route,atomic_load(&native_shift_requested),entry,*held!=0);
    }
    if (!*held && !entry && !binding) return 0;
    if (!down && !up) return 1; /* pressure follows the captured pad */
    int size = FIELD(raw,0x20,int);
    if (down && (size <= 0 || size > 8)) {
        if (!*held) *held = UINT64_MAX;
        mixer_fail_input();
        return 1;
    }
    struct phone_event e = {0};
    e.kind = entry ? PHONE_MIXER_ENTRY : PHONE_MIXER_PAD;
    e.pad = pad;
    e.down = down != 0;
    e.when_ms = now_ms();
    e.generation = entry ? route >> 2 : binding;
    memcpy(e.native,raw,16);
    if (size > 0 && size <= 8) {
        memcpy(e.midi,(char*)raw+16,24);
        e.up = up != 0;
    }
    if (entry) atomic_store(&mixer_pending, e.generation);
    return mixer_queue_pair(&e, held);
}

static void mixer_forward_pad(void *raw)
{
#ifdef MPC_CHORDS
    voice_guard_lock();
#endif
    ORIG(H_PAD,void(*)(void*))(raw);
#ifdef MPC_CHORDS
    voice_guard_unlock();
#endif
}

static int mixer_replay_entry(void)
{
    _Alignas(8) unsigned char shift[16];
    int inject = !atomic_load(&native_buttons_down[0x31]);
    if (inject) {
        pthread_mutex_lock(&gesture_mutex);
        memcpy(shift,gesture_down_events[0],sizeof(shift));
        pthread_mutex_unlock(&gesture_mutex);
        if (shift[8] != 0x31) return 0;
        forward_buttons(shift);
    }
    mixer_forward_pad(mixer_entry_down);
    if (inject) {
        shift[9] = 0;
        forward_buttons(shift);
    }
    return 1;
}

static void mixer_tick_ui(void)
{
    static uint64_t shown_presence[2];
    if (atomic_exchange(&mixer_input_fault, 0)) {
        mixer_close_ui();
        atomic_store(&mixer_pending, 0);
    }
    if (!mixer_ui_session) return;
    struct mixer_control *control = mixer_session_control(mixer_ui_session);
    struct mixer_view_state before, after;
    int was_open = mixer_control_active(control);
    uint64_t now = now_ms();
    mixer_control_view(control,now,&before);
    mixer_session_ui_tick(mixer_ui_session);
    int ready = mixer_session_status(mixer_ui_session) == 1;
    int blocked = mixer_ui_blocked();
    if (!ready || blocked) mixer_close_ui();
    if (!phone_events_pending() &&
        mixer_entry_tick(&mixer_entry, now_ms(), mixer_binding, ready, blocked) == MIXER_ENTRY_LONG_DOWN &&
        mixer_replay_entry()) mixer_native_pads |= 1u << 8;
    mixer_publish_route();
    if (!was_open) return;
    mixer_control_view(control,now,&after);
    int presence_changed = shown_presence[0] != after.present[0] || shown_presence[1] != after.present[1];
    shown_presence[0] = after.present[0];
    shown_presence[1] = after.present[1];
    if (!mixer_control_active(control) || before.revision != after.revision || before.pending != after.pending || presence_changed)
        note_native_repaint_sample();
    else
        note_native_repaint_sample_region(0,102,320,93);
}

static void mixer_pad_ui(const struct phone_event *e, struct mixer_control *control, int suppress)
{
    _Alignas(8) unsigned char raw[40];
    memcpy(raw,e->native,16);
    memcpy(raw+16,e->midi,24);
    unsigned bit = 1u << e->pad;
    int current = control && e->generation == mixer_binding;
    if (!e->down && (mixer_native_pads & bit)) {
        mixer_native_pads &= ~bit;
        if (e->up) mixer_forward_pad(raw);
    }
    if (!current) return;
    if (e->kind == PHONE_MIXER_ENTRY || (e->pad == 8 && !e->down && mixer_entry.held)) {
        int ready = mixer_session_status(mixer_ui_session) == 1 && !atomic_load(&mixer_input_fault);
        enum mixer_entry_action action = mixer_entry_pad(&mixer_entry,e->pad,e->down,
            e->when_ms,e->generation,1,ready,mixer_ui_blocked());
        if (e->down && action == MIXER_ENTRY_CONSUME) memcpy(mixer_entry_down,raw,sizeof(raw));
        if (action == MIXER_ENTRY_OPEN && mixer_control_open(control,ready,0)) {
            int bank = step_native_browser_bank(-1);
            if (bank >= 0 && bank < 8)
                mixer_control_presence(control,0,0,(unsigned)bank);
            mixer_input_button(control,0x31,atomic_load(&native_buttons_down[0x31]),mixer_control_ticket(control));
        }
        if (action == MIXER_ENTRY_LONG_PAIR && e->up && mixer_replay_entry()) mixer_forward_pad(raw);
        if (!suppress && action == MIXER_ENTRY_PASS && e->down) {
            mixer_forward_pad(raw);
            mixer_native_pads |= bit;
        }
    } else if (!mixer_control_pad(control,e->pad,e->down,mixer_control_ticket(control)) && e->down && !suppress) {
        mixer_forward_pad(raw);
        mixer_native_pads |= bit;
    }
    if (e->pad == 8 && !e->down) {
        mixer_publish_route();
        atomic_store(&mixer_pending,0);
    }
}

static void mixer_button_ui(struct phone_event *e, struct mixer_control *control, int suppress)
{
    unsigned code = e->native[8];
    if (!e->down && mixer_native_buttons[code]) {
        mixer_native_buttons[code] = 0;
        forward_buttons(e->native);
        return;
    }
    int mode = code == 6 || code == 7;
    if (!control || e->generation != mixer_binding) {
        if (mode && e->down) {
            mixer_native_buttons[code] = 1;
            forward_buttons(e->native);
        }
        return;
    }
    if (e->down && mode) mixer_entry_cancel(&mixer_entry);
    int was_open = mixer_control_active(control);
    if (!mixer_input_button(control,code,e->down,mixer_control_ticket(control)) && e->down &&
        (!suppress || mode)) {
        mixer_native_buttons[code] = 1;
        forward_buttons(e->native);
    }
    if (was_open && !mixer_control_active(control)) note_native_repaint_sample();
}

static int mixer_dispatch(struct phone_event *e)
{
    if (e->kind < PHONE_MIXER_PAD || e->kind > PHONE_MIXER_CC) return 0;
    struct mixer_control *control = mixer_session_control(mixer_ui_session);
    int blocked = control && mixer_ui_blocked();
    if (blocked && mixer_control_active(control)) mixer_fail_input();
    int suppress = atomic_load(&mixer_input_fault) || atomic_load(&mixer_project_invalidated) ||
        mixer_session_status(mixer_ui_session) != 1;
    if (control && (suppress || blocked)) mixer_close_ui();
    switch (e->kind) {
    case PHONE_MIXER_ENTRY:
    case PHONE_MIXER_PAD:
        mixer_pad_ui(e,control,suppress);
        break;
    case PHONE_MIXER_BUTTON:
        mixer_button_ui(e,control,suppress);
        break;
    case PHONE_MIXER_CC:
        if (!suppress && control && e->generation == mixer_binding &&
            !mixer_control_cc(control,e->native[8],e->native[9],mixer_control_ticket(control)))
            ORIG(H_CONTROLLER,void(*)(void*))(e->native);
        break;
    default:
        return 0;
    }
    mixer_publish_route();
    if (mixer_control_active(control)) note_native_repaint_sample();
    return 1;
}
#endif


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
        send_pad_rgb((void*)s,colour,i);
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
        send_pad_rgb((void*)s,colour,i);
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
#ifdef MPC_MIXER_AUTOBIND
    mixer_runtime_ui_tick();
#endif
    unsigned action_batch=process_events();
    if(!action_batch)atomic_fetch_sub(&ui_dispatching,1);
#ifdef MPC_MIXER_LIVE
    mixer_tick_ui();
#endif
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
    note_native_mix_shift_tick();
    int music_running=music_ui_tick();
    int step_running=step_native_tick();
    int workflow_running=workflow_tick();
#ifdef MPC_REAR_ROUTING
    int rear_running=rear_ui_tick();
#endif
#ifdef MPC_COMPRESSOR
    int compressor_running=compressor_ui_tick();
#endif
#ifdef MPC_SONG_UI
    song_ui_tick();
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
    unsigned tail=tail_mode();
    atomic_store(&tail_layout,tail);workflow_tail_tick(tail!=0);
    send_step_lights();send_record_light();
    music_lighting_tick();
    if(action_batch)atomic_fetch_sub(&ui_dispatching,1);
    if (editor && !visible()) stop();
    if (!editor) {
        if (!note_running && !music_running && !step_running && !workflow_running && !resample_active && !step_native_active() && !phone_events_pending()
#ifdef MPC_COMPRESSOR
            && !compressor_running
#endif
#ifdef MPC_REAR_ROUTING
            && !rear_running
#endif
#ifdef MPC_POWER_BUTTON
            && !power_control_busy()
#endif
#ifdef MPC_MIXER_AUTOBIND
            && 0
#elif defined(MPC_MIXER_LIVE)
            && !mixer_ui_session
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
#ifdef MPC_KNOB_RESET
    knob_reset_observe(p);
#endif
    int painted=note_native_footer(p,g);
    if (painted) start_timer();
    return painted;
}

static void sample_dtor(void *p)
{
#ifdef MPC_KNOB_RESET
    knob_reset_forget(p);
#endif
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
#ifdef MPC_MIXER_LIVE
static void mixer_paint_rect(void *g,int x,int y,int w,int h,uint32_t colour)
{
    FN(0x102d5e0,void(*)(void*,uint32_t))(g,colour);
    FN(0x102e000,void(*)(void*,int,int,int,int))(g,x,y,w,h);
}

static void mixer_paint_text(void *g,const char *text,int x,int y,int w,int h,unsigned size,uint32_t colour,int centre)
{
    uint64_t str = 0, font = 0;
    FN(0xf535c0,void(*)(uint64_t*,const char*))(&str,text);
    FN(0x1082810,void(*)(uint64_t*,int,float))(&font,0,(float)size);
    FN(0x102dca0,void(*)(void*,const uint64_t*))(g,&font);
    FN(0x102d5e0,void(*)(void*,uint32_t))(g,colour);
    FN(0x10817b0,void(*)(void*,const uint64_t*,int,int,int,int,int,int,float))
        (g,&str,x,y,w,h,centre?36:33,1,0.75f);
    FN(0x1070d60,void(*)(void*))(&font);
    FN(0xf0a290,void(*)(void*))(&str);
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
#ifdef MPC_MIXER_LIVE
    struct mixer_control *control = mixer_session_control(mixer_ui_session);
    if (mixer_control_active(control) && !mixer_ui_blocked()) {
        struct mixer_view_state view;
        const struct resample_painter painter = {g,mixer_paint_rect,mixer_paint_text};
        mixer_control_view(control,now_ms(),&view);
        mixer_view_paint(&view,&painter);
    }
#endif
#ifdef MPC_STARTUP_SPLASH
    if(workflow_startup_loading())startup_splash_draw(base,g);
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
void rear_ui_wake(void){if(!ui_known){ui_thread=pthread_self();ui_known=1;}if(on_ui())start_timer();}
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
static int sample_file_copy(const uint64_t*source,const uint64_t*destination){
 uintptr_t caller=(uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0));
 return sample_load_copy_trace(base,caller,source,destination,ORIG(H_SAMPLE_FILE_COPY,sample_file_copy_fn));
}
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
#ifdef MPC_REAR_ROUTING
    rear_native_bind(p,a);
#endif
#ifdef MPC_RESAMPLE_TRIAL
    resample_ui_bind(p);
#endif
#ifdef MPC_MIXER_AUTOBIND
    mixer_sources_bind(base,p);
#endif
}
static void copy_context_dtor(void*p){
#ifdef MPC_REAR_ROUTING
    rear_native_dead(p);
#endif
#ifdef MPC_MIXER_AUTOBIND
    mixer_sources_stop(p);
#endif
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
    int original_pad=FIELD(properties,0x228,int),pitch=0;
    /* Transpose16 and the Degrees layout replace native's chromatic pitch.
     * DSP reads one atomic snapshot: no allocation, file I/O or mutex. */
    if (FIELD(properties,0x88,int)==1 && pad>=0 &&
        music_layout_pad_pitch(music_published(),(unsigned)(pad%16),original_pad,&pitch))
        FN(0x27e4400,void(*)(void*,int,int,int))((char*)event+0x14,0,0,pitch);
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
#ifdef MPC_REAR_ROUTING
    rear_native_project_changed();
#endif
#ifdef MPC_MIXER_LIVE
    mixer_project_changing();
#endif
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
#ifdef MPC_REAR_ROUTING
    rear_native_project_changed();
#endif
#ifdef MPC_MIXER_LIVE
    mixer_project_changing();
#endif
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
#ifdef MPC_REAR_ROUTING
    if(confirmed){rear_native_project_changed();rear_settings_reset();}
#endif
#ifdef MPC_MIXER_LIVE
    if(confirmed)mixer_project_changing();
#endif
#ifdef MPC_RESAMPLE_TRIAL
    if(confirmed)resample_ui_project_changing();
#endif
    if (confirmed) step_native_project_changed();
    ORIG(H_NEW_PROJECT,void(*)(void*,int*))(p,choice);
#ifdef MPC_EARLY_STARTUP
    if (confirmed) startup_project_changed();
#endif
    if (confirmed) {
        music_reset();
#ifdef MPC_MIXER_ARCHIVE
        mixer_archive_reset();
#endif
    }
#ifdef MPC_PROJECT_RECOVERY
    if (confirmed) project_recovery_reset();
#endif
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
#ifdef MPC_PROJECT_RECOVERY
    project_recovery_save_paint(base,p,g);
#endif
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
#ifdef MPC_REAR_ROUTING
    if(on_ui()&&rear_ui_control(event))return;
#endif
#ifdef MPC_COMPRESSOR
    if(on_ui()&&compressor_ui_owns(PHONE_CONTROLLER,cc)&&compressor_ui_control(event,compressor_ui_ticket()))return;
#endif
#ifdef MPC_CHORDS
    if(on_ui()&&chords_ui_control(event))return;
#endif
    if (on_ui() && step_native_owns(cc) && step_native_control(event,route)) return;
    if (on_ui() && (music_ui_control(event) || note_native_control(event))) return;
    forward_controller(event);
    if (cc == 0x64 && on_ui() && editor && visible()) after_native(1);
}

static void buttons_ui(void *event,uint64_t route)
{
#ifdef MPC_PROJECT_RECOVERY
    if(on_ui()&&project_recovery_save_button(base,screen,event))return;
#endif
    int naming=on_ui()&&editor&&visible();
#ifdef MPC_REAR_ROUTING
    if(!naming&&on_ui()&&rear_ui_button(event))return;
#endif
    unsigned code=FIELD(event,8,unsigned char),down=FIELD(event,9,unsigned char)!=0;
    if(naming&&code==0x09){
        /* Erase deletes in both input modes: before the cursor with pads, the
         * selected character with the encoder. Both edges are consumed. */
        if(down&&(state.enabled||read_native())){
            ++internal;phone_delete(&state,!state.enabled&&state.cursor<state.length);--internal;
            write_native();repaint();
        }
        return;
    }
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
#ifdef MPC_REAR_ROUTING
    rear_ui_fault();
#endif
#ifdef MPC_MIXER_LIVE
    if (mixer_capture_route()) mixer_fail_input();
#endif
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
    if(kind==PHONE_CONTROLLER)e.down=atomic_load(&native_shift_requested);
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
#ifdef MPC_REAR_ROUTING
    if(!naming&&rear_ui_owns(kind,FIELD(native,8,unsigned char)))e.pad=128u;
#endif
#ifdef MPC_SONG_UI
    if(!naming&&song_ui_owns(kind,FIELD(native,8,unsigned char),(atomic_load(&native_shift_requested)||atomic_load(&native_buttons_down[0x31])),kind!=PHONE_BUTTON||FIELD(native,9,unsigned char)!=0))e.pad=256u;
#endif
    if (phone_events_push(&e)) return 1;
    queue_failed();
    if (e.pad) return 1;
    return 0;
}

static int forward_main_volume(void*event)
{
    if(FIELD(event,8,unsigned char)!=0x13)return 0;
    forward_controller(event);return 1;
}
static int forward_knob_fx(void *event){
 unsigned cc=FIELD(event,8,unsigned char);
 if(cc<0x10||cc>0x12||!music_ui_knob_fx_active())return 0;
 forward_controller(event);return 1;
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
    if(!phone_events_pending()&&!atomic_load(&ui_dispatching)&&forward_knob_fx(event))return;
#ifdef MPC_KNOB_RESET
    if(!atomic_load(&naming_active)&&knob_reset_claim(FIELD(event,8,unsigned char))){
        struct phone_event e={0};e.kind=PHONE_KNOB_RESET;e.when_ms=now_ms();
        e.workflow_ticket=knob_reset_ticket();memcpy(e.native,event,sizeof(e.native));
        if(!phone_events_push(&e))queue_failed();
        return;
    }
#endif
#ifdef MPC_POWER_BUTTON
    if(power_control_busy())return;
#endif
    /* Observing Sample/Seq is not ownership of its native parameter knobs.
     * Keep native K1-K3 on the original input thread, without the UI timer's
     * 50ms batching or coalescing. Custom controls still cross to the UI.
     * An outstanding FIFO action is a barrier (page/Shift/ownership may
     * change there); never let a fresh movement overtake that action. */
    unsigned native_cc=FIELD(event,8,unsigned char);
#ifdef MPC_MIXER_LIVE
    uint64_t mixer_ticket = mixer_capture_route();
    if (mixer_ticket && ((native_cc >= 0x10 && native_cc <= 0x12) || native_cc == 0x14 || native_cc == 0x64)) {
        struct phone_event e = {.kind=PHONE_MIXER_CC,.generation=mixer_ticket};
        memcpy(e.native,event,16);
        if (!phone_events_push(&e)) mixer_fail_input();
        return;
    }
#endif
    int custom_owned=atomic_load(&naming_active)||resample_running()||
        note_native_owns(native_cc)||music_ui_owns(native_cc)||
        step_native_owns(native_cc)||workflow_owns(PHONE_CONTROLLER,native_cc)
#ifdef MPC_REAR_ROUTING
        ||rear_ui_owns(PHONE_CONTROLLER,native_cc)
#endif
#ifdef MPC_CHORDS
        ||chords_ui_owns(native_cc)
#endif
#ifdef MPC_COMPRESSOR
        ||compressor_ui_owns(PHONE_CONTROLLER,native_cc)
#endif
#ifdef MPC_SONG_UI
        ||song_ui_owns(PHONE_CONTROLLER,native_cc,0,1)
#endif
        ;
    if(native_cc>=0x10&&native_cc<=0x12&&!custom_owned&&
       !phone_events_pending()&&!atomic_load(&ui_dispatching)){
        forward_controller(event);return;
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
    forward_controller(event);
}

static void buttons(void *event)
{
    unsigned native_code=FIELD(event,8,unsigned char),native_down=FIELD(event,9,unsigned char)!=0;
    /* r27q-fix6 diagnostic: physical button codes (Sample/Seq/... are not
     * mapped in the repo yet), with the 16 Levels/Chop state, 300 presses. */
    {static unsigned logged;if(native_down&&logged<300){++logged;unsigned on=0,mode=0,origin=0;music_lighting_levels_state(&on,&mode,&origin);
        fprintf(stderr,"BUTTON_DOWN code=%02x levels=%u/%u\n",native_code,on,mode);}}
    if(native_code==0x51)atomic_store(&stop_requested,native_down);
    if(native_code==0x51&&native_down){pthread_mutex_lock(&stop_template_mutex);memcpy(stop_template,event,16);stop_template_ready=1;pthread_mutex_unlock(&stop_template_mutex);}
#ifdef MPC_KNOB_RESET
    knob_reset_button(native_code,native_down);
#endif
#ifdef MPC_MIXER_LIVE
    if ((native_code == 6 || native_code == 7) && (mixer_mode_forwarded & (1u << native_code))) {
        if (!native_down) {
            mixer_mode_forwarded &= ~(1u << native_code);
            forward_buttons(event);
        }
        return;
    }
    if (native_code < 112 && mixer_button_pairs[native_code]) {
        struct phone_event e = {.kind=PHONE_MIXER_BUTTON,.down=native_down};
        memcpy(e.native,event,16);
        if (mixer_queue_pair(&e,&mixer_button_pairs[native_code])) return;
    }
#endif
#ifdef MPC_POWER_BUTTON
    pthread_mutex_lock(&gesture_mutex);memcpy(power_stop_template,event,16);power_stop_ready=1;pthread_mutex_unlock(&gesture_mutex);
    if(power_control_button(native_code,(int)native_down)){
        if(!native_down&&native_code<112&&atomic_load(&native_buttons_down[native_code]))forward_buttons(event);
        return;
    }
#endif
    if(native_code==0x31)atomic_store(&native_shift_requested,native_down);
#ifdef MPC_MIXER_LIVE
    if (native_code < 112 && native_code != 0x31) {
        uint64_t *held = &mixer_button_pairs[native_code];
        uint64_t ticket = mixer_capture_route();
        if (ticket && native_down && !atomic_load(&native_buttons_down[native_code]) &&
            mixer_input_action(native_code,NULL)) {
            struct phone_event e = {.kind=PHONE_MIXER_BUTTON,.generation=ticket,.down=native_down};
            memcpy(e.native,event,16);
            if (mixer_queue_pair(&e,held)) {
                if (*held != UINT64_MAX || (native_code != 6 && native_code != 7)) return;
                *held = 0;
                /* Queue fault cancels Mixer edits. Keep both edges of a native
                 * mode key on this path so its release cannot overtake its down. */
                mixer_mode_forwarded |= 1u << native_code;
                forward_buttons(event);
                return;
            }
        }
    }
#endif
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
#ifdef MPC_MIXER_LIVE
        || (native_code == 0x31 && mixer_capture_route())
#endif
#ifdef MPC_RESAMPLE_TRIAL
        ||resample_ui_owns(PHONE_BUTTON,native_code)
#endif
#ifdef MPC_CHORDS
        ||chord_owned
#endif
#ifdef MPC_COMPRESSOR
        ||compressor_ui_owns(PHONE_BUTTON,native_code)
#endif
#ifdef MPC_REAR_ROUTING
        ||rear_ui_owns(PHONE_BUTTON,native_code)
#endif
#ifdef MPC_SONG_UI
        ||song_ui_owns(PHONE_BUTTON,native_code,(atomic_load(&native_shift_requested)||atomic_load(&native_buttons_down[0x31])),(int)native_down)
#endif
        )
        && queue_native(event,PHONE_BUTTON)) return;
    forward_buttons(event);
}

static void pad_event(void *event)
{
    unsigned pad = FIELD(event,8,unsigned char);
    void *msg = (char*)event + 0x10;
    if (pad >= 16) {
#ifdef MPC_POWER_BUTTON
        if (power_control_busy()) return;
#endif
        ORIG(H_PAD,void(*)(void*))(event); return;
    }
    unsigned bit = 1u << pad;
    int down = FN(0xddfed0,int(*)(void*,int))(msg,0);
    int up = FN(0xddff20,int(*)(void*,int))(msg,1);
#ifdef MPC_MIXER_LIVE
    if (mixer_pad_pairs[pad] && mixer_queue_pad(event,pad,down,up)) return;
#endif
#ifdef MPC_POWER_BUTTON
    if (power_control_busy()) return;
#endif
    if(atomic_load(&tail_pads)&bit){if(up)atomic_fetch_and(&tail_pads,~bit);return;}
    /* Shift+Pad16 is the stock Knob FX shortcut; it must not open Knob FX
     * over the browser (device request). Swallow that pad's press/release. */
    {static unsigned knob_fx_pad;
     if((knob_fx_pad&bit)||(down&&pad==15&&atomic_load(&native_shift_requested)&&workflow_browser_shown&&workflow_browser_shown()&&!workflow_owns(3,pad))){
        if(down){knob_fx_pad|=bit;}
        if(up){knob_fx_pad&=~bit;}
        return;}}
    if (passed & bit) {
        if (up) passed &= ~bit;
        ORIG(H_PAD,void(*)(void*))(event); return;
    }
#ifdef MPC_KNOB_RESET
    if((atomic_load(&bank_pad_owned)&bit)||(down&&(knob_reset_ticket()&2)&&!atomic_load(&naming_active)&&!workflow_owns(3,pad)
#ifdef MPC_RESAMPLE_TRIAL
       &&!resample_ui_active()
#endif
       )){
        if(down)atomic_fetch_or(&bank_pad_owned,bit);
        if(down||up){struct phone_event e={0};e.kind=PHONE_BANK_PAD;e.pad=pad;e.down=down;e.up=up;
            memcpy(e.native,event,16);memcpy(e.midi,msg,24);if(!phone_events_push(&e))queue_failed();}
        if(up)atomic_fetch_and(&bank_pad_owned,~bit);
        return;
    }
#endif
#ifdef MPC_MIXER_LIVE
    unsigned other_pads = atomic_load(&consumed) | atomic_load(&consumed_workflow) | atomic_load(&shortcut_pads);
#ifdef MPC_RESAMPLE_TRIAL
    other_pads |= atomic_load(&consumed_resample);
#endif
    if (!(other_pads & bit) && mixer_queue_pad(event,pad,down,up)) return;
#endif
#ifdef MPC_CHORDS
    if((down||up)&&chords_parameter(pad,down))return;
#endif
#ifdef MPC_REAR_ROUTING
    if((atomic_load(&consumed_rear)&bit)||(!atomic_load(&naming_active)&&rear_ui_pad_owns())){
        if(down)atomic_fetch_or(&consumed_rear,bit);
        if(down||up){struct phone_event e={0};e.kind=PHONE_REAR_PAD;e.pad=pad;e.down=(unsigned)down;e.up=(unsigned)up;if(!phone_events_push(&e))queue_failed();}
        if(up)atomic_fetch_and(&consumed_rear,~bit);
        return;
    }
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
    unsigned tail=atomic_load(&tail_layout);
    if(down&&!owned&&!pad_route&&tail&&atomic_load(&stop_requested)&&!atomic_load(&native_shift_requested)){
        atomic_fetch_or(&tail_pads,bit);
        struct phone_event e={0};e.kind=PHONE_TAIL_PAD;e.pad=pad;e.workflow_ticket=tail;
        if(!phone_events_push(&e))queue_failed();
        return;
    }
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
        if(e.kind==PHONE_TAIL_PAD){
            unsigned tail=tail_mode();
            if(tail&&tail==e.workflow_ticket){
                /* Device report: the pad already sounding kept playing under
                 * its own tail. Second Stop (still held) silences it first. */
                unsigned char ev[16];int ready;
                pthread_mutex_lock(&stop_template_mutex);memcpy(ev,stop_template,16);ready=stop_template_ready;pthread_mutex_unlock(&stop_template_mutex);
                if(ready&&atomic_load(&stop_requested)){ev[8]=0x51;ev[9]=0;forward_buttons(ev);ev[9]=1;forward_buttons(ev);note("TAIL_PREVIEW SECOND_STOP");}
                workflow_tail_preview(e.pad,tail==2);
            }
            continue;
        }
#ifdef MPC_KNOB_RESET
        if(e.kind==PHONE_BANK_PAD){
#ifdef MPC_REAR_ROUTING
            if(rear_ui_pad_owns()){
                if(e.down&&e.pad<8&&!rear_native_busy())step_native_browser_bank((int)e.pad);
                continue;
            }
#endif
            _Alignas(8) unsigned char native[40];memcpy(native,e.native,16);memcpy(native+16,e.midi,24);
            ORIG(H_PAD,void(*)(void*))(native);continue;
        }
        if(e.kind==PHONE_KNOB_RESET){
            unsigned cc=FIELD(e.native,8,unsigned char),shift=!!(e.workflow_ticket&1),column=cc-0x10;
            /* Mod windows that own the K knobs (browser, Batch, Rear Routing)
             * reset their own values; the Sample chain below stays blocked
             * behind them as before. */
            if(knob_reset_begin(e.workflow_ticket,cc)&&!atomic_load(&naming_active)&&!resample_running()){
              if(workflow_blocks_sample_controls()){
                if(!workflow_reset_knob(column,shift))note("KNOB_RESET no_default_for_current_parameter");
              }else{
                int done=0;
#ifdef MPC_REAR_ROUTING
                done=rear_ui_reset_knob?rear_ui_reset_knob(column,shift):0;
#endif
#ifdef MPC_COMPRESSOR
                if(!done)done=compressor_ui_reset_knob(column,shift);
#endif
#ifdef MPC_CHORDS
                if(!done)done=chords_ui_reset_knob(column,shift);
#endif
                if(!done)done=step_native_reset_knob(column,shift);
                if(!done)done=music_ui_reset_knob(column,shift);
                if(!done)done=note_native_reset_knob(column,shift);
                if(!done)done=knob_reset_native(column,shift);
                if(!done)note("KNOB_RESET no_default_for_current_parameter");
              }
            }
            continue;
        }
#endif
#ifdef MPC_MIXER_LIVE
        if (mixer_dispatch(&e)) continue;
#endif
        int current = !atomic_load(&editor_destroyed) && editor &&
            e.generation == atomic_load(&generation) && visible();
        if (e.kind == PHONE_CONTROLLER) {
            if(forward_knob_fx(e.native))continue;
            if (current && !(seen&1)) { seen|=1; note("FIFO_CONTROLLER_ON_UI"); }
            if (e.pad) {
                /* Never replay a claimed metadata edit as native audio pitch. */
#ifdef MPC_SONG_UI
                if(e.pad&256u){if(!song_ui_control(e.native))forward_controller(e.native);}
                else
#endif
#ifdef MPC_REAR_ROUTING
                if(e.pad&128u){if(!rear_ui_control(e.native))forward_controller(e.native);}
                else
#endif
#ifdef MPC_COMPRESSOR
                if(e.pad&64u)compressor_ui_control(e.native,e.workflow_ticket);
                else
#endif
#ifdef MPC_CHORDS
                if(e.pad&32u)chords_ui_control(e.native);
                else
#endif
#ifdef MPC_RESAMPLE_TRIAL
                if(e.pad&16u){if(!resample_ui_control(e.native,e.workflow_ticket))forward_controller(e.native);}
                else
#endif
                if (e.pad&8u){if(!workflow_control(e.native,e.workflow_ticket))forward_controller(e.native);}
                else if (e.pad&4u) step_native_control(e.native,e.step_ticket);
                else if (e.pad&2u) music_ui_control_at(e.native,e.down);
                else note_native_control(e.native);
            } else if (current || note_native_active() || music_ui_active() || step_native_active()
#ifdef MPC_COMPRESSOR
                       ||compressor_ui_owns(PHONE_CONTROLLER,FIELD(e.native,8,unsigned char))
#endif
                       ) controller_ui(e.native,e.step_ticket);
            else forward_controller(e.native);
        } else if (e.kind == PHONE_BUTTON) {
            if (current && !(seen&2)) { seen|=2; note("FIFO_BUTTON_ON_UI"); }
#ifdef MPC_SONG_UI
            if(e.pad&256u){if(!song_ui_button(e.native))forward_buttons(e.native);}
            else
#endif
#ifdef MPC_REAR_ROUTING
            if(e.pad&128u){if(!rear_ui_button(e.native))forward_buttons(e.native);}
            else
#endif
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
            else if (e.pad&2u) music_buttons(e.native);
            else if (current || note_native_active() || music_ui_active() || step_native_active()
#ifdef MPC_COMPRESSOR
                    ||compressor_ui_owns(PHONE_BUTTON,FIELD(e.native,8,unsigned char))
#endif
                    ) buttons_ui(e.native,e.step_ticket);
            else forward_buttons(e.native);
            /* Let native FX visibility/LED listeners finish before the next
             * queued K movement chooses between FX and an underlying page. */
            if(FIELD(e.native,8,unsigned char)==0x00&&FIELD(e.native,9,unsigned char))break;
        }
#ifdef MPC_REAR_ROUTING
        else if(e.kind==PHONE_REAR_PAD){rear_ui_pad(e.pad,(int)e.down);}
#endif
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
#ifdef MPC_MIXER_LIVE
        if (e.kind == PHONE_BUTTON && e.native[8] == 0x31 && mixer_ui_session) {
            struct mixer_control *control = mixer_session_control(mixer_ui_session);
            mixer_input_button(control,0x31,atomic_load(&native_buttons_down[0x31]),mixer_control_ticket(control));
        }
#endif
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
    int enabled=music_pad_lighting_mode()&&music_ui_sequence_visible()&&bank>=0&&bank<8&&!(step_native_transport_state()&4u)&&
        !atomic_load(&native_mute_mode)&&!atomic_load(&pad_bank_held_long);
    static struct sequence_clock clock;
    static int previous_bank=-1,previous_transport=-1,previous_selected=-1;
    if(!enabled){clock.valid=0;previous_bank=-1;atomic_store(&sequence_palette_enabled,0);return;}
    unsigned present=music_ui_sequence_presence((unsigned)bank);
    int selected=music_ui_sequence_selected();
    int transport=(step_native_transport_state()&3u)==3u;
    uint64_t serial;unsigned running=0;int64_t tick=step_native_clock_ticks(&serial,&running);
    uint64_t now=now_ms();struct sequence_timing timing=sequence_timing();
    if(!transport&&selected!=previous_selected)clock.valid=0;
    static uint64_t selected_since;if(selected!=previous_selected)selected_since=now;
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
        unsigned index=(unsigned)bank*16+i;
        int queued=transport&&pulsing[i]&&(int)index!=selected;
        unsigned brightness=sequence_brightness(!!(present&(1u<<i)),index,selected,playing,when,queued,&timing);
        int chosen=music_sequence_colour((unsigned)bank*16+i);
        unsigned colour=music_identity_rgb(chosen);
        /* Agreed: the selected sequence pad blinks white like the Sample
         * cursor (Mod Settings Cursor pulse; Steady keeps it white). */
        if((int)index==selected&&!queued&&music_cursor_on(now-selected_since,500)){
            atomic_store(&sequence_palette[i],pad_light(PAD_SELECT,.95f));continue;
        }
        if((int)index==selected&&brightness<96)brightness=96;
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
static _Atomic unsigned pad_colour_sample_layout,pad_colour_native_workflow,pad_colour_cursor_white,pad_colour_source_white;
static _Atomic int pad_colour_native_occupied=-1;
static _Atomic int pad_colour_native_selected=-1;
static _Atomic uint64_t pad_colour_chop_phase,pad_colour_target_phase;
static _Atomic int pad_colour_chop_source=-1;
static _Atomic int pad_colour_copy_source=-1; /* latched native Copy source, absolute index */
static _Atomic int pad_colour_bank;
/* Sample pad layout: the user's Pad Colour (Mix, K2) keeps the
 * native brightness of each pad (dim loaded, bright playing) and replaces
 * only its hue. Stock (0) passes the native colour through. */
static int pad_colour_light(unsigned pad,uint64_t *out){
    if(!music_pad_lighting_mode()||pad>=16||!atomic_load(&pad_colour_sample_layout)||atomic_load(&led_enabled))return 0;
    int bank=atomic_load(&pad_colour_bank);if(bank<0||bank>=8)return 0;
    int chosen=music_pad_colour((unsigned)bank*16+pad);
    if(atomic_load(&pad_colour_native_workflow)){
        unsigned kind=atomic_load(&pad_colour_native_workflow);
        unsigned raw=(unsigned)atomic_load(&original_lights[pad]);
        int occupied=atomic_load(&pad_colour_native_occupied);
        int used=occupied>=0&&((unsigned)occupied&(1u<<pad));
        uint64_t phase=atomic_load(&pad_colour_target_phase);
        unsigned index=(unsigned)bank*16+pad;
        if(kind==2||kind==3){
            /* Native Copy encoding (device log): source = pulsing pure green
             * (003000..00e800, 000000 in its off phase), chosen target =
             * ff0000 whether free or occupied. Agreed rules: source blinks
             * white/own colour, target free = Free colour, target occupied =
             * Occupied colour (steady), others own colour dim, empty off. */
            uint32_t hue=music_identity_rgb((unsigned)(kind==3?(music_sequence_colour?music_sequence_colour(index):0):music_pad_colour(index)));
            unsigned r=(raw>>16)&255,g=(raw>>8)&255,b=raw&255;
            if(!r&&!b&&g)atomic_store(&pad_colour_copy_source,(int)index);
            if((int)index==atomic_load(&pad_colour_copy_source)&&raw!=0xff0000){
                *out=pad_source_light(used?hue:PAD_SELECT,phase);return 1;
            }
            if(raw==0xff0000){*out=pad_light(used?PAD_DANGER:PAD_FREE,used?PAD_BRIGHT_DANGER:.60f);return 1;}
            *out=pad_light(used?hue:0,used?.18f:0);return 1;
        }
        int muted=used&&(atomic_load(&pad_colour_native_muted)&(1u<<pad));
        /* Browser + Mute (device report: red, never grey): native Mute
         * emits ff0000 muted / ff7700 playable. Agreed Mute look: own colour
         * brighter, muted = Occupied/danger colour; outside Mute, muted
         * loaded pads are grey (Muted colour) as on the Sample screen. */
        if(kind==1&&(atomic_load(&native_mute_mode)||atomic_load(&native_buttons_down[0x2b]))){
            if(!used){*out=pad_light(0,0);return 1;}
            *out=raw==0xff0000u||muted?pad_light(PAD_DANGER,.80f):pad_light(music_identity_rgb((unsigned)chosen),.70f);return 1;
        }
        /* Sample browser (kind 1): selected load pad. */
        if(kind==1&&atomic_load(&pad_colour_native_selected)==(int)index){
            *out=pad_source_light(used?music_identity_rgb((unsigned)chosen):0,phase);return 1;
        }
        if(occupied>=0){
            int marked=raw==0xff0000||raw==0xffffff;
            if(muted&&!marked){*out=pad_light(PAD_MUTED,.32f);return 1;}
            *out=pad_target_colour(index,used,marked,marked,phase);return 1;
        }
        return 0;
    }
    /* Mute mode and held Mute (0x23) still show the native colours (their
     * per-pad meaning is not decoded yet). Held Erase (0x09) keeps the user's
     * pad colours (agreed rules, PREF-05): occupied pads stay in their hue at
     * content brightness, empty pads stay off. */
    if(atomic_load(&native_mute_mode)||atomic_load(&pad_bank_held_long))return 0;
    int erase_held=atomic_load(&native_buttons_down[0x09]);
    unsigned enabled=0,mode=0,origin=0;music_lighting_levels_state(&enabled,&mode,&origin);
    if(enabled&&mode==8){
        unsigned raw=(unsigned)atomic_load(&original_lights[pad]);
        unsigned top=(raw>>16)&255;if(((raw>>8)&255)>top)top=(raw>>8)&255;if((raw&255)>top)top=raw&255;
        int source=atomic_load(&pad_colour_chop_source);
        if(source<0||source>=128)return 0;
        uint32_t hue=music_identity_rgb((unsigned)music_pad_colour((unsigned)source));
        *out=raw==0xff7700?pad_focus_light(hue,PAD_BRIGHT_CONTENT,atomic_load(&pad_colour_chop_phase)):pad_light(top?hue:0,top?PAD_BRIGHT_CONTENT:0);
        return 1;
    }
    if(enabled||(step_native_active()&&!step_native_sound_layout())
#ifdef MPC_CHORDS
       ||chords_active()
#endif
       )return 0;
    unsigned old=(unsigned)atomic_load(&original_lights[pad]);
    unsigned level=(old>>16)&255;if(((old>>8)&255)>level)level=(old>>8)&255;if((old&255)>level)level=old&255;
    if(!level)return 0;
    unsigned colour=music_identity_rgb(chosen);
    if(erase_held){*out=pad_light(colour,PAD_BRIGHT_CONTENT);return 1;}
    unsigned red=((colour>>16)&255)*level/255,green=((colour>>8)&255)*level/255,blue=(colour&255)*level/255;
    float intensity=1;uint32_t bits;memcpy(&bits,&intensity,4);
    *out=(uint64_t)((red<<16)|(green<<8)|blue)|((uint64_t)bits<<32);return 1;
}
#ifdef MPC_MIXER_LIVE
static int mixer_light(unsigned pad,uint64_t *out)
{
    uint32_t native=(uint32_t)atomic_load(&original_lights[pad]);
    if(!mixer_leds_light(pad,native,out))return 0;
    if(atomic_load(&mixer_project_invalidated)||atomic_load(&mixer_input_fault)||
       atomic_load(&native_mute_mode)||atomic_load(&native_buttons_down[0x2b])||
       atomic_load(&native_buttons_down[0x09]))
        *out=(UINT64_C(0x3f800000)<<32)|native;
    return 1;
}
#endif
static void rgb(void *s,uint32_t colour,unsigned pad)
{
    if (pad < 16) {
        atomic_store(&sender,(uintptr_t)s);
        atomic_store(&original_lights[pad],colour);
        step_native_observe_rgb(pad,colour);
        atomic_fetch_or(&light_mask,1u << pad);
#ifdef MPC_MIXER_LIVE
        mixer_leds_native_rgb(pad);
#endif
        if(atomic_load(&startup_pads))return; /* startup row animation owns the pads */
#ifdef MPC_KNOB_RESET
        if((knob_reset_ticket()&2)&&atomic_load(&pad_bank_held_long)){ORIG(H_RGB,void(*)(void*,uint32_t,unsigned))(s,colour,pad);return;}
#endif
        uint64_t workflow_colour;
#ifdef MPC_REAR_ROUTING
        if(rear_ui_light(pad,&workflow_colour)){
            uint32_t packed=FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&workflow_colour);
            ORIG(H_RGB,void(*)(void*,uint32_t,unsigned))(s,packed,pad);return;
        }
#endif
#ifdef MPC_STOCK_RESAMPLE
        if(resample_ui_light(pad,&workflow_colour)){
            uint32_t packed=FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&workflow_colour);
            send_pad_rgb(s,packed,pad);return;
        }
#endif
        if(workflow_light(pad,&workflow_colour)){
            uint32_t packed=FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&workflow_colour);
            send_pad_rgb(s,packed,pad);return;
        }
        if (atomic_load(&led_enabled)) {
            struct phone_input colours = {0};
            colours.numeric = (int)atomic_load(&led_numeric);
            colours.uppercase = (int)atomic_load(&led_uppercase);
            colours.held = atomic_load(&led_pressed);
            uint64_t replacement = phone_light(&colours,pad + 1);
            uint32_t packed = FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&replacement);
            send_pad_rgb(s,packed,pad);
            return;
        }
        uint64_t replacement;
        if (
#ifdef MPC_MIXER_LIVE
            mixer_light(pad,&replacement) ||
#endif
            workflow_light(pad,&replacement)
            ||(atomic_load(&pad_colour_native_workflow)&&pad_colour_light(pad,&replacement))
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
            send_pad_rgb(s,packed,pad);return;
        }
    }
    send_pad_rgb(s,colour,pad);
}
/* r27q-fix4 diagnostic (stderr -> MPC_CLONE_TRIAL.txt on power-off):
 * one line per change of lighting context/owners, at most 240 lines.
 * Owner per pad: W mod workflow, N native workflow, C chords, S steps,
 * Q sequence colours, P pad colour, - native passthrough. Read-only. */
static void lights_diagnostic(void){
    static unsigned lines;static char last[160];
    if(lines>=800)return;
    unsigned on=0,mode=0,origin=0;music_lighting_levels_state(&on,&mode,&origin);
    char owners[17];unsigned raw_mask=0;
    for(unsigned i=0;i<16;++i){
        uint64_t l;char o='-';
        if(workflow_light(i,&l))o='W';
        else if(atomic_load(&pad_colour_native_workflow)&&pad_colour_light(i,&l))o='N';
#ifdef MPC_CHORDS
        else if(chords_light(i,&l))o='C';
#endif
        else if(step_native_light(i,&l))o='S';
#ifdef MPC_SEQUENCE_COLOURS
        else if(sequence_light(i,&l))o='Q';
#endif
        else if(pad_colour_light(i,&l))o=(l&0xffffffu)?'P':'p';
        owners[i]=o;if(atomic_load(&original_lights[i])&0xffffffu)raw_mask|=1u<<i;
    }
    owners[16]=0;
    char line[160];
    snprintf(line,sizeof(line),"lv=%u/%u/%u nw=%u sl=%u seq=%d tr=%u erase=%d mute=%u src=%d raw=%04x own=%s",
        on,mode,origin,atomic_load(&pad_colour_native_workflow),atomic_load(&pad_colour_sample_layout),
        music_ui_sequence_visible(),step_native_transport_state(),(int)atomic_load(&native_buttons_down[0x09]),
        atomic_load(&native_mute_mode),atomic_load(&pad_colour_chop_source),raw_mask,owners);
    if(!strcmp(line,last))return;
    memcpy(last,line,sizeof(line));++lines;
    char raws[16*7+1];
    for(unsigned i=0;i<16;++i)snprintf(raws+i*7,8,"%06x ",(unsigned)atomic_load(&original_lights[i])&0xffffffu);
    fprintf(stderr,"PAD_LIGHTS %s copy=%d sel=%d occ=%x raw: %s\n",line,workflow_copy_mode_id(),
        atomic_load(&pad_colour_native_selected),(unsigned)atomic_load(&pad_colour_native_occupied),raws);
}
static void send_step_lights(void)
{
    {static uint64_t bank_since;uint64_t t=now_ms();
     if(atomic_load(&native_buttons_down[0x23])){if(!bank_since)bank_since=t;}else bank_since=0;
     atomic_store(&pad_bank_held_long,bank_since&&t-bank_since>=300);}
    /* The compressor still plays the current Sample bank. Its algorithm and
     * Bypass do not change pad ownership. Other FX keep native status colours. */
    unsigned native_workflow=workflow_native_pad_colours();
    /* Copy Sequence (kind 3): remember chosen targets (native ff0000) and,
     * when Copy closes, give every target that now holds a sequence the
     * source sequence's colour (device report: colour was not carried). */
    {static uint64_t targets[2];static int was_seq_copy;
     if(native_workflow==3){int b=step_native_browser_bank(-1);
        if(b>=0&&b<8)for(unsigned i=0;i<16;++i)if((unsigned)atomic_load(&original_lights[i])==0xff0000u){unsigned n=(unsigned)b*16+i;targets[n/64]|=1ull<<(n%64);}
        was_seq_copy=1;}
     else if(was_seq_copy){was_seq_copy=0;int source=atomic_load(&pad_colour_copy_source);
        if(source>=0&&source<128&&music_sequence_colour&&music_set_sequence_colour&&music_ui_sequence_presence){int colour=music_sequence_colour((unsigned)source);
            for(unsigned n=0;n<128;++n)if((targets[n/64]>>(n%64))&1&&(int)n!=source&&(music_ui_sequence_presence(n/16)>>(n%16)&1)){
                music_set_sequence_colour(n,(unsigned)colour);fprintf(stderr,"COPY_SEQUENCE_COLOUR %d->%u colour=%d\n",source,n,colour);}}
        targets[0]=targets[1]=0;}}
    if(native_workflow!=2&&native_workflow!=3)atomic_store(&pad_colour_copy_source,-1);
    atomic_store(&pad_colour_native_workflow,native_workflow);
    atomic_store(&pad_colour_native_occupied,workflow_native_occupied());
    atomic_store(&pad_colour_native_muted,workflow_native_muted?workflow_native_muted():0u);
    atomic_store(&pad_colour_native_selected,workflow_native_selected());
    atomic_store(&pad_colour_bank,step_native_browser_bank(-1));
    uint64_t selection_phase=now_ms();
    atomic_store(&pad_colour_cursor_white,music_cursor_on(selection_phase,500));
    atomic_store(&pad_colour_source_white,selection_phase%1000<80||(selection_phase%1000>=180&&selection_phase%1000<260));
    atomic_store(&pad_colour_target_phase,selection_phase);
    /* Resolve Chop focus on UI only. Its first frame must be white even if
     * selection occurs in the old cursor's identity phase. */
    static int last_chop=-1,last_source=-1;static uint64_t chop_since;
    unsigned on=0,mode=0,origin=0;music_lighting_levels_state(&on,&mode,&origin);
    int focus=-1,source=-1;
    if(on&&mode==8&&!native_workflow){int bank=atomic_load(&pad_colour_bank);
        /* origin is a 16-Levels reference position, not the source sound.
         * Resolve the actual retained instrument before publishing to RGB. */
        void *things=note_native_context(),*program=things?FIELD(things,0xb8,void*):NULL;
        void *instrument=things?FIELD(things,0x668,void*):NULL;
        if(program&&instrument){int index=FN(0x286bc20,int(*)(void*,void*))(program,instrument);if(index>=0&&index<128)source=index;}
        for(unsigned i=0;i<16;++i)if((unsigned)atomic_load(&original_lights[i])==0xff7700){focus=bank*16+(int)i;break;}}
    atomic_store(&pad_colour_chop_source,source);
    if(focus!=last_chop||source!=last_source){last_chop=focus;last_source=source;chop_since=selection_phase;}
    atomic_store(&pad_colour_chop_phase,selection_phase-chop_since);
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
#ifdef MPC_KNOB_RESET
    if((knob_reset_ticket()&2)&&atomic_load(&pad_bank_held_long)){
        if(s)for(unsigned i=0;i<16;++i)if(atomic_load(&light_mask)&(1u<<i))
            ORIG(H_RGB,void(*)(void*,uint32_t,unsigned))((void*)s,(uint32_t)atomic_load(&original_lights[i]),i);
        owned=0;return;
    }
#endif
    if (!s) {owned=0;return;}
    {static int was;
     int loading=startup_project_loading?startup_project_loading():workflow_startup_loading?workflow_startup_loading():0;
     loading=loading&&music_pad_lighting_mode();
     atomic_store(&startup_pads,(unsigned)loading);
     if(loading){
        /* One pad runs left to right, top to bottom: 13..16, 9..12, 5..8, 1..4. */
        unsigned step=(unsigned)((now_ms()/100u)%16u),lit=(3u-step/4u)*4u+step%4u;
        for(unsigned i=0;i<16;++i){
            uint64_t light=i==lit?pad_light(PAD_PLAYHEAD,.90f):pad_light(0,0);
            if(!(owned&(1u<<i))||previous[i]!=light){previous[i]=light;
                uint32_t packed=FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&light);
                ORIG(H_RGB,void(*)(void*,uint32_t,unsigned))((void*)s,packed,i);}
        }
        owned=0xffffu;was=1;return;
     }
     /* Loaded: let the native layer re-emit the real pad state. */
     if(was){was=0;music_lighting_request_refresh();}}
    uint64_t palette_epoch=workflow_palette_generation();
    int force_palette=palette_epoch!=workflow_generation;workflow_generation=palette_epoch;
    uint64_t workflow_probe;int workflow_colours=workflow_light(0,&workflow_probe);
#ifdef MPC_STOCK_RESAMPLE
    static uint64_t resample_generation;
    uint64_t resample_epoch=resample_ui_palette_generation();
    force_palette|=resample_epoch!=resample_generation;resample_generation=resample_epoch;
    workflow_colours|=resample_ui_light(0,&workflow_probe);
#endif
    /* A mod workflow (Extract, Copy, Stereo > Mono, browser) owned the pads
     * while the native layer may have repainted underneath (e.g. Extract
     * changes the program). On release, ask the native layer to re-emit. */
    {static int workflow_was;int now_workflow=workflow_colours||(int)native_workflow;
     if(workflow_was&&!now_workflow){music_lighting_request_refresh();}
     workflow_was=now_workflow;}
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
                   |(atomic_load(&native_mute_mode)?512u:0u)|(music_pad_lighting_mode()?1024u:0u)
#ifdef MPC_REAR_ROUTING
                   |(rear_ui_pad_owns()?2048u:0u)
#endif
                   ;
        if(m!=last_mode){last_mode=m;resync=6;}
    }
    for(unsigned i=0;i<16;++i) {
        uint64_t light;unsigned bit=1u<<i;
        if (
#ifdef MPC_REAR_ROUTING
            rear_ui_light(i,&light) ||
#endif
#ifdef MPC_STOCK_RESAMPLE
            resample_ui_light(i,&light) ||
#endif
            workflow_light(i,&light)
            ||(atomic_load(&pad_colour_native_workflow)&&pad_colour_light(i,&light))
#ifdef MPC_MIXER_LIVE
            || mixer_light(i,&light)
#endif
#ifdef MPC_CHORDS
            || chords_light(i,&light)
#endif
            || step_native_light(i,&light)
#ifdef MPC_SEQUENCE_COLOURS
            || sequence_light(i,&light)
#endif
            || pad_colour_light(i,&light)
        ) {
            int changed = force_palette || !(owned&bit) || previous[i]!=light;
#ifdef MPC_MIXER_LIVE
            uint32_t packed=FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&light);
            changed |= (atomic_load(&last_sent_mask)&bit) && atomic_load(&last_sent_rgb[i])!=packed;
            changed |= !!(atomic_load(&rgb_collision)&bit) || atomic_load(&rgb_sending[i]);
#endif
            if (changed) {
                previous[i]=light;
#ifdef MPC_MIXER_LIVE
                atomic_fetch_and(&rgb_collision,~bit);
#endif
#ifndef MPC_MIXER_LIVE
                uint32_t packed=FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&light);
#endif
                send_pad_rgb((void*)s,packed,i);
            }
            owned|=bit;
        } else if ((owned&bit)||resync) {
            if(atomic_load(&light_mask)&bit)
                send_pad_rgb((void*)s,(uint32_t)atomic_load(&original_lights[i]),i);
            owned&=~bit;
        }
    }
    if(resync)--resync;
    lights_diagnostic();
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
    static unsigned owned[5],last[5];const unsigned codes[5]={0x50,0x49,0x28,0x4b,0x2b};
    uintptr_t s=atomic_load(&button_sender);unsigned value;
    if (!s) return;
    for(unsigned i=0;i<5;++i) {
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
#ifdef MPC_SONG_UI
    song_ui_init(base);
#endif
#ifdef MPC_COMPRESSOR
    compressor_ui_init(base);
#endif
    note_native_init(base);
#ifdef MPC_KNOB_RESET
    knob_reset_init(base);
#endif
    music_reset(); music_lighting_preferences_init(); music_archive_init(base); music_ui_init(base);
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
        [H_PREVIEW_EVENT_GATE]=(void(*)(void))step_native_preview_suppressed,
        [H_SAMPLE_FILE_COPY]=(void(*)(void))sample_file_copy
    };
    for (unsigned i=0;i<H_COUNT;++i) {
        uint64_t *entry=(uint64_t*)(table+64+i*16);
        if (!callbacks[i] || entry[0]!=phone_hooks[i].entry || entry[1]) { note("BRIDGE_REJECTED"); return; }
    }
    if (mprotect(table,4096,PROT_READ|PROT_WRITE)) { note("BRIDGE_NOT_WRITABLE"); return; }
    if (!music_ui_install(music_sample_paint,music_project_paint,music_tab_paint)) _exit(121);
#ifdef MPC_REAR_ROUTING
    if(!rear_native_install(base)||!rear_ui_install(base))_exit(121);
#endif
    if (!step_native_install(step_paint)) _exit(121);
    if (!workflow_install(workflow_files,workflow_copy)) _exit(121);
#ifdef MPC_LOOP_FEEDBACK
    if (!sample_playing_install(base)) _exit(121);
#endif
    if (!install_name_cursor()) _exit(121);
    if (!project_exit_install(base)) _exit(121);
#ifdef MPC_EARLY_STARTUP
    if (!startup_project_install(base)) _exit(121);
#endif
#ifdef MPC_COMPRESSOR
    if (!compressor_ui_install()) _exit(121);
#endif
#ifdef MPC_SONG_UI
    /* Optional: a mismatched SongScreen leaves stock Song Mode untouched. */
    if (!song_ui_install()) note("SONG_UI NOT_INSTALLED");
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
    filter_spectrum_tint_provider(music_ui_waveform_tint);
    if (!pad_eq_native_install(base)) _exit(121);
    if (!pad_eq_history_install(base)) _exit(121);
#endif
#ifdef MPC_MIXER_LIVE
    if (!mixer_live_native_install(base)) _exit(121);
    if (!mixer_leds_install(base)) _exit(121);
#endif
    for (unsigned i=0;i<H_COUNT;++i) FIELD(table,64+i*16+8,uintptr_t)=(uintptr_t)callbacks[i];
    if (mprotect(table,4096,PROT_READ|PROT_EXEC)) _exit(120);
    note("SAMPLE_LOAD_TRACE READY bounded_native_copy_observer_no_retry");
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
