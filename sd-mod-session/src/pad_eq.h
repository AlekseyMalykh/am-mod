#ifndef MPC_PAD_EQ_H
#define MPC_PAD_EQ_H
#include <stdint.h>
#include "fx_eq.h"
#include "resample_view.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { PAD_EQ_FREQUENCY,PAD_EQ_GAIN,PAD_EQ_Q,PAD_EQ_TYPE,PAD_EQ_BYPASS };
enum { PAD_EQ_BELL,PAD_EQ_LOW_PASS,PAD_EQ_HIGH_PASS,PAD_EQ_NOTCH,PAD_EQ_LOW_SHELF,PAD_EQ_HIGH_SHELF,PAD_EQ_TYPES };
uint64_t pad_eq_default(void);
int pad_eq_valid(uint64_t);
uint64_t pad_eq_edit(uint64_t,int field,unsigned value,int relative);
void pad_eq_config(uint64_t,double,struct fx_eq_config*);
void pad_eq_paint(uint64_t,unsigned pad,int shift,int focus,const struct resample_painter*);
/* Numeric snapshot only: one atomic load per instrument block. */
void pad_eq_publish(unsigned pad,uint64_t);
uint64_t pad_eq_published(unsigned pad);
void pad_eq_publish_all(const uint64_t values[128]);
/* Per-render-thread state, caller supplies a valid stereo/mono planar buffer.
 * Returns0 only if the bounded lane table is exhausted or inputs invalid.
 * No allocation, locks, file/UI/native model calls on this path. */
int pad_eq_audio(unsigned pad,uintptr_t instrument,double rate,float**channels,unsigned count,unsigned frames,int silent);
/* Spectrum of the pad shown on the EQ page (audio thread capture). */
int pad_eq_spectrum_wanted(unsigned pad);
void pad_eq_spectrum_capture(unsigned pad,float**channels,unsigned count,unsigned frames,double rate);
int pad_eq_spectrum_dirty(void);
#ifdef __cplusplus
}
#endif
#endif
