#include "sample_live_tuner.h"
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace {
static_assert(ATOMIC_INT_LOCK_FREE==2&&ATOMIC_POINTER_LOCK_FREE==2&&ATOMIC_LLONG_LOCK_FREE==2,
              "Sample tuner requires lock-free audio-side atomics");
constexpr unsigned window=1024,slots=4,expiry_ms=350;
struct Request {unsigned token,pad;uintptr_t program;};
std::atomic<unsigned> request_serial{0},request_pad{0};
std::atomic<uintptr_t> request_program{0};
struct Block {unsigned token,pad,rate;uint64_t sample_end,clock_us;float pcm[window];};
Block queue[slots];std::atomic<unsigned> written{0},queue_read{0};
/* These values have exactly one producer: the live engine render owner. */
float accumulating[window];unsigned fill=0,decimated=0,producer_token=0,producer_rate=0;
float decimation_sum=0;
uint64_t produced_samples=0;std::atomic<uint64_t> capture_progress{0};
uint64_t engine_us=0;std::atomic<uint64_t> audio_clock_us{0};
std::atomic<unsigned> result_serial{0},result_token{0},result_hz{0};
std::atomic<uint64_t> result_ms{0};
/* Process-lifetime signal: no static condition-variable destructor may wait
 * on the detached worker during native application shutdown. UI allocates it. */
struct Signal {std::mutex mutex;std::condition_variable wake;};
Signal *wake_signal=nullptr;std::once_flag start;
uint64_t milliseconds()
{return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
bool requested(Request &out)
{
    unsigned first=request_serial.load(std::memory_order_acquire);if(first&1)return false;
    out={first,request_pad.load(std::memory_order_relaxed),request_program.load(std::memory_order_relaxed)};
    std::atomic_thread_fence(std::memory_order_acquire);
    return first==request_serial.load(std::memory_order_acquire)&&out.program&&out.pad<128;
}
void publish(unsigned token,float hz,uint64_t now)
{
    unsigned bits;std::memcpy(&bits,&hz,4);
    result_serial.fetch_add(1,std::memory_order_acq_rel);
    result_token.store(token,std::memory_order_relaxed);result_hz.store(bits,std::memory_order_relaxed);
    result_ms.store(now,std::memory_order_relaxed);result_serial.fetch_add(1,std::memory_order_release);
}
bool pitch(const float *pcm,unsigned rate,float &hz)
{
    float normalised[512]={0};
    const unsigned maximum=std::min(rate/40,511u),minimum=std::max(rate/1000,2u);
    if(maximum<=minimum||maximum>=window-512)return false;
    unsigned count=window-maximum;double energy=0;
    for(unsigned i=0;i<count;++i)energy+=(double)pcm[i]*pcm[i];
    if(energy/count<1e-6)return false;
    double sum=0;
    for(unsigned lag=1;lag<=maximum;++lag){
        double d=0;for(unsigned i=0;i<count;++i){double delta=(double)pcm[i]-pcm[i+lag];d+=delta*delta;}
        sum+=d;normalised[lag]=sum>0?(float)(d*lag/sum):1.f;
    }
    for(unsigned lag=minimum;lag<maximum;++lag)if(normalised[lag]<.15f){
        while(lag+1<maximum&&normalised[lag+1]<normalised[lag])++lag;
        float period=(float)lag;
        if(lag>1){float a=normalised[lag-1],b=normalised[lag],c=normalised[lag+1],den=a-2*b+c;
            if(den!=0)period+=.5f*(a-c)/den;}
        hz=rate/period;return std::isfinite(hz)&&hz>=40.f&&hz<=1100.f;
    }
    return false;
}
bool fresh(const Block &block)
{return audio_clock_us.load(std::memory_order_acquire)-block.clock_us<=(uint64_t)expiry_ms*1000;}
void analyse_pending(uint64_t now,bool refresh_clock=false)
{
    unsigned at=queue_read.load(std::memory_order_relaxed);
    unsigned end=written.load(std::memory_order_acquire);
    /* Analyse only the latest complete window; consume backlog without
     * replaying old pitches after CPU contention. Slots stay owned until copied. */
    if(at!=end){
        Block block=queue[(end-1)%slots];queue_read.store(end,std::memory_order_release);
        Request target;
        if(fresh(block)&&requested(target)&&target.token==block.token){
            float hz=0;if(!pitch(block.pcm,block.rate,hz))hz=0;
            if(refresh_clock)now=milliseconds();
            Request after;if(fresh(block)&&requested(after)&&after.token==block.token)publish(block.token,hz,now);
        }
    }
    if(now-result_ms.load(std::memory_order_relaxed)>expiry_ms){
        Request target;if(requested(target))publish(target.token,0.f,now);
    }
}
int read_snapshot(uint64_t now,float *hz,unsigned *pad)
{
    if(hz)*hz=0;
    Request target;if(!requested(target))return 0;
    if(pad)*pad=target.pad;
    unsigned serial=result_serial.load(std::memory_order_acquire);if(serial&1)return 0;
    unsigned token=result_token.load(std::memory_order_relaxed),bits=result_hz.load(std::memory_order_relaxed);
    uint64_t when=result_ms.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    if(serial!=result_serial.load(std::memory_order_acquire)||token!=target.token||now-when>expiry_ms)return 0;
    float value;std::memcpy(&value,&bits,4);if(value<=0)return 0;
    if(hz)*hz=value;
    return 1;
}
#ifndef SAMPLE_LIVE_TUNER_TEST
void worker(Signal *signal)
{
    setpriority(PRIO_PROCESS,(id_t)syscall(SYS_gettid),19);
    for(;;){
        std::unique_lock<std::mutex> lock(signal->mutex);
        signal->wake.wait(lock,[]{return request_program.load(std::memory_order_acquire)!=0;});
        signal->wake.wait_for(lock,std::chrono::milliseconds(20));lock.unlock();
        analyse_pending(milliseconds(),true);
    }
}
#endif
}
extern "C" void sample_live_tuner_target(uintptr_t program,unsigned pad)
{
    if(pad>=128)program=0;
    if(request_program.load(std::memory_order_relaxed)==program&&(!program||request_pad.load(std::memory_order_relaxed)==pad))return;
#ifndef SAMPLE_LIVE_TUNER_TEST
    if(program){try{std::call_once(start,[]{
        Signal *signal=new Signal;
        try{std::thread(worker,signal).detach();}catch(...){delete signal;throw;}
        wake_signal=signal;
    });}catch(...){return;}}
#endif
    request_serial.fetch_add(1,std::memory_order_acq_rel);
    request_program.store(program,std::memory_order_relaxed);request_pad.store(pad,std::memory_order_relaxed);
    request_serial.fetch_add(1,std::memory_order_release);
    if(wake_signal)wake_signal->wake.notify_one();
}
extern "C" void sample_live_tuner_clock(unsigned frames,unsigned rate)
{
    if(!request_program.load(std::memory_order_acquire)||!frames||frames>8192||rate<8000||rate>192000)return;
    engine_us+=(uint64_t)frames*1000000/rate;audio_clock_us.store(engine_us,std::memory_order_release);
}
extern "C" void sample_live_tuner_push(uintptr_t program,unsigned pad,const float *left,
                                       const float *right,unsigned frames,unsigned rate)
{
    Request target;
    if(!requested(target)||target.program!=program||target.pad!=pad||!frames||frames>8192||rate<8000||rate>192000)return;
    if(producer_token!=target.token||producer_rate!=rate){
        producer_token=target.token;producer_rate=rate;fill=decimated=0;decimation_sum=0;
    }
    const unsigned step=(rate+11999)/12000;
    for(unsigned i=0;i<frames;++i){
        float l=left?left[i]:0,r=right?right[i]:l;
        if(!std::isfinite(l))l=0;
        if(!std::isfinite(r))r=0;
        decimation_sum+=(l+r)*.5f;
        if(++decimated!=step)continue;
        accumulating[fill++]=decimation_sum/step;decimation_sum=0;decimated=0;++produced_samples;
        if(fill==window){
            unsigned w=written.load(std::memory_order_relaxed),rindex=queue_read.load(std::memory_order_acquire);
            if(w-rindex<slots){Block &out=queue[w%slots];out.token=target.token;out.pad=pad;out.rate=rate/step;out.sample_end=produced_samples;out.clock_us=engine_us;
                std::memcpy(out.pcm,accumulating,sizeof accumulating);written.store(w+1,std::memory_order_release);}
            fill=0;
        }
    }
    capture_progress.store(produced_samples,std::memory_order_release);
}
extern "C" int sample_live_tuner_info(float *hz,unsigned *pad)
{return read_snapshot(milliseconds(),hz,pad);}
