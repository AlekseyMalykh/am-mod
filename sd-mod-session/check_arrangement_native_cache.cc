/* Private ELF proof of stock cache copy -> queued audio swap -> retirement.
 * Queue storage and the cache wrapper/guard below are explicit fixtures.
 * No native Project, Track, Program, full cache constructor or transport is
 * claimed. The copy, swap, weak-lock, tree, fences and destructors are stock. */
#define check_arrangement_material check_material_before_cache
#include "check_arrangement_native_material.cc"
#undef check_arrangement_material
#include <pthread.h>

namespace {
std::uintptr_t cache_image;
alignas(8) unsigned char queued_cache_task[48];
bool cache_task_pending=false,cache_audio=false;
unsigned cache_enqueues=0;
void cache_no_allocate(){assert(!cache_audio);}
void cache_no_delete(){assert(!cache_audio);}
void cache_enqueue(void*queue,void*task,void*optional){
    assert(queue&&!optional&&!cache_audio&&!cache_task_pending);
    /* The fixture moves the native closure, never calls a fake swap/copy.
     * Clearing the moved payload lets stock caller cleanup run normally. */
    std::memcpy(queued_cache_task,task,sizeof queued_cache_task);
    std::memset(static_cast<char*>(task)+16,0,24);
    cache_task_pending=true;++cache_enqueues;
}
void run_cache_task(){
    assert(cache_task_pending&&!cache_audio);
    cache_audio=true;
    auto blocks=native_blocks.load(),bytes=native_bytes.load(),all=live_blocks.load();
    native_alloc_observer=cache_no_allocate;native_delete_observer=cache_no_delete;
    fail_after=0; // Both C++ and native test allocators reject audio allocation.
    reinterpret_cast<void(*)(void*)>(field<std::uintptr_t>(queued_cache_task,0))(queued_cache_task+16);
    fail_after=-1;
    native_alloc_observer=nullptr;native_delete_observer=nullptr;
    assert(blocks==native_blocks&&bytes==native_bytes&&all==live_blocks);cache_audio=false;
}
void retire_cache_task(){
    assert(cache_task_pending&&!cache_audio);
    reinterpret_cast<void(*)(unsigned,void*,void*)>(field<std::uintptr_t>(queued_cache_task,8))(1,queued_cache_task+16,nullptr);
    std::memset(queued_cache_task,0,sizeof queued_cache_task);cache_task_pending=false;
}
void cache_patch(std::uintptr_t offset,void*target){
    auto*p=reinterpret_cast<unsigned char*>(cache_image+offset);
    const std::uint32_t code[]={0x58000050,0xd61f0200};
    std::memcpy(p,code,sizeof code);std::memcpy(p+8,&target,8);
    __builtin___clear_cache(reinterpret_cast<char*>(p),reinterpret_cast<char*>(p+16));
}
template<class T>void cache_put(void*p,unsigned offset,T value){std::memcpy(static_cast<char*>(p)+offset,&value,sizeof value);}
struct CacheFixture {
    alignas(8) unsigned char body[0x248]{};
    alignas(8) unsigned char lifetime[0x160]{};
    alignas(8) unsigned char queue[0x80]{};
    bool live=true;
    CacheFixture(){
        auto ctor=reinterpret_cast<void(*)(void*)>(cache_image+0x170a2f0);
        ctor(body+0x98);ctor(lifetime);
        cache_put(body,0,cache_image+0x6d906c8);
        cache_put(body,8,static_cast<void*>(queue));
        cache_put(body,0x28,static_cast<void*>(body));
        auto*control=field<unsigned char*>(lifetime,0x150);
        // One fixture weak self in addition to the real Holder's references.
        __atomic_fetch_add(reinterpret_cast<unsigned*>(control+12),1u,__ATOMIC_ACQ_REL);
        cache_put(body,0x30,control);
        assert(!pthread_mutex_init(reinterpret_cast<pthread_mutex_t*>(queue+0x48),nullptr));
    }
    void expire(){assert(live);reinterpret_cast<void(*)(void*)>(cache_image+0x14c4a20)(lifetime);live=false;}
    ~CacheFixture(){
        assert(!cache_task_pending);
        if(live)expire();
        reinterpret_cast<void(*)(void*)>(cache_image+0x13bb720)(field<void*>(body,0x30));
        reinterpret_cast<void(*)(void*)>(cache_image+0x14c4a20)(body+0x98);
        assert(!pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t*>(queue+0x48)));
    }
    void publish(const void*source){
        const void*pair[]={source,source?field<const void*>(source,0x150):nullptr};
        reinterpret_cast<void(*)(void*,const void*)>(cache_image+0x170b060)(body,pair);
        assert(cache_task_pending);
    }
    std::vector<step_event>events()const{
        std::vector<step_event>out;auto*s=body+0x98;auto*end=s+0x68;
        const void*n=field<const void*>(s,0x78);
        while(n!=end){assert(out.size()<max_events);out.push_back(field<step_event>(n,0x20));
            n=reinterpret_cast<void*(*)(const void*)>(cache_image+0xb51738)(n);}
        assert(out.size()==field<std::uint64_t>(s,0x88));return out;
    }
};
}

