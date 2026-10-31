#include "src/resample_fast_plan.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(void){
    struct resample_fast_plan p;
    /* A 45s sample must cover23 bars at120BPM, regardless of the user's
     * current 2-bar/4s sequence. That sequence is deliberately not an input. */
    assert(resample_fast_plan_make(45*44100,44100,45*44100,44100,48000,120000,960,4,UINT64_MAX,&p));
    assert(p.minimum_frames==45*48000&&p.minimum_ticks==86400&&p.render_bars==23&&p.render_ticks==88320&&p.render_frames==46*48000);
    assert(resample_fast_plan_make(45*44100,44100,90*48000,48000,48000,120000,960,4,UINT64_MAX,&p));assert(p.minimum_frames==90*48000&&p.render_bars==45);
    /* Trim cannot silently weaken the user-requested full source floor. */
    assert(resample_fast_plan_make(45*44100,44100,44100,44100,44100,120000,960,4,UINT64_MAX,&p));assert(p.minimum_frames==45*44100);
    assert(resample_fast_plan_make(1,44100,1,44100,48000,120000,960,4,UINT64_MAX,&p)&&p.minimum_frames==2&&p.render_bars==1);
    assert(!resample_fast_plan_make(45*44100,44100,0,44100,48000,120000,960,4,22*3840,&p)&&!p.minimum_frames);
    assert(!resample_fast_plan_make(UINT64_MAX,8000,0,44100,192000,120000,960,4,UINT64_MAX,&p));
    for(unsigned tempo=20000;tempo<=300000;tempo+=17000)for(unsigned rate=44100;rate<=48000;rate+=3900){
        assert(resample_fast_plan_make(123456789,44100,123456790,48000,rate,tempo,960,7,UINT64_MAX,&p));
        assert(p.render_frames>=p.minimum_frames&&(__uint128_t)p.minimum_frames*44100>=(__uint128_t)123456789*rate);
    }
    float l[128]={0},r[128]={0};struct resample_fast_tail t;
    assert(resample_fast_tail_init(&t,100,10,128,.001f));
    assert(resample_fast_tail_scan(&t,l,r,99)==RFT_MORE&&t.scanned==99);
    assert(resample_fast_tail_scan(&t,l,r,10)==RFT_MORE&&t.scanned==109);
    r[0]=.5f;assert(resample_fast_tail_scan(&t,l,r,1)==RFT_MORE&&t.quiet==0);r[0]=0;
    assert(resample_fast_tail_scan(&t,l,r,10)==RFT_QUIET&&t.scanned==120);
    assert(resample_fast_tail_init(&t,10,5,128,.001f));
    for(unsigned i=0;i<128;++i)r[i]=i%4==0?.1f:0;
    assert(resample_fast_tail_scan(&t,l,r,128)==RFT_NEEDS_MORE_ROOM);
    assert(resample_fast_tail_init(&t,10,5,128,.001f));r[7]=NAN;assert(resample_fast_tail_scan(&t,l,r,128)==RFT_INVALID);
    assert(!resample_fast_tail_init(&t,100,29,128,.001f));
    puts("PASS fast-sample planning: long source independent of sequence, slower processing grows duration, raw source floor retained, round-up/resampling, overflow/native limits reject; tail waits past floor, both channels, stable silence, nondecaying budget is not success. Engine integration is not implemented.");return 0;
}
