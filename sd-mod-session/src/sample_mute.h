#ifndef AM_SAMPLE_MUTE_H
#define AM_SAMPLE_MUTE_H

/* Pinned MutePadMode (15309e0) reads bit 0 of the instrument's mute flags. */
static inline int sample_instrument_muted(const void *instrument)
{
    return instrument && (__atomic_load_n((const unsigned char *)instrument + 0x78, __ATOMIC_ACQUIRE) & 1u);
}

#define SAMPLE_MUTED_TINT 0x999999u

#endif
