#include "resample_queue.h"
#include <mutex>
#include <new>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
struct resample_queue {
    uintptr_t base;void *context,*user;int(*tick)(void*);
    std::mutex mutex;uint64_t generation=0;bool pending=false,failed=false,stopping=false;
};
struct QueueAction {uintptr_t invoke,manage;void *reference,*job;uintptr_t reserved;};
static_assert(sizeof(QueueAction)==40,"Pinned native Copy closure");
static bool post(resample_queue*);
static void run(void*payload)noexcept{
    void*reference=F(payload,0,void*);auto*q=F(payload,8,resample_queue*);
    uint64_t generation;bool stopping;
    {std::lock_guard<std::mutex> lock(q->mutex);generation=q->generation;stopping=q->stopping;}
    int more=0;try{if(!stopping)more=q->tick(q->user);}catch(...){more=-1;}
    {
        std::lock_guard<std::mutex> lock(q->mutex);
        bool again=false;
        if(!q->stopping&&more>=0&&(more||generation!=q->generation)){
            again=post(q);q->failed=!again;
        }else q->failed=more<0;
        __atomic_fetch_sub(reinterpret_cast<unsigned*>(static_cast<char*>(reference)+8),1,__ATOMIC_RELEASE);
        q->pending=again;
    }
    /* Nothing below dereferences q: destroy may succeed after pending=false. */
}
static bool post(resample_queue*q){
    if(q->stopping)return false; /* mutex held; do not touch stopped context. */
    void*owner=F(q->context,0x48,void*);void*reference=static_cast<char*>(q->context)+0x48;
    void*mutex=static_cast<char*>(owner)+0x220;
    reinterpret_cast<void(*)(void*)>(q->base+0xf40e50)(mutex);
    /* Native post can WAIT or execute queued actions inline when full. Never
     * enter that path from UI or the queue's own callback. Under its producer
     * mutex, reserve only the immediately available slot; reader only frees.
     *2b59300 refreshes cached read+10 from acquire read+18 before posting. */
    void*queue=static_cast<char*>(owner)+0x1d8;
    uint64_t size=F(queue,0,uint64_t),write=__atomic_load_n(reinterpret_cast<uint64_t*>(static_cast<char*>(queue)+0x20),__ATOMIC_ACQUIRE);
    uint64_t read=__atomic_load_n(reinterpret_cast<uint64_t*>(static_cast<char*>(queue)+0x18),__ATOMIC_ACQUIRE);
    void*slots=F(queue,0x30,void*);
    if(size<2||write>=size||read>=size||!slots||F(queue,0x2d,unsigned char)||
       (write+1==size?0:write+1)==read||
       (__atomic_load_n(reinterpret_cast<uintptr_t*>(static_cast<char*>(slots)+write*0xc0+0xb0),__ATOMIC_ACQUIRE)&3)){
        reinterpret_cast<void(*)(void*)>(q->base+0xf40e90)(mutex);return false;
    }
    QueueAction action{reinterpret_cast<uintptr_t>(run),q->base+0x1517b30,reference,q,0};
    __atomic_fetch_add(reinterpret_cast<unsigned*>(static_cast<char*>(reference)+8),1,__ATOMIC_ACQUIRE);
    int accepted=0;
    try{
    accepted=reinterpret_cast<int(*)(void*,void*,uintptr_t)>(q->base+0x2b59300)
        (static_cast<char*>(owner)+0x1d8,&action,reinterpret_cast<uintptr_t>(reference)&~uintptr_t(3))&255;
    }catch(...){accepted=0;}
    reinterpret_cast<void(*)(void*)>(q->base+0xf40e90)(mutex);
    reinterpret_cast<int(*)(int,void*,void*)>(action.manage)(1,&action.reference,nullptr);
    if(!accepted)__atomic_fetch_sub(reinterpret_cast<unsigned*>(static_cast<char*>(reference)+8),1,__ATOMIC_RELEASE);
    return accepted;
}
extern "C" resample_queue *resample_queue_create(uintptr_t base,void*context,int(*tick)(void*),void*user){
    if(!base||!context||!tick||!F(context,0x48,void*))return nullptr;
    auto*q=new(std::nothrow)resample_queue{};if(q){q->base=base;q->context=context;q->tick=tick;q->user=user;}return q;
}
extern "C" int resample_queue_wake(resample_queue*q){
    if(!q)return 0;
    std::lock_guard<std::mutex> lock(q->mutex);if(q->stopping)return 0;++q->generation;
    if(q->pending)return 1;
    q->pending=true;q->failed=false;
    if(post(q))return 1;
    q->pending=false;q->failed=true;return 0;
}
extern "C" void resample_queue_stop(resample_queue*q){
    if(q){std::lock_guard<std::mutex> lock(q->mutex);q->stopping=true;}
}
extern "C" int resample_queue_status(resample_queue*q){
    if(!q)return -1;
    std::lock_guard<std::mutex> lock(q->mutex);return q->pending?1:q->stopping?-2:q->failed?-1:0;
}
extern "C" int resample_queue_destroy(resample_queue*q){
    if(!q)return 1;
    {std::lock_guard<std::mutex> lock(q->mutex);if(q->pending)return 0;}
    delete q;return 1;
}
