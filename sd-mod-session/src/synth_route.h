#ifndef AM_SYNTH_ROUTE_H
#define AM_SYNTH_ROUTE_H
#include "synth_binding.h"
#ifdef __cplusplus
extern "C" {
#endif
struct am_synth_route_snapshot {char program_key[128];unsigned carrier;uint64_t generation,serial,stop;};
/* UI-only publisher. Values only; no native pointer and no audio mutex. */
int am_synth_route_publish(const struct am_synth_binding*,uint64_t generation);
void am_synth_route_clear(void);
void am_synth_route_stop(void);
uint64_t am_synth_route_stop_serial(void);
/* Bounded coherent snapshot for the native music executor. */
int am_synth_route_read(uint64_t generation,struct am_synth_route_snapshot*);
#ifdef __cplusplus
}
#endif
#endif
