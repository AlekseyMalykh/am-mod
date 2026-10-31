#include "pad_resample_history.h"
#include <memory>
#include <cstring>
#include <new>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
struct HistoryLease {
    void *user;void(*release)(void*)=nullptr;
    explicit HistoryLease(const pad_commit_context&c):user(c.user){
        if(c.retain){if(!c.retain(c.user))throw std::bad_alloc();release=c.release;}
    }
    ~HistoryLease(){if(release)release(user);}
};
struct History {
    alignas(8) unsigned char header[0x50]{};
    uintptr_t base,table[11]{};
    pad_resample_commit *commit=nullptr;
    void *user;
    void (*hold)(void*,void*);
    HistoryLease lease;
    void *recovery_next=nullptr;
    explicit History(const pad_commit_context&c,void(*h)(void*,void*)):
        base(c.base),user(c.user),hold(h),lease(c){
        uint64_t title=0;
        reinterpret_cast<void(*)(void*,const char*)>(base+0xf535c0)(&title,"Resample Pad");
        try{reinterpret_cast<void(*)(void*,const uint64_t*,int,int)>(base+0x19c9850)(this,&title,1,0);}
        catch(...){reinterpret_cast<void(*)(void*)>(base+0xf0a290)(&title);throw;}
        reinterpret_cast<void(*)(void*)>(base+0xf0a290)(&title);
        std::memcpy(table,reinterpret_cast<void*>(base+0x6e439a0),sizeof(table));
    }
    ~History(){
        reinterpret_cast<void(*)(void*)>(base+0xf0a290)(header+0x38);
        reinterpret_cast<void(*)(void*)>(base+0xf0a290)(header+0x30);
        reinterpret_cast<void(*)(void*)>(base+0x236f110)(header+8);
    }
};
static void dispose(History*p)noexcept{
    if(p->commit&&!pad_commit_destroy(p->commit)){
        /* No native base/context destruction while recovery still owns
         * model references. Explicit host handoff, never silent freeing. */
        p->hold(p->user,p);return;
    }
    p->commit=nullptr;delete p;
}
static int perform_history(History*p)noexcept{return pad_commit_apply(p->commit)?0:1;}
static int undo_history(History*p)noexcept{return pad_commit_undo(p->commit)?0:1;}
extern "C" int pad_history_prepare(const pad_commit_context*c,const uintptr_t sample[2],
    unsigned target,int overwrite,void(*hold)(void*,void*),uintptr_t out[2]){
    if(!out)return 0;
    out[0]=out[1]=0;if(!hold)return 0;
    pad_resample_commit*commit=pad_commit_prepare(c,sample,target,overwrite);if(!commit)return 0;
    try{
        auto*raw=new History(*c,hold);raw->commit=commit;commit=nullptr;
        raw->table[0]=reinterpret_cast<uintptr_t>(dispose);
        raw->table[1]=reinterpret_cast<uintptr_t>(dispose);
        raw->table[8]=reinterpret_cast<uintptr_t>(perform_history);
        raw->table[9]=reinterpret_cast<uintptr_t>(undo_history);F(raw,0,uintptr_t*)=raw->table;
        std::shared_ptr<History> owned(raw,dispose);
        static_assert(sizeof(owned)==2*sizeof(uintptr_t),"Pinned native shared_ptr");
        std::memcpy(out,&owned,sizeof(owned));new(&owned) std::shared_ptr<History>();return 1;
    }catch(...){pad_commit_destroy(commit);return 0;}
}
extern "C" int pad_history_finish_recovery(void*p){
    if(!p)return 0;
    auto*h=static_cast<History*>(p);
    if(!pad_commit_recover(h->commit)||!pad_commit_destroy(h->commit))return 0;
    h->commit=nullptr;delete h;return 1;
}
extern "C" void **pad_history_recovery_next(void*p){return p?&static_cast<History*>(p)->recovery_next:nullptr;}
extern "C" void *pad_history_recovery_user(void*p){return p?static_cast<History*>(p)->user:nullptr;}
