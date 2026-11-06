#include "mixer_session.h"
#include "mixer_archive.h"
#include <atomic>
#include <mutex>
#include <new>
#include <ctime>
#include <cstdio>
struct mixer_session {
    std::mutex mutex;
    mixer_live*live=nullptr;
    mixer_control*control=nullptr;
    mixer_project confirmed{},command{},inflight{};
    uint64_t revision=0,expected=0,target=0,epoch=0,part=0,rejected=0;
    const char *error=nullptr;
    int(*healthy)(void*)=nullptr;void *health_user=nullptr;
    bool queued=false,published=false;
    std::atomic<int> status{0};
};
static int submit(void*user,const mixer_project*model,uint64_t expected,uint64_t*revision)
{
    auto*s=static_cast<mixer_session*>(user);std::lock_guard<std::mutex>lock(s->mutex);
    if(s->status.load(std::memory_order_acquire)<0||s->queued||s->published||expected!=s->revision||expected==UINT64_MAX)return 0;
    s->command=*model;s->expected=expected;s->queued=true;*revision=expected+1;return 1;
}
extern "C" mixer_session*mixer_session_create(uint64_t epoch,uint64_t part,double rate,unsigned capacity,
    void*engine,void*program,const mixer_live_ops*ops,const fx_backend*backend)
{
    auto*s=new(std::nothrow)mixer_session;if(!s)return nullptr;
    unsigned mode=0;
    if(!epoch||!mixer_archive_snapshot(&s->confirmed,&s->revision,&mode)||mode==MIXER_ARCHIVE_OPAQUE){delete s;return nullptr;}
    s->epoch=epoch;s->part=part;enum mixer_bus_result error;
    s->live=mixer_live_prepare(&s->confirmed,part,rate,capacity,epoch,engine,program,ops,backend,&error);
    mixer_control_ops control_ops{s,submit};s->control=mixer_control_create(&control_ops,rate);
    if(!s->live||!s->control){mixer_session_destroy(s);return nullptr;}
    mixer_control_confirm(s->control,&s->confirmed,s->revision,epoch);return s;
}
extern "C" mixer_control*mixer_session_control(mixer_session*s){return s?s->control:nullptr;}
extern "C" void mixer_session_health(mixer_session*s,int(*healthy)(void*),void*user)
{if(s){s->healthy=healthy;s->health_user=user;}}
extern "C" mixer_live*mixer_session_live(mixer_session*s){return s?s->live:nullptr;}
extern "C" int mixer_session_status(const mixer_session*s){return s?s->status.load(std::memory_order_acquire):-2;}
extern "C" void mixer_session_owner_tick(mixer_session*s)
{
    if(!s||s->status.load(std::memory_order_acquire)<0)return;
    mixer_live_collect(s->live);
    mixer_project current;uint64_t revision;unsigned mode;
    if(!mixer_archive_snapshot(&current,&revision,&mode))return;
    std::lock_guard<std::mutex>lock(s->mutex);
    if(revision!=s->revision||mode==MIXER_ARCHIVE_OPAQUE){s->status.store(-1,std::memory_order_release);return;}
    if(s->published&&mixer_live_applied_revision(s->live)>=s->target){
        if(s->healthy&&!s->healthy(s->health_user)){s->status.store(-2,std::memory_order_release);return;}
        if(mixer_archive_replace(&s->inflight,s->expected)!=MIXER_EDIT_OK){s->status.store(-1,std::memory_order_release);return;}
        s->confirmed=s->inflight;s->revision=s->target;s->published=false;
    }
    if(!s->queued||s->published)return;
    if(s->expected!=s->revision){s->status.store(-1,std::memory_order_release);return;}
    enum mixer_bus_result result=mixer_live_publish(s->live,&s->command,s->part,s->expected+1);
    if(result==MIXER_BUS_STATE)return;
    if(result!=MIXER_BUS_OK){
        s->rejected=s->expected+1;s->queued=false;
        s->error=result==MIXER_BUS_FX_ERROR?"FX unavailable":result==MIXER_BUS_RESOURCE?"FX resources unavailable":"Update rejected";
        return;
    }
    s->inflight=s->command;s->target=s->expected+1;s->published=true;s->queued=false;
}
extern "C" void mixer_session_ui_tick(mixer_session*s)
{
    if(!s)return;
    if(s->status.load(std::memory_order_acquire)<0){mixer_control_close(s->control);return;}
    mixer_meter_snapshot meter;
    if(mixer_live_take_meter(s->live,&meter)){
        timespec time{};clock_gettime(CLOCK_MONOTONIC,&time);
        uint64_t now=static_cast<uint64_t>(time.tv_sec)*1000+time.tv_nsec/1000000;
        mixer_control_meter(s->control,&meter,now);
        bool ready=meter.valid_mask==0x3ff;
        {   /* Lab diagnostic: which of the 10 bus meters (A-H, Direct, Master) are valid. */
            static uint32_t last=~0u;static unsigned lines;
            if(meter.valid_mask!=last&&lines<60){++lines;last=meter.valid_mask;
                std::fprintf(stderr,"MIXER_METERS valid=0x%03x fault=0x%03x ready=%d\n",meter.valid_mask,meter.fault_mask,ready?1:0);}
        }
        int expected=ready?0:1;
        s->status.compare_exchange_strong(expected,ready?1:0,std::memory_order_acq_rel);
        if(!ready)mixer_control_close(s->control);
    }
    mixer_project model;uint64_t revision,rejected;const char *error;
    {std::lock_guard<std::mutex>lock(s->mutex);model=s->confirmed;revision=s->revision;rejected=s->rejected;error=s->error;s->rejected=0;}
    if(rejected)mixer_control_reject(s->control,rejected,error);
    mixer_control_confirm(s->control,&model,revision,s->epoch);mixer_control_tick(s->control);
}
extern "C" void mixer_session_destroy(mixer_session*s)
{
    if(s){mixer_control_destroy(s->control);mixer_live_destroy(s->live);delete s;}
}
