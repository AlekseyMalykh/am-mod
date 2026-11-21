#include "seq_mutes.h"
#include "music_settings.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#define FIELD(p,off,type) (*(type *)((char *)(p)+(off)))
static _Atomic int current=-1,variant;
/* Audio-path snapshot: one bit per MIDI note (the note -> pad map of the current program applied). */
static _Atomic uint64_t gate_serial,gate_notes[2];
static _Atomic unsigned gate_hits;
static unsigned reports,last_hits;
void seq_mutes_refresh(void *program,void *sequence)
{
    if(!program||!sequence)return; /* transient: keep the last snapshot instead of un-muting */
    int index=FIELD(sequence,0x480,int);
    if(index<0||index>=128)index=-1;
    atomic_store(&current,index);
    uint64_t pads[2]={0,0},notes[2]={0,0};
    if(index>=0)music_sequence_mutes_get((unsigned)index,pads);
    if(pads[0]|pads[1])
        for(unsigned note=0;note<128;++note){
            int pad=FIELD(program,0x14a8+note*4,int);
            if(pad>=0&&pad<128&&((pads[pad/64]>>(pad%64))&1u))notes[note/64]|=(uint64_t)1<<(note%64);
        }
    if(notes[0]!=atomic_load(&gate_notes[0])||notes[1]!=atomic_load(&gate_notes[1])){
        atomic_fetch_add_explicit(&gate_serial,1,memory_order_acq_rel);
        atomic_store_explicit(&gate_notes[0],notes[0],memory_order_relaxed);
        atomic_store_explicit(&gate_notes[1],notes[1],memory_order_relaxed);
        atomic_fetch_add_explicit(&gate_serial,1,memory_order_release);
        if(reports<60){++reports;fprintf(stderr,"SEQ_MUTES SNAPSHOT seq=%d pads=%016llx%016llx notes=%016llx%016llx\n",index,
            (unsigned long long)pads[1],(unsigned long long)pads[0],(unsigned long long)notes[1],(unsigned long long)notes[0]);}
    }
    unsigned hits=atomic_load(&gate_hits);
    if(hits!=last_hits&&reports<60){++reports;last_hits=hits;fprintf(stderr,"SEQ_MUTES GATE_HITS=%u seq=%d\n",hits,index);}
}
int seq_mutes_event_muted(const void *event)
{
    /* Called from the sequencer route on the audio path: atomics only. */
    unsigned note=((const unsigned char *)event)[16+0x20];
    if(note>=128)return 0;
    for(unsigned attempt=0;attempt<2;++attempt){
        uint64_t serial=atomic_load_explicit(&gate_serial,memory_order_acquire);
        if(serial&1)continue;
        uint64_t word=atomic_load_explicit(&gate_notes[note/64],memory_order_relaxed);
        if(serial!=atomic_load_explicit(&gate_serial,memory_order_acquire))continue;
        int hit=(int)((word>>(note%64))&1u);
        if(hit)atomic_fetch_add_explicit(&gate_hits,1,memory_order_relaxed);
        return hit;
    }
    return 0;
}
int seq_mutes_variant(void){return atomic_load(&variant);}
void seq_mutes_set_variant(int on){atomic_store(&variant,!!on);}
int seq_mutes_current(void){return atomic_load(&current);}
int seq_mutes_toggle(unsigned bank,unsigned pad)
{
    int s=atomic_load(&current);
    if(s<0||bank>=8||pad>=16)return -1;
    return music_sequence_pad_toggle((unsigned)s,bank*16+pad);
}
int seq_mutes_pad_state(unsigned bank,unsigned pad)
{
    int s=atomic_load(&current);
    if(s<0||bank>=8||pad>=16)return 0;
    return music_sequence_pad_muted((unsigned)s,bank*16+pad);
}
int seq_mutes_clear(void)
{
    int s=atomic_load(&current);
    return s<0?0:music_sequence_mutes_clear((unsigned)s);
}
