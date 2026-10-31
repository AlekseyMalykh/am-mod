#include "pad_resample_commit.h"
#include "pad_assignment.h"
#include "pad_sample_pool.h"
#include <new>
#include <cstring>
#include <string>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
extern "C" void pad_source_layer_sample(void*,int,uintptr_t*,uintptr_t);
extern "C" void pad_assignment_pool_find(void*,const uint64_t*,uintptr_t*,uintptr_t);
struct Snapshot {uintptr_t samples[8][2]{};int tune[8]{};std::string name[8];};
struct pad_resample_commit {
    pad_commit_context context;
    uintptr_t sample[2]{},command[2]{};
    void *instrument;
    unsigned target;
    Snapshot before,after;
    pad_commit_state state=PAD_COMMIT_READY;
    bool needs_restore=false;
    bool leased=false;
};
static void retain(const uintptr_t in[2],uintptr_t out[2]){
    out[0]=in[0];out[1]=in[1];
    if(in[1])__atomic_fetch_add(reinterpret_cast<unsigned*>(in[1]+8),1,__ATOMIC_ACQUIRE);
}
static void release(pad_resample_commit*j,Snapshot&s){
    for(auto &p:s.samples)pad_assignment_release(j->context.base,p);
}
static bool capture(pad_resample_commit*j,Snapshot&s){
    try{
    void*layers=F(j->instrument,0x22e8,void*);
    void*end=F(j->instrument,0x22f0,void*);
    if(!layers||reinterpret_cast<uintptr_t>(end)-reinterpret_cast<uintptr_t>(layers)!=8*0xb30)return false;
    for(unsigned i=0;i<8;++i){
        pad_source_layer_sample(j->instrument,(int)i,s.samples[i],j->context.base+0x2803b90);
        s.tune[i]=F(static_cast<char*>(layers)+i*0xb30,0x418,int);
        const char*name=s.samples[i][0]?F(reinterpret_cast<void*>(s.samples[i][0]),0xa8,const char*):nullptr;
        s.name[i]=name?name:"";
    }
    return true;
    }catch(...){release(j,s);return false;}
}
static bool context_valid(pad_resample_commit*j,pad_commit_operation operation){
    const auto&c=j->context;
    /* Owner guard must run BEFORE dereferencing cached model pointers. A
     * project switch can invalidate them while the context lease survives. */
    if(!c.validate(c.user,operation)||F(c.properties,0x1278,void*)!=c.pool||
       reinterpret_cast<void*(*)(void*,int)>(c.base+0x286bbe0)(c.program,(int)j->target)!=j->instrument||
       F(j->instrument,0x2310,void*)!=c.program)return false;
    return true;
}
static bool valid(pad_resample_commit*j,pad_commit_operation operation,const Snapshot&expected){
    if(!context_valid(j,operation))return false;
    Snapshot now;bool ok=capture(j,now);
    if(ok)for(unsigned i=0;i<8;++i)
        if(now.samples[i][0]!=expected.samples[i][0]||now.samples[i][1]!=expected.samples[i][1]||
           now.tune[i]!=expected.tune[i]||now.name[i]!=expected.name[i])ok=false;
    release(j,now);return ok;
}
static int native_call(pad_resample_commit*j,unsigned slot){
    void*p=reinterpret_cast<void*>(j->command[0]);
    auto table=F(p,0,uintptr_t*);
    return reinterpret_cast<int(*)(void*)>(table[slot])(p);
}
extern "C" pad_resample_commit *pad_commit_prepare(const pad_commit_context*c,
    const uintptr_t sample[2],unsigned target,int overwrite){
    if(!c||!c->base||!c->properties||!c->program||!c->service||!c->pool||!c->validate||
       !sample||!sample[0]||!sample[1]||target>=128||(!c->retain!=!c->release))return nullptr;
    auto*j=new(std::nothrow)pad_resample_commit{};if(!j)return nullptr;
    j->context=*c;j->target=target;retain(sample,j->sample);
    try{
        if(c->retain){if(!c->retain(c->user))goto rejected;j->leased=true;}
        if(!c->validate(c->user,PAD_COMMIT_APPLY))goto rejected;
        j->instrument=reinterpret_cast<void*(*)(void*,int)>(c->base+0x286bbe0)(c->program,(int)target);
        if(!j->instrument||F(j->instrument,0x2310,void*)!=c->program||
           F(c->properties,0x1278,void*)!=c->pool||
           pad_sample_pool_contains(c->base,c->pool,sample)||!capture(j,j->before))goto rejected;
        if(!overwrite)for(auto&p:j->before.samples)if(p[0])goto rejected;
        return j;
    }catch(...){}
rejected:
    pad_commit_destroy(j);return nullptr;
}
static bool remove_result(pad_resample_commit*j){
    return pad_sample_pool_remove(j->context.base,j->context.pool,j->sample)!=0;
}
extern "C" int pad_commit_apply(pad_resample_commit*j){
    if(!j||(j->state!=PAD_COMMIT_READY&&j->state!=PAD_COMMIT_UNDONE))return 0;
    bool performed=false;
    try{
        if(!valid(j,PAD_COMMIT_APPLY,j->before)||
           pad_sample_pool_contains(j->context.base,j->context.pool,j->sample))return 0;
        if(!pad_sample_pool_register(j->context.base,j->context.pool,j->sample)){
            if(!remove_result(j))j->state=PAD_COMMIT_RECOVERY_REQUIRED;
            return 0;
        }
        /* Registration can rename after Undo if another sample took its name.
         * Never reuse the old native command, which resolves by cached name. */
        pad_assignment_release(j->context.base,j->command);
        if(!pad_assignment_prepare(j->context.base,j->context.properties,j->context.program,
            j->context.service,j->context.pool,j->sample,j->target,0,j->command)){
            if(!remove_result(j))j->state=PAD_COMMIT_RECOVERY_REQUIRED;
            return 0;
        }
        performed=true;j->needs_restore=true;
        if(native_call(j,8)!=0)throw 0;
        release(j,j->after);
        if(!capture(j,j->after)||j->after.samples[0][0]!=j->sample[0])throw 0;
        for(unsigned i=1;i<8;++i)if(j->before.samples[i][0]!=j->after.samples[i][0]||
            j->before.samples[i][1]!=j->after.samples[i][1]||j->before.tune[i]!=j->after.tune[i])throw 0;
        j->state=PAD_COMMIT_APPLIED;return 1;
    }catch(...){
        /* A command can throw after some writes. Restore its saved eight-layer
         * snapshot before unregistering the generated Sample. */
        try{
            if(performed&&(native_call(j,9)!=0||!valid(j,PAD_COMMIT_APPLY,j->before))){
                j->state=PAD_COMMIT_RECOVERY_REQUIRED;return 0;
            }
            j->needs_restore=false;
            if(!remove_result(j))j->state=PAD_COMMIT_RECOVERY_REQUIRED;
        }catch(...){j->state=PAD_COMMIT_RECOVERY_REQUIRED;}
        return 0;
    }
}
static bool originals_registered(pad_resample_commit*j){
    for(unsigned i=0;i<8;++i){auto&p=j->before.samples[i];if(!p[0])continue;
        const char*name=F(reinterpret_cast<void*>(p[0]),0xa8,const char*);
        if(!name||j->before.name[i]!=name||!pad_sample_pool_contains(j->context.base,j->context.pool,p))return false;
        uintptr_t found[2]={};
        /* Lookup is also required: membership alone cannot prove the cached
         * name still resolves to the exact original Sample. */
        try{pad_assignment_pool_find(j->context.pool,reinterpret_cast<const uint64_t*>(reinterpret_cast<char*>(p[0])+0xa8),found,j->context.base+0x292ed50);}
        catch(...){pad_assignment_release(j->context.base,found);return false;}
        bool ok=found[0]==p[0]&&found[1]==p[1];pad_assignment_release(j->context.base,found);
        if(!ok)return false;
    }
    return true;
}
extern "C" int pad_commit_undo(pad_resample_commit*j){
    if(!j||j->state!=PAD_COMMIT_APPLIED)return 0;
    try{
        if(!valid(j,PAD_COMMIT_UNDO,j->after))return 0;
        /* Native Undo restores ALL layer names; if an original has left the
         * library, do not silently restore an empty layer in its place. */
        if(!originals_registered(j))return 0;
        if(native_call(j,9)!=0||!valid(j,PAD_COMMIT_UNDO,j->before)){
            j->state=PAD_COMMIT_RECOVERY_REQUIRED;return 0;
        }
        j->needs_restore=false;
        if(!remove_result(j)){j->state=PAD_COMMIT_RECOVERY_REQUIRED;return 0;}
        j->state=PAD_COMMIT_UNDONE;return 1;
    }catch(...){j->state=PAD_COMMIT_RECOVERY_REQUIRED;return 0;}
}
extern "C" int pad_commit_recover(pad_resample_commit*j){
    if(!j||j->state!=PAD_COMMIT_RECOVERY_REQUIRED)return 0;
    try{
        if(!context_valid(j,PAD_COMMIT_RECOVER)||!originals_registered(j))return 0;
        if(j->needs_restore){
            /* Native Undo clears its cached instrument. Never call it twice
             * after a partial/failed receiver update; require host recovery. */
            if(!valid(j,PAD_COMMIT_RECOVER,j->before))return 0;
            j->needs_restore=false;
        }
        if(!remove_result(j))return 0;
        j->state=PAD_COMMIT_UNDONE;return 1;
    }catch(...){return 0;}
}
extern "C" pad_commit_state pad_commit_status(const pad_resample_commit*j){return j?j->state:PAD_COMMIT_RECOVERY_REQUIRED;}
extern "C" int pad_commit_destroy(pad_resample_commit*j){
    if(!j)return 1;
    if(j->state==PAD_COMMIT_RECOVERY_REQUIRED)return 0;
    pad_assignment_release(j->context.base,j->command);
    release(j,j->before);release(j,j->after);pad_assignment_release(j->context.base,j->sample);
    auto context=j->context;bool leased=j->leased;delete j;
    if(leased)context.release(context.user);
    return 1;
}
