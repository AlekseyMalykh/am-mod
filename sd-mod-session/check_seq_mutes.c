/* Sequence Pad Mutes: model, own project field, copy, and the sequencer-gate snapshot. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/music_settings.h"
#include "src/seq_mutes.h"
#define FIELD(p,off,type) (*(type *)((char *)(p)+(off)))
int main(void){
    music_reset();
    /* model */
    assert(!music_sequence_pad_muted(3,17));
    assert(music_sequence_pad_toggle(3,17)==1&&music_sequence_pad_muted(3,17));
    assert(music_sequence_pad_toggle(3,100)==1);
    assert(!music_sequence_pad_muted(4,17)&&!music_sequence_pad_muted(3,16));
    assert(music_sequence_pad_toggle(128,0)==-1&&music_sequence_pad_toggle(0,128)==-1);
    uint64_t m[2];assert(music_sequence_mutes_get(3,m)&&m[0]==(1ull<<17)&&m[1]==(1ull<<(100-64)));
    assert(music_sequence_mutes_copy(3,9)&&music_sequence_pad_muted(9,17)&&music_sequence_pad_muted(9,100));
    assert(music_sequence_pad_toggle(3,17)==0&&music_sequence_pad_muted(9,17)); /* copy is independent */
    /* own project field: round trip */
    struct music_project p,q;music_snapshot(&p);
    char *text=malloc(SEQ_MUTES_PAYLOAD_BYTES);assert(text);
    int n=music_seqmutes_encode(&p,text,SEQ_MUTES_PAYLOAD_BYTES);assert(n>0);
    music_defaults(&q);assert(music_seqmutes_decode(text,(size_t)n,&q));
    assert(!memcmp(q.sequence_mutes,p.sequence_mutes,sizeof(p.sequence_mutes)));
    /* old project / nothing muted: no field is written */
    music_defaults(&q);assert(music_seqmutes_encode(&q,text,SEQ_MUTES_PAYLOAD_BYTES)==0);
    /* all 128 sequences fully muted fits the payload */
    for(unsigned i=0;i<128;++i)q.sequence_mutes[i][0]=q.sequence_mutes[i][1]=~0ull;
    n=music_seqmutes_encode(&q,text,SEQ_MUTES_PAYLOAD_BYTES);assert(n>0&&n<SEQ_MUTES_PAYLOAD_BYTES);
    /* damaged payloads are rejected and leave the target untouched */
    struct music_project t;music_defaults(&t);t.sequence_mutes[5][0]=7;
    const char *bad[]={"","AMSM1\n","AMSM2\n3 00000000000000010000000000000000\n","AMSM1\n3 0000000000000001000000000000000\n",
        "AMSM1\n3 0000000000000000000000000000000x\n","AMSM1\n3 00000000000000000000000000000000\n","AMSM1\n128 00000000000000010000000000000000\n",
        "AMSM1\n4 00000000000000010000000000000000\n3 00000000000000010000000000000000\n","AMSM1\n3 00000000000000010000000000000000"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);++i){assert(!music_seqmutes_decode(bad[i],strlen(bad[i]),&t));assert(t.sequence_mutes[5][0]==7);}
    /* project replace / new project */
    music_replace(&p);assert(music_sequence_pad_muted(9,100));
    music_reset();assert(!music_sequence_pad_muted(9,100));
    /* sequencer gate snapshot: note -> pad map of the program, current sequence from the sequence object */
    music_replace(&p);
    static unsigned char program[0x14a8+128*4+64],sequence[0x500];
    for(unsigned note=0;note<128;++note)FIELD(program,0x14a8+note*4,int)=(int)note; /* identity map */
    FIELD(program,0x14a8+60*4,int)=17;  /* note 60 plays pad 17 */
    FIELD(program,0x14a8+61*4,int)=-1;  /* unmapped */
    unsigned char event[56];memset(event,0,sizeof(event));
    FIELD(sequence,0x480,int)=9;
    seq_mutes_refresh(program,sequence);
    assert(seq_mutes_current()==9);
    event[16+0x20]=60;assert(seq_mutes_event_muted(event));    /* pad 17 muted in sequence 9 */
    event[16+0x20]=100;assert(seq_mutes_event_muted(event));   /* pad 100 muted */
    event[16+0x20]=17;assert(seq_mutes_event_muted(event));    /* note 17 -> pad 17 */
    event[16+0x20]=61;assert(!seq_mutes_event_muted(event));
    event[16+0x20]=5;assert(!seq_mutes_event_muted(event));
    FIELD(sequence,0x480,int)=2;seq_mutes_refresh(program,sequence);   /* another sequence: no mutes */
    event[16+0x20]=100;assert(!seq_mutes_event_muted(event));
    FIELD(sequence,0x480,int)=9;seq_mutes_refresh(program,sequence);
    event[16+0x20]=100;assert(seq_mutes_event_muted(event));
    seq_mutes_refresh(NULL,sequence);seq_mutes_refresh(program,NULL); /* transient: snapshot is kept */
    assert(seq_mutes_event_muted(event));
    /* UI helpers act on the current sequence: bank*16+pad */
    assert(seq_mutes_pad_state(6,4)==1);                              /* pad 100 = bank 6, pad 4 */
    assert(seq_mutes_toggle(6,4)==0&&!seq_mutes_pad_state(6,4));
    seq_mutes_refresh(program,sequence);event[16+0x20]=100;assert(!seq_mutes_event_muted(event));
    assert(seq_mutes_clear());seq_mutes_refresh(program,sequence);
    event[16+0x20]=60;assert(!seq_mutes_event_muted(event));
    assert(!seq_mutes_variant());seq_mutes_set_variant(1);assert(seq_mutes_variant());seq_mutes_set_variant(0);
    free(text);
    puts("PASS check_seq_mutes");return 0;
}
