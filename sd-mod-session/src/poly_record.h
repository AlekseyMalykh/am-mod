#ifndef MPC_POLY_RECORD_H
#define MPC_POLY_RECORD_H
#include <stdint.h>
int poly_record_install(uintptr_t image);
void poly_record_context(void *current_things);
/* Chords arpeggiator (MPC_CHORDS). Held chord notes in press order, filled
 * on the pad input thread; the UI timer reads them and plays one step at a
 * time through the same native Engine/recorder route as a chord. */
int poly_record_arp_notes(int *notes,int max,float *velocity);
int poly_record_arp_emit(int pitch,int on,float velocity);
#endif
