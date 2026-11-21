#define SAMPLE_LIVE_TUNER_TEST
#include "src/sample_live_tuner.cc"
#include <cassert>
#include <cstdio>
static unsigned long long generated;
static void tone(uintptr_t program,unsigned pad,unsigned rate,float hz,unsigned count)
{
    float pcm[128];
    for(unsigned at=0;at<count;at+=128){
        unsigned n=std::min(128u,count-at);
        for(unsigned i=0;i<n;++i)pcm[i]=.4f*std::sin((generated+i)*6.283185307179586*hz/rate);
        sample_live_tuner_clock(n,rate);sample_live_tuner_push(program,pad,pcm,pcm,n,rate);generated+=n;
    }
}
int main()
{
    const uintptr_t program=0x12345000;float hz;unsigned pad;
    assert(!read_snapshot(1000,&hz,&pad));
    sample_live_tuner_target(program,14);
    tone(program,14,48000,220,4096);analyse_pending(1000);
    assert(read_snapshot(1000,&hz,&pad)&&pad==14&&std::fabs(hz-220)<2);
    /* The original regression: the same Sound changes pitch while it plays. */
    tone(program,14,48000,440,4096);analyse_pending(1100);
    assert(read_snapshot(1100,&hz,&pad)&&std::fabs(hz-440)<2);
    assert(!read_snapshot(1500,&hz,&pad));analyse_pending(1500);
    assert(!read_snapshot(1500,&hz,&pad));
    tone(program,13,48000,330,4096);analyse_pending(1600);
    assert(!read_snapshot(1600,&hz,&pad));
    tone(program+16,14,48000,330,4096);analyse_pending(1700);
    assert(!read_snapshot(1700,&hz,&pad));
    sample_live_tuner_target(program,15);assert(!read_snapshot(1700,&hz,&pad));
    tone(program,15,192000,110,16384);analyse_pending(1800);
    assert(read_snapshot(1800,&hz,&pad)&&pad==15&&std::fabs(hz-110)<2);
    for(unsigned i=0;i<128;++i){sample_live_tuner_clock(128,192000);sample_live_tuner_push(program,15,nullptr,nullptr,128,192000);}
    analyse_pending(1900);assert(!read_snapshot(1900,&hz,&pad));
    /* A full queue never blocks the audio producer; the worker catches up
     * directly to its latest complete window. Changing target rejects backlog. */
    tone(program,15,48000,330,4096*8);assert(written.load()-queue_read.load()==slots);
    sample_live_tuner_target(program,14);analyse_pending(2000);
    assert(!read_snapshot(2000,&hz,&pad));
    tone(program,14,44100,261.6256f,4096);analyse_pending(2100);
    assert(read_snapshot(2100,&hz,&pad)&&std::fabs(hz-261.6256f)<2);
    /* Playback clock advances even without a Pad sum: late analysis must not
     * resurrect an old note after the Sound has stopped. */
    tone(program,14,48000,330,4096);
    for(unsigned i=0;i<200;++i)sample_live_tuner_clock(128,48000);
    analyse_pending(2600);assert(!read_snapshot(2600,&hz,&pad));
    sample_live_tuner_target(0,128);assert(!read_snapshot(2200,&hz,&pad));
    std::puts("PASS live Sample tuner: same-Sound pitch changes, 44.1/48/192k capture, silence/expiry, source isolation, target changes and bounded backlog");
}
