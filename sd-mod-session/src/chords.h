#ifndef MPC_CHORDS_H
#define MPC_CHORDS_H
#include <stdint.h>
#define CHORD_NOTES 8
enum { CHORD_OCTAVE,CHORD_INVERSION,CHORD_LATCH,CHORD_ARP,CHORD_ARP_RATE,CHORD_ARP_RANGE,CHORD_REPEAT,CHORD_REPEAT_RATE };
/* K3 pattern, Shift+K3 rate, Shift+K1 range (1 or 2 octaves). */
enum { ARP_OFF,ARP_UP,ARP_DOWN,ARP_UP_DOWN,ARP_UP_DOWN_ENDS,ARP_RANDOM,ARP_AS_PLAYED,ARP_CHORD,ARP_PATTERNS };
#define ARP_RATES 6
extern const char *const chords_arp_names[ARP_PATTERNS];
extern const char *const chords_arp_rate_names[ARP_RATES];
extern const unsigned chords_arp_rate_ticks[ARP_RATES]; /* at 960 ticks per quarter */
struct chord_config {
    int enabled,valid,root,tonic,scale,semi,octave,inversion,latch,last_root,arp,arp_rate,arp_range;
    int repeat,repeat_rate; /* Note Repeat held: whole chord repeats at repeat_rate */
    unsigned types,extensions,held_params,held_roots;
};
struct chord_voices {
    unsigned held;
    unsigned char count[8],references[241];
    signed char notes[8][CHORD_NOTES];
};
void chords_open(void);
void chords_close(void);
int chords_active(void);
void chords_context(int root,int tonic,int scale,int semi,int valid);
void chords_get(struct chord_config *out);
void chords_set(int field,int value);
void chords_reset(void);
int chords_parameter(unsigned pad,int down);
void chords_roots(unsigned held,int last);
int chords_degree(int scale,unsigned degree);
int chords_quality(unsigned types);
const char *chords_quality_name(unsigned types);
int chords_make(const struct chord_config*,unsigned root,int out[CHORD_NOTES]);
/* Physical root keys keep their press-time notes. Shared tones have one owner
 * count and one recorded NoteOn; changing quality never retunes a held key. */
int chords_transition(struct chord_voices*,const struct chord_config*,unsigned root,
                      int down,int out[CHORD_NOTES]);
int chords_light(unsigned pad,uint64_t *out);
uint64_t chords_generation(void);
/* Arpeggiator step: notes[] are the held notes in press order (count n).
 * Returns how many pitches to sound this step (1, or n for ARP_CHORD) in
 * out[], and advances *step. *seed drives ARP_RANDOM (xorshift). */
int chords_arp_step(int pattern,int range,const int *notes,int n,unsigned *step,unsigned *seed,int out[CHORD_NOTES*2]);
/* Pattern and rate in effect: Note Repeat overrides the arpeggiator. */
static inline int chords_arp_pattern(const struct chord_config*c){return c->repeat?ARP_CHORD:c->arp;}
static inline int chords_arp_rate(const struct chord_config*c){return c->repeat?c->repeat_rate:c->arp_rate;}
#endif