extern "C" int check_arrangement_material(std::uintptr_t image){
    assert(!check_material_before_cache(image));cache_image=image;
    auto original=std::array<unsigned char,16>{};std::memcpy(original.data(),reinterpret_cast<void*>(image+0x2b59300),16);
    cache_patch(0x2b59300,reinterpret_cast<void*>(cache_enqueue));
    auto blocks=live_blocks.load(),native=native_blocks.load();
    {
        CacheFixture cache;NativeArrangementPlan plan;plan.arrangement_revision=plan.library_revision=1;auto&d=plan.destination;
        d.project_epoch=d.routing_revision=d.destination_revision=1;d.program_key="cache-proof";
        d.sequence_ticks=61440;d.transport_valid=true;
        for(unsigned i=0;i<128;++i)d.pad_to_note[i]=d.note_to_pad[i]=int(i);
        auto note=reinterpret_cast<step_event(*)(unsigned,std::int64_t,std::int64_t,unsigned,unsigned,float)>(image+0x27e54d0);
        for(unsigned i=0;i<7;++i){NativeBoundNote n;n.track=i+1;n.clip=i+8;n.pattern=21;n.event=i+1;n.pad=n.source_note=50;
            n.native=note(50,i<4?0:std::int64_t(i-3)*7680,40000,100,2,.8f);plan.notes.push_back(n);}
        std::shared_ptr<const NativePlaybackMaterial>source;
        assert(NativePlaybackMaterial::prepare(image,plan,source)==NativeMaterialResult::ok);
        auto expected=read(image,*source);auto*source_first=field<const void*>(source->events(),0x78);
        std::vector<Id>expected_origins;
        for(const void*n=source_first;n!=static_cast<const unsigned char*>(source->events())+0x68;
            n=reinterpret_cast<const void*(*)(const void*)>(image+0xb51738)(n))
            expected_origins.push_back(source->origin(static_cast<const unsigned char*>(n)+0x20)->event);
        assert(cache.events().empty());
        auto before_publish=live_blocks.load(),native_before_publish=native_blocks.load();
        unsigned failed_copies=0;bool published=false;
        for(long point=0;point<64;++point){
            fail_after=point;
            try{cache.publish(source->events());published=true;}
            catch(const std::bad_alloc&){++failed_copies;}
            fail_after=-1;
            if(published)break;
            assert(!cache_task_pending&&live_blocks==before_publish&&native_blocks==native_before_publish&&cache.events().empty());
        }
        assert(published&&failed_copies>=8);
        assert(cache.events().empty()); // publication is not an immediate audio mutation
        auto*copy=field<const void*>(queued_cache_task,16);
        assert(copy&&field<const void*>(copy,0x78)!=source_first);
        NativePlaybackOriginIndex origins;
        assert(NativePlaybackOriginIndex::prepare(*source,copy,origins)==NativeMaterialResult::ok&&origins.size()==7);
        auto*first_event=static_cast<const unsigned char*>(field<const void*>(copy,0x78))+0x20;
        auto*retained_origin=origins.origin(first_event);assert(retained_origin);
        assert(!source->origin(first_event)&&!origins.origin(nullptr)&&!origins.origin(reinterpret_cast<void*>(UINTPTR_MAX)));
        assert(NativePlaybackOriginIndex::prepare(*source,source->events(),origins)==NativeMaterialResult::invalid);
        assert(NativePlaybackOriginIndex::prepare(*source,nullptr,origins)==NativeMaterialResult::invalid);
        auto allocations=live_blocks.load();unsigned index_failures=0;bool indexed=false;
        for(long point=0;point<16;++point){
            fail_after=point;auto result=NativePlaybackOriginIndex::prepare(*source,copy,origins);fail_after=-1;
            if(result==NativeMaterialResult::ok){indexed=true;break;}
            assert(result==NativeMaterialResult::allocation_failed&&origins.origin(first_event)==retained_origin&&live_blocks==allocations);++index_failures;
        }
        assert(indexed&&index_failures>=2);retained_origin=origins.origin(first_event);
        cache_put(const_cast<void*>(copy),0x88,std::uint64_t(8));
        assert(NativePlaybackOriginIndex::prepare(*source,copy,origins)==NativeMaterialResult::invalid&&origins.origin(first_event)==retained_origin);
        cache_put(const_cast<void*>(copy),0x88,std::uint64_t(7));
        auto changed=field<step_event>(first_event,0);++changed.time;
        std::memcpy(const_cast<unsigned char*>(first_event),&changed,sizeof changed);
        assert(NativePlaybackOriginIndex::prepare(*source,copy,origins)==NativeMaterialResult::invalid&&origins.origin(first_event)==retained_origin);
        --changed.time;std::memcpy(const_cast<unsigned char*>(first_event),&changed,sizeof changed);
        auto destination_snapshot=plan.destination;source.reset();plan=NativeArrangementPlan{};
        run_cache_task();auto actual=cache.events();assert(actual.size()==expected.size());
        for(unsigned i=0;i<actual.size();++i)assert(same_note(actual[i],expected[i]));
        unsigned i=0;
        for(const void*n=field<const void*>(cache.body+0x98,0x78);n!=cache.body+0x100;
            n=reinterpret_cast<const void*(*)(const void*)>(image+0xb51738)(n)){
            auto origin=origins.origin(static_cast<const unsigned char*>(n)+0x20);
            assert(origin&&origin->event==expected_origins[i++]&&origin->track==origin->event);
        }
        assert(i==7);
        assert(reinterpret_cast<std::int64_t(*)(const void*)>(image+0x27eb1d0)(cache.body+0x98)==63040);
        retire_cache_task();assert(cache.events().size()==7);
        // Publish empty: old tree remains until the queued boundary, and its
        // destruction happens on this control owner after the audio swap.
        cache.publish(nullptr);assert(cache.events().size()==7);
        NativeArrangementPlan empty_plan;empty_plan.destination=destination_snapshot;empty_plan.arrangement_revision=empty_plan.library_revision=1;
        std::shared_ptr<const NativePlaybackMaterial>empty_source;
        assert(NativePlaybackMaterial::prepare(image,empty_plan,empty_source)==NativeMaterialResult::ok);
        NativePlaybackOriginIndex empty_origins;
        assert(NativePlaybackOriginIndex::prepare(*empty_source,field<const void*>(queued_cache_task,16),empty_origins)==NativeMaterialResult::ok&&!empty_origins.size());
        empty_source.reset();empty_plan=NativeArrangementPlan{};
        run_cache_task();assert(cache.events().empty());retire_cache_task();
        origins=std::move(empty_origins); // Retired generation: no address reuse lookup.
        // A late queued operation must not touch an expired destination.
        reinterpret_cast<void*(*)(void*,const void*)>(image+0x27ee5d0)(cache.body+0x98,&expected[0]);
        cache.publish(nullptr);auto before=cache.events();cache.expire();run_cache_task();assert(cache.events().size()==before.size());retire_cache_task();
    }
    assert(live_blocks==blocks&&native_blocks==native&&cache_enqueues==3);
    std::memcpy(reinterpret_cast<void*>(image+0x2b59300),original.data(),16);
    __builtin___clear_cache(reinterpret_cast<char*>(image+0x2b59300),reinterpret_cast<char*>(image+0x2b59310));
    puts("PASS native cache handoff: stock copy/queued swap preserve duplicate notes, distinct copied origins and tails after producer release; no audio allocation/native delete; empty publication and expired guard; queue/cache wrapper/guard binding are fixtures, no real transport or Project/Track/Program lease");
    return 0;
}
