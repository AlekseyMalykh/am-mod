#ifndef AM_SEQ_MUTES_H
#define AM_SEQ_MUTES_H
#include <stdint.h>
/* Sequence Pad Mutes: static pad mutes stored per sequence (project data, see music_settings).
 * They only block sequencer playback; events are never changed, manual pad play is never blocked.
 * Independent of the native (Global) pad mutes and of track mutes. */
void seq_mutes_refresh(void *program,void *sequence); /* UI tick: current sequence + note->pad map -> gate snapshot */
int seq_mutes_event_muted(const void *event);          /* sequencer gate (audio path): lock-free, no I/O */
int seq_mutes_variant(void);                           /* the Mute session works on sequence mutes */
void seq_mutes_set_variant(int on);
int seq_mutes_current(void);                           /* current sequence 0..127 or -1 */
int seq_mutes_toggle(unsigned bank,unsigned pad);      /* new state 0/1, -1 on error */
int seq_mutes_pad_state(unsigned bank,unsigned pad);
int seq_mutes_clear(void);                             /* Unmute All of the current sequence, all banks */
#endif
