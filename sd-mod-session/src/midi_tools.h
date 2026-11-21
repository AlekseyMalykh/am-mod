#ifndef AM_MIDI_TOOLS_H
#define AM_MIDI_TOOLS_H
#include <stddef.h>
#include <stdint.h>
#define AM_MIDI_PADS 128u
#define AM_MIDI_LOG 512u
#define AM_MIDI_PAYLOAD 4608u
enum { AM_MIDI_LAYER_PAD, AM_MIDI_LAYER_LEVELS };
enum { AM_MIDI_OTHER, AM_MIDI_USB, AM_MIDI_EXTERNAL, AM_MIDI_SURFACE };
enum { AM_MIDI_IN, AM_MIDI_OUT };
enum { AM_MIDI_INPUT_OFF, AM_MIDI_INPUT_NOTE, AM_MIDI_INPUT_CC };
enum { AM_MIDI_NATIVE, AM_MIDI_OFF, AM_MIDI_NOTE, AM_MIDI_CC, AM_MIDI_PROGRAM, AM_MIDI_PITCHED };
/* Incoming-note modes. Explicit In Maps always win; modes only see unmapped notes.
 * DRUM: stock pad map. SELECTED: one channel (0 any) plays the pad selected on the MPC,
 * note-root transposes it. CHANNELS: channel N plays its assigned pad, note-root transposes. */
enum { AM_MIDI_MODE_DRUM, AM_MIDI_MODE_SELECTED, AM_MIDI_MODE_CHANNELS };
enum { AM_MIDI_KEY_TUNE, AM_MIDI_KEY_CHOP };
struct am_midi_mode { uint8_t mode,channel,root,pad[16],kind[16]; }; /* pad[ch]=pad+1, 0 none; kind: TUNE=16 Levels semitones, CHOP=16 slices */
int am_midi_mode_get(struct am_midi_mode*);
int am_midi_mode_set(const struct am_midi_mode*);
int am_midi_mode_active(void); /* any channel assignment configured */
void am_midi_selected_pad(int pad); /* UI owner; -1 unknown */
void am_midi_tune_push(unsigned pad,int value,int chop); /* stream thread, before the note is forwarded */
int am_midi_tune_take(unsigned pad,int *value,int *chop); /* music thread, one pending note-on for pad */
/* Port is a matching scope (0 any), not a request to open a second port.
 * Channel 0 matches/preserves the native channel, 1..16 are explicit. */
struct am_midi_map { uint8_t in_kind,in_port,in_channel,in_number;
    uint8_t out_kind,out_port,out_channel,out_number; };
struct am_midi_message { uint8_t size,data[3]; };
struct am_midi_parser { uint8_t status,used,needed,data[2],sysex; uint32_t sysex_bytes; };
typedef void (*am_midi_emit)(void*,const struct am_midi_message*);
void am_midi_parse(struct am_midi_parser*,unsigned,am_midi_emit,void*);
struct am_midi_held { uint8_t active,status,number,kind; };
struct am_midi_router { struct am_midi_held held[2][16][128]; };
/* Stream owner only. 0 drops, 1 emits; unrecognized messages pass unchanged.
 * A Note Off always follows the target captured by its Note On. */
unsigned am_midi_held_notes(void);
/* Controller sources for native buttons (Play/Stop/Rec...): Learn waits for the
 * MIDI message after a hardware button was chosen; the stream thread queues
 * presses that the UI thread replays through the native button path. */
#define AM_MIDI_BUTTON_SLOTS 16
struct am_midi_button { uint8_t port,channel,kind,number,code; };
int am_midi_button_get(unsigned slot,struct am_midi_button*);
void am_midi_button_learn(unsigned code,int on);
unsigned am_midi_button_learning(void);
void am_midi_button_clear(void);
int am_midi_button_take(unsigned*code,unsigned*down);
int am_midi_translate(struct am_midi_router*,unsigned direction,unsigned port,
                      const struct am_midi_message*,struct am_midi_message*);
void am_midi_router_panic(struct am_midi_router*,unsigned channel,am_midi_emit,void*);
int am_midi_map_get(unsigned,struct am_midi_map*);
int am_midi_map_set(unsigned,const struct am_midi_map*);
int am_midi_layer_get(unsigned pad,struct am_midi_map*,unsigned *enabled);
int am_midi_layer_set(unsigned pad,const struct am_midi_map*,unsigned enabled);
int am_midi_layer_resolve(unsigned pad,unsigned layer,struct am_midi_map*);
void am_midi_reset(void);
void am_midi_native_notes(const int notes[128]); /* UI owner publishes copied values */
int am_midi_encode(char*,size_t);
int am_midi_restore(const char*); /* transactional, NULL resets */
struct am_midi_record { uint64_t serial,ms; uint8_t direction,port,size,data[3]; };
void am_midi_observe(unsigned,unsigned,const struct am_midi_message*);
unsigned am_midi_snapshot(struct am_midi_record*,unsigned,uint64_t*);
void am_midi_log_clear(void);
void am_midi_log_pause(int);
int am_midi_log_paused(void);
uint64_t am_midi_log_revision(void);
uint64_t am_midi_log_dropped(void);
void am_midi_diagnostic_tick(void); /* UI owner, bounded RAM console output */
void am_midi_io_ready(void);
void am_midi_io_identity(unsigned,const char*,const char*);
const char *am_midi_port_name(unsigned);
void am_midi_learn(unsigned pad,int enabled);
/* Any thread. If Learn is active, move it to another pad and drop an
 * unconsumed message so a key played after a pad tap lands on the new target. */
void am_midi_learn_retarget(unsigned pad);
int am_midi_learn_active(void);
void am_midi_learn_tick(void); /* UI owner */
#endif
