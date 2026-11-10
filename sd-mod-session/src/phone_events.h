#ifndef MPC_PHONE_EVENTS_H
#define MPC_PHONE_EVENTS_H
#include <stdint.h>
enum { PHONE_CONTROLLER = 1, PHONE_BUTTON, PHONE_PAD, PHONE_STEP_PAD, PHONE_STEP_SHORTCUT_PAD, PHONE_WORKFLOW_PAD, PHONE_RESAMPLE_PAD, PHONE_KNOB_RESET, PHONE_BANK_PAD, PHONE_TAIL_PAD, PHONE_REAR_PAD,
       PHONE_MIXER_PAD, PHONE_MIXER_ENTRY, PHONE_MIXER_BUTTON, PHONE_MIXER_CC, PHONE_PALETTE_PAD };
#define PHONE_EVENT_CAPACITY 128
struct phone_event {
    uint64_t generation, when_ms, step_ticket, workflow_ticket;
    unsigned kind, pad, down, up; /* Controller down stores its arrival-time Shift. */
    _Alignas(8) unsigned char native[16];
    _Alignas(8) unsigned char midi[24]; /* Only native inline messages (size1..8). */
};
/* Fixed RAM FIFO; trailing absolute K1-K3 movements retain their initial
 * positions, turning points and latest positions in arrival order, without
 * crossing an action/context boundary (native Scaled/Pickup need this history).
 * Producer never touches editor pointers; consumer is UI Timer.
 * No allocation, observer thread or message payload with a component pointer. */
int phone_events_push(const struct phone_event *event);
/* Successful down reserves a slot for its one matching up. The producer
 * must suppress repeated downs and submit that up even after a mode change. */
int phone_events_push_pair(const struct phone_event *event, int down);
int phone_events_pop(struct phone_event *event);
unsigned phone_events_pending(void);
#endif
