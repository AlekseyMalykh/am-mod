#include "src/sample_live_tuner.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>
static std::atomic<unsigned> pitch_hz{220};
static std::atomic<bool> running{true};
static void audio()
{
    unsigned long long frame=0;float pcm[128];
    while(running.load()){
        unsigned hz=pitch_hz.load();
        for(unsigned i=0;i<128;++i)pcm[i]=.4f*std::sin((frame+i)*6.283185307179586*hz/48000);
        sample_live_tuner_clock(128,48000);sample_live_tuner_push(0x12345000,14,pcm,pcm,128,48000);frame+=128;
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
}
static bool await_pitch(float expected)
{
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(4);
    while(std::chrono::steady_clock::now()<deadline){
        float hz=0;unsigned pad=128;
        if(sample_live_tuner_info(&hz,&pad)&&pad==14&&std::fabs(hz-expected)<2)return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return false;
}
int main()
{
    sample_live_tuner_target(0x12345000,14);std::thread producer(audio);
    assert(await_pitch(220));pitch_hz.store(440);assert(await_pitch(440));
    sample_live_tuner_target(0x12345000,15);float hz;unsigned pad;
    assert(!sample_live_tuner_info(&hz,&pad));
    sample_live_tuner_target(0,128);running.store(false);producer.join();
    assert(!sample_live_tuner_info(&hz,&pad));
    std::puts("PASS threaded Sample tuner: production worker follows one Sound 220 -> 440 Hz, isolates selection and exits without worker shutdown deadlock");
    return 0;
}
