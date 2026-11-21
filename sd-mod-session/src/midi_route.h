#ifndef AM_MIDI_ROUTE_H
#define AM_MIDI_ROUTE_H
#include "midi_event.h"
#include "midi_tools.h"
/* A value-only routing decision. The native adapter owns occurrence IDs and
 * must retain this captured release across map/project/selection changes. */
struct am_midi_target {unsigned port;struct am_midi_message on,off;};
int am_midi_route_event(unsigned sound,unsigned layer,unsigned port,unsigned channel,
                        const struct am_midi_event*,struct am_midi_target*);
#endif
