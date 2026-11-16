#include "src/resample_stock_backend.cc"
#include <cassert>
static std::thread::id owner;
static unsigned reads;
static int read_tone(void*,uint64_t first,size_t n,float*out){
    assert(std::this_thread::get_id()==owner);++reads;
    for(size_t i=0;i<n;++i)out[i*2]=out[i*2+1]=.5f*std::sin((first+i)*6.283185307179586*220/48000);
    return 1;
}
int main(){
    owner=std::this_thread::get_id();resample_stock_backend b;
    pad_sample_selection sel{};sel.first=0;sel.end=16000;sel.stream.rate=48000;sel.stream.read=read_tone;
    resample_stock_backend::TuneSnapshot job;job.epoch=1;job.serial=1;job.pad=4;job.rate=48000;
    tuner_capture(sel,job);assert(job.windows==3&&reads>0);
    const unsigned captured_reads=reads;
    {std::lock_guard<std::mutex>lock(b.mutex);b.tune_serial=1;b.tune_pending=std::move(job);b.tune_pending_ready=true;b.tuner.state=1;}
    std::thread worker(tuner_worker,&b);b.tune_wake.notify_one();
    bool found=false;
    for(unsigned i=0;i<400;++i){
        {std::lock_guard<std::mutex>lock(b.mutex);if(b.tuner.state==2){assert(std::fabs(b.tuner.hz-220)<2&&b.tuner.pad==4);found=true;break;}}
        usleep(5000);
    }
    assert(found&&reads==captured_reads);
    /* An old project result must never replace the currently displayed note. */
    job={};job.epoch=0;job.serial=2;job.pad=7;job.rate=48000;
    {std::lock_guard<std::mutex>lock(b.mutex);b.tune_serial=2;b.tune_pending=std::move(job);b.tune_pending_ready=true;}
    b.tune_wake.notify_one();usleep(30000);
    {std::lock_guard<std::mutex>lock(b.mutex);assert(b.tuner.state==2&&b.tuner.pad==4);}
    b.live.store(false);b.tune_wake.notify_one();worker.join();
    std::puts("PASS sample tuner: copied 220 Hz PCM analysed by worker, model-only native reads, stale project result rejected, clean worker stop");
}
