#include "src/arrangement_native_material.h"
#include "src/pattern_tools.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <cstddef>
#include <atomic>
#include <chrono>
#include <thread>
namespace {
long fail_after=-1;
void(*native_alloc_observer)()=nullptr;
void(*native_delete_observer)()=nullptr;
std::uint64_t store_row=0;unsigned retire_reads=0;
std::atomic<std::size_t>live_blocks{0},native_blocks{0},native_bytes{0},peak_native_bytes{0};
struct alignas(std::max_align_t) Allocation {std::size_t bytes;bool native;};
void*allocate(std::size_t n,bool native){
    if(fail_after==0)throw std::bad_alloc();
    if(fail_after>0)--fail_after;
    auto*p=static_cast<Allocation*>(std::malloc(sizeof(Allocation)+(n?n:1)));
    if(!p)throw std::bad_alloc();
    p->bytes=n;p->native=native;++live_blocks;
    if(native){++native_blocks;auto bytes=native_bytes.fetch_add(n)+n;auto peak=peak_native_bytes.load();
        while(peak<bytes&&!peak_native_bytes.compare_exchange_weak(peak,bytes)){} }
    return p+1;
}
void release(void*p)noexcept{
    if(!p)return;
    auto*h=static_cast<Allocation*>(p)-1;assert(live_blocks);--live_blocks;
    if(h->native){if(native_delete_observer)native_delete_observer();assert(native_blocks&&native_bytes>=h->bytes);--native_blocks;native_bytes-=h->bytes;}
    std::free(h);
}
void*native_new(std::size_t n){void*p=allocate(n,true);
    if(native_alloc_observer){auto observe=native_alloc_observer;native_alloc_observer=nullptr;try{observe();}catch(...){release(p);throw;}}
    return p;
}
void native_delete(void*p){release(p);}
void native_sized_delete(void*p,std::size_t){release(p);}
template<class T>T field(const void*p,unsigned offset){T result;std::memcpy(&result,static_cast<const char*>(p)+offset,sizeof result);return result;}
using namespace am_pattern;
bool same_note(const step_event&a,const step_event&b){
    /* Stock tag-3 copy writes time/type and 37 payload bytes, not the ABI
     * padding at 12..15 / 53..55. Compare every musical byte, not padding. */
    return a.time==b.time&&a.type==b.type&&!std::memcmp(a.data,b.data,37);
}
void edit_during_prepare(){assert(pt_arr_track_state(store_row,1,0));}
void reset_during_prepare(){pt_reset();}
void inspect_during_retire(){pt_arrangement state{};assert(pt_arr_info(&state));++retire_reads;}
std::vector<step_event>read(std::uintptr_t image,const NativePlaybackMaterial&material){
    const auto*source=static_cast<const unsigned char*>(material.events());
    assert(field<std::uintptr_t>(source,0)==image+0x6d8c0e8);
    auto*end=source+0x68;const void*node=field<const void*>(source,0x78);
    std::vector<step_event>events;std::vector<unsigned char>seen(material.plan().notes.size());
    while(node!=end){assert(events.size()<max_events);events.push_back(field<step_event>(node,0x20));
        auto*event=static_cast<const unsigned char*>(node)+0x20;auto*origin=material.origin(event);assert(origin&&origin->event&&origin->event<=seen.size());
        assert(same_note(origin->native,events.back())&&!seen[origin->event-1]);seen[origin->event-1]=1;
        node=reinterpret_cast<void*(*)(const void*)>(image+0xb51738)(node);}
    assert(events.size()==field<std::uint64_t>(source,0x88));return events;
}
}
void*operator new(std::size_t n){return allocate(n,false);}
void*operator new[](std::size_t n){return allocate(n,false);}
void operator delete(void*p)noexcept{release(p);}
void operator delete[](void*p)noexcept{release(p);}
void operator delete(void*p,std::size_t)noexcept{release(p);}
void operator delete[](void*p,std::size_t)noexcept{release(p);}
void*operator new(std::size_t n,const std::nothrow_t&)noexcept{try{return allocate(n,false);}catch(...){return nullptr;}}
void*operator new[](std::size_t n,const std::nothrow_t&)noexcept{try{return allocate(n,false);}catch(...){return nullptr;}}
void operator delete(void*p,const std::nothrow_t&)noexcept{release(p);}
void operator delete[](void*p,const std::nothrow_t&)noexcept{release(p);}
extern "C" void*check_native_symbol(const char*name,void*original){
    if(!std::strcmp(name,"_Znwm")||!std::strcmp(name,"_Znam"))return reinterpret_cast<void*>(native_new);
    if(!std::strcmp(name,"_ZdlPv")||!std::strcmp(name,"_ZdaPv"))return reinterpret_cast<void*>(native_delete);
    if(!std::strcmp(name,"_ZdlPvm")||!std::strcmp(name,"_ZdaPvm"))return reinterpret_cast<void*>(native_sized_delete);
    return original;
}
extern "C" int check_arrangement_material(std::uintptr_t image){
    using Material=NativePlaybackMaterial;using Code=NativeMaterialResult;
    const auto baseline=live_blocks.load();assert(!native_blocks&&!native_bytes);
    {
        NativeArrangementPlan plan;auto&d=plan.destination;
        d.project_epoch=7;d.routing_revision=2;d.destination_revision=3;d.program_key="owned-native-events";
        d.sequence_ticks=30720;d.transport_valid=true;plan.arrangement_revision=9;plan.library_revision=10;plan.store_receipt=11;
        for(unsigned i=0;i<128;++i)d.pad_to_note[i]=d.note_to_pad[i]=int(i);
        auto factory=reinterpret_cast<step_event(*)(unsigned,std::int64_t,std::int64_t,unsigned,unsigned,float)>(image+0x27e54d0);
        auto set=reinterpret_cast<void(*)(void*,int,int,int)>(image+0x27e4400);
        for(unsigned i=0;i<7;++i){NativeBoundNote n;n.track=i+1;n.clip=i+8;n.pattern=21;n.event=i+1;n.pad=n.source_note=50;
            n.native=factory(50,i<4?0:std::int64_t(i-3)*7680,40000,100,2,.8f);
            if(i!=1)set(n.native.data+4,0,0,12-int(i)); // includes an exact duplicate below
            n.native.data[34]=61;n.native.data[36]=(n.native.data[36]&~7u)|3u;
            plan.notes.push_back(n);}
        plan.notes[2].native=plan.notes[1].native;
        auto expected=plan.notes;auto less=reinterpret_cast<int(*)(const step_event*,const step_event*)>(image+0x19d4d70);
        std::stable_sort(expected.begin(),expected.end(),[&](const NativeBoundNote&a,const NativeBoundNote&b){return less(&a.native,&b.native)&255;});
        std::shared_ptr<const Material>material;assert(Material::compatible(image)&&Material::prepare(image,plan,material)==Code::ok);
        auto stored=read(image,*material);assert(stored.size()==plan.notes.size()&&material->plan().store_receipt==11);
        assert(!material->origin(nullptr)&&!material->origin(&plan.notes[0].native)&&!material->origin(reinterpret_cast<void*>(UINTPTR_MAX)));
        for(unsigned i=0;i<stored.size();++i)assert(same_note(stored[i],expected[i].native));
        assert(reinterpret_cast<std::int64_t(*)(const void*)>(image+0x27eb1d0)(material->events())==63040);
        auto held=material;material.reset();auto saved=stored;
        assert(read(image,*held).size()==7); // consumer retains source after producer drops it
        auto refuse=[&](const NativeArrangementPlan&bad){auto old=held;auto blocks=live_blocks.load();
            assert(Material::prepare(image,bad,held)==Code::invalid&&held==old&&blocks==live_blocks);};
        auto bad=plan;bad.notes[0].native.type=4;refuse(bad);bad=plan;bad.notes[0].native.type|=0x100u;refuse(bad);
        bad=plan;bad.notes[0].native.data[32]=49;refuse(bad);bad=plan;bad.notes[0].pad=128;refuse(bad);
        bad=plan;bad.destination.program_key=unassigned_program_key;refuse(bad);
        bad=plan;bad.destination.playing=true;refuse(bad);bad=plan;bad.destination.note_to_pad[50]=-1;refuse(bad);
        bad=plan;bad.notes.back().native.time=d.sequence_ticks;refuse(bad);bad=plan;bad.notes[1].native.time=-1;refuse(bad);
        bad=plan;std::int64_t overflow=INT64_MAX;std::memcpy(bad.notes.back().native.data+24,&overflow,8);refuse(bad);
        for(auto offset:{0x27eb120u,0x170a2f0u,0x14c4a20u,0x14c4c88u,0x6d8c0e8u}){
            auto*word=reinterpret_cast<std::uint32_t*>(image+offset);auto original=*word;*word^=1;
            auto old=held;assert(Material::prepare(image,plan,held)==Code::unsupported&&held==old);*word=original;
        }
        auto blocks=live_blocks.load();auto nodes=native_blocks.load();unsigned failures=0;bool succeeded=false;
        for(long point=0;point<256;++point){
            std::shared_ptr<const Material>out=held;fail_after=point;
            auto code=Material::prepare(image,plan,out);fail_after=-1;
            if(code==Code::ok){out.reset();assert(live_blocks==blocks&&native_blocks==nodes);succeeded=true;break;}
            assert(code==Code::allocation_failed&&out==held&&live_blocks==blocks&&native_blocks==nodes);++failures;
        }
        assert(succeeded&&failures>plan.notes.size());
        auto after=read(image,*held);assert(after.size()==saved.size());
        for(unsigned i=0;i<after.size();++i)assert(!std::memcmp(&after[i],&saved[i],sizeof(step_event)));
        auto empty=plan;empty.notes.clear();std::shared_ptr<const Material>nothing;
        assert(Material::prepare(image,empty,nothing)==Code::ok&&read(image,*nothing).empty());
        {
            std::shared_ptr<const Material>source;assert(Material::prepare(image,plan,source)==Code::ok);
            auto*events=static_cast<const unsigned char*>(source->events());void*lease=nullptr;
            /* Copy the real native lifetime control with its own helper. No
             * guessed refcount increment and no extra C++ Material owner. */
            reinterpret_cast<void(*)(void*,const void*)>(image+0x13b35c0)(&lease,events+0x150);
            assert(lease&&__atomic_load_n(static_cast<unsigned*>(lease)+2,__ATOMIC_ACQUIRE)==2);
            std::atomic<unsigned>go{0},done{0};
            std::thread retire([&](std::shared_ptr<const Material>owned){
                while(!go.load(std::memory_order_acquire))std::this_thread::yield();
                owned.reset();done.store(1,std::memory_order_release);
            },source);
            source.reset();go.store(1,std::memory_order_release);
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
            const auto*flag=reinterpret_cast<const unsigned*>(events+0x158);
            while(!done.load(std::memory_order_acquire)&&!(__atomic_load_n(flag,__ATOMIC_ACQUIRE)&2u)
                  &&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
            assert(!done.load()&&(__atomic_load_n(flag,__ATOMIC_ACQUIRE)&3u)==2u);
            assert(reinterpret_cast<std::int64_t(*)(const void*)>(image+0x27eb1d0)(events)==63040);
            reinterpret_cast<void(*)(void*)>(image+0x123bca0)(lease);retire.join();assert(done.load());
            puts("PASS native Holder lifetime: actual native destructor waits for native source lease; tree remains readable during retirement and is released after last lease; no Project/Track lease claimed");
        }
        auto large=plan;large.notes.assign(max_events+1,plan.notes[0]);refuse(large);large.notes.resize(max_events);
        {std::shared_ptr<const Material>limit;assert(Material::prepare(image,large,limit)==Code::ok);
            assert(field<std::uint64_t>(limit->events(),0x88)==max_events);}
        std::printf("PASS native Arrange material: real Events ctor/insert/comparator/iterator/tail query/dtor, duplicates and inline choices, retained owner, atomic failures (%u allocation points), no native destination pointers; peak native allocations %zu bytes\n",failures,peak_native_bytes.load());
    }
    assert(live_blocks==baseline&&!native_blocks&&!native_bytes);
    {
        pt_init(image);pt_reset();pt_expect_epoch(pt_epoch());
        int mapping[128],outputs[128];for(unsigned i=0;i<128;++i){mapping[i]=int(i);outputs[i]=0;}
        pr_note notes[3]{};auto factory=reinterpret_cast<step_event(*)(unsigned,std::int64_t,std::int64_t,unsigned,unsigned,float)>(image+0x27e54d0);
        for(unsigned i=0;i<3;++i){notes[i].sound=50;notes[i].selected=1;notes[i].event=factory(50,i*960,10000,100,0,.8f);}
        std::uint64_t clip=0;assert(pt_capture(notes,3,3840,1,"store-material",mapping,outputs,2));
        assert(pt_arr_add_track("Part",&store_row)&&pt_arr_add_clip(store_row,0,0,3840,&clip)&&pt_arr_length(3840));
        NativeDestination destination;destination.project_epoch=pt_epoch();destination.routing_revision=1;destination.destination_revision=1;
        destination.program_key="store-material";destination.sequence_ticks=3840;destination.transport_valid=true;
        for(unsigned i=0;i<128;++i)destination.pad_to_note[i]=destination.note_to_pad[i]=int(i);
        std::shared_ptr<const Material>ready;assert(pt_prepare_native_material(destination,ready)&&read(image,*ready).size()==3);
        auto retained=ready;native_alloc_observer=edit_during_prepare;
        assert(!pt_prepare_native_material(destination,ready)&&ready==retained&&!native_alloc_observer);
        assert(!pt_native_plan_current(ready->plan(),destination)&&read(image,*ready).size()==3);
        assert(pt_history(-1)&&!pt_native_plan_current(ready->plan(),destination));retained.reset();
        std::weak_ptr<const Material>retired=ready;native_delete_observer=inspect_during_retire;
        assert(pt_prepare_native_material(destination,ready)&&retired.expired()&&retire_reads);
        retained=ready;native_alloc_observer=reset_during_prepare;
        assert(!pt_prepare_native_material(destination,ready)&&ready==retained&&!native_alloc_observer);
        assert(!pt_native_plan_current(ready->plan(),destination)&&read(image,*ready).size()==3);
        retained.reset();ready.reset();native_delete_observer=nullptr;pt_expect_epoch(0);pt_reset();
    }
    assert(!native_blocks&&!native_bytes);
    puts("PASS material Store bridge: edit/Undo and project reset during real native allocation reject stale publication without invalidating retained source; native retirement reenters Store outside its mutex; no native destination objects");
    return 0;
}
