/* Actual ClipAudioThreadCache constructor, separate from the event-list cache.
 * ThreadQueues storage is a fixture. No Project/Track/Program or transport is
 * supplied, and this test does not assert audible playback. */
#define check_arrangement_material check_material_before_clip
#include "check_arrangement_native_material.cc"
#undef check_arrangement_material
#include <pthread.h>

namespace {
template<class T>void clip_put(void*p,unsigned offset,T value){
    std::memcpy(static_cast<char*>(p)+offset,&value,sizeof value);
}
bool clip_consumer=false;
void clip_no_allocate(){assert(!clip_consumer);}
void clip_no_delete(){assert(!clip_consumer);}
struct ClipQueue {
    alignas(8) unsigned char queues[0x300]{},slots[4][0xc0]{};
    std::uintptr_t image;std::uint64_t reader=0;
    explicit ClipQueue(std::uintptr_t b):image(b){
        clip_put(queues+0x1d8,0,std::uint64_t(4));
        clip_put(queues+0x1d8,0x30,static_cast<void*>(slots));
        for(auto&s:slots){clip_put(s,0,image+0xb57a7c);clip_put(s,8,image+0x15265b0);}
        reinterpret_cast<void(*)(void*)>(image+0xf406b0)(queues+0x220);
    }
    void reclaim(){reinterpret_cast<void(*)(void*)>(image+0x2b58c70)(queues+0x1d8);}
    void run(){
        std::thread worker([&]{
            auto all=live_blocks.load(),native=native_blocks.load(),bytes=native_bytes.load();
            clip_consumer=true;native_alloc_observer=clip_no_allocate;
            native_delete_observer=clip_no_delete;fail_after=0;
            reinterpret_cast<void(*)(void*,std::uint64_t*)>(image+0x2b59200)(queues+0x1d8,&reader);
            fail_after=-1;native_alloc_observer=nullptr;native_delete_observer=nullptr;clip_consumer=false;
            assert(all==live_blocks&&native==native_blocks&&bytes==native_bytes);
        });worker.join();
    }
    ~ClipQueue(){
        assert(reader==field<std::uint64_t>(queues+0x1d8,0x20));reclaim();
        assert(!pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t*>(queues+0x220)));
    }
};
void post_clip(ClipQueue&queue,void*cache,void*source){
    auto*control=field<unsigned char*>(cache,0x30);assert(control);
    __atomic_fetch_add(reinterpret_cast<unsigned*>(control+12),1u,__ATOMIC_ACQ_REL);
    struct Action {std::uintptr_t invoke,manage;const void*payload[3];};
    Action action{queue.image+0x27c7ff0,queue.image+0x27bdeb0,{source,cache,control}};
    assert(reinterpret_cast<int(*)(void*,void*,std::uintptr_t)>(queue.image+0x2b59300)
        (queue.queues+0x1d8,&action,0)&255);
}
void*empty_clip(std::uintptr_t image){
    void*source=reinterpret_cast<void*(*)(std::size_t)>(image+0xb51418)(0x298);
    reinterpret_cast<void(*)(void*)>(image+0x2973b50)(source);
    return source;
}
}

extern "C" int check_arrangement_material(std::uintptr_t image){
    assert(!check_material_before_clip(image));
    ClipQueue queue(image);
    alignas(8) unsigned char body[0x3d8]{};
    auto before=live_blocks.load();auto native_before=native_blocks.load();
    std::puts("constructing stock ClipAudioThreadCache (null source/Track)");
    reinterpret_cast<void(*)(void*,void*,void*,void*,unsigned)>(image+0x2974cf0)
        (body,queue.queues,nullptr,nullptr,0);
    assert(field<std::uintptr_t>(body,0)==image+0x6dbbad8);
    assert(field<std::uintptr_t>(body,0x130)==image+0x6d8c0e8);
    assert(!field<std::uint64_t>(body,0x130+0x88));
    // This is the actual cache consumed at ClipPlayer::source binding,
    // not SequenceEventListAudioThreadCache with a fabricated wrapper.
    assert(field<void*>(body,8)==queue.queues+0x1d8);
    assert(field<void*>(body,0x90)==queue.queues);
    assert(field<void*>(body,0x28)==body+0x338); // Guard info, not the wrapper.
    auto*control=field<unsigned char*>(body,0x30);assert(control);
    auto*source=static_cast<unsigned char*>(empty_clip(image));
    assert(field<std::uintptr_t>(source,0x98)==image+0x6d8c0e8);
    auto make=reinterpret_cast<step_event(*)(unsigned,std::int64_t,std::int64_t,unsigned,unsigned,float)>(image+0x27e54d0);
    const void*nodes[7]{};step_event original[7]{};
    for(unsigned i=0;i<7;++i){
        auto note=make(50,i<4?0:std::int64_t(i-3)*7680,40000,100,2,.8f);
        original[i]=note;
        nodes[i]=reinterpret_cast<void*(*)(void*,const void*)>(image+0x27ee5d0)(source+0x98,&note);
        assert(nodes[i]);
    }
    NativeArrangementPlan plan;plan.arrangement_revision=plan.library_revision=1;
    auto&destination=plan.destination;
    destination.project_epoch=destination.routing_revision=destination.destination_revision=1;
    destination.transport_valid=true;destination.sequence_ticks=61440;destination.program_key="clip-proof";
    for(unsigned i=0;i<128;++i)destination.pad_to_note[i]=destination.note_to_pad[i]=int(i);
    for(unsigned i=0;i<7;++i){
        NativeBoundNote bound;bound.track=i+1;bound.clip=i+8;bound.pattern=21;bound.event=i+1;
        bound.pad=bound.source_note=50;bound.native=original[i];plan.notes.push_back(bound);
    }
    std::shared_ptr<const NativePlaybackMaterial>material;
    assert(NativePlaybackMaterial::prepare(image,plan,material)==NativeMaterialResult::ok);
    NativePlaybackOriginIndex origins;
    assert(NativePlaybackOriginIndex::prepare(*material,source+0x98,origins)==NativeMaterialResult::ok);
    post_clip(queue,body,source);
    material.reset();plan=NativeArrangementPlan{};
    assert(!field<std::uint64_t>(body,0x130+0x88));
    queue.run();
    assert(field<std::uint64_t>(body,0x130+0x88)==7);
    for(unsigned i=0;i<7;++i){
        auto*event=static_cast<const unsigned char*>(nodes[i])+0x20;
        assert(same_note(field<step_event>(event,0),original[i]));
        auto*origin=origins.origin(event);
        assert(origin&&origin->track==i+1&&origin->clip==i+8&&origin->pattern==21&&origin->event==i+1);
    }
    assert(reinterpret_cast<std::int64_t(*)(const void*)>(image+0x27eb1d0)(body+0x130)==63040);
    queue.reclaim(); // Native manager deletes retired native source on control.
    assert(field<std::uint64_t>(body,0x130+0x88)==7);
    for(unsigned i=0;i<7;++i)
        assert(origins.origin(static_cast<const unsigned char*>(nodes[i])+0x20)->event==i+1);
    origins=NativePlaybackOriginIndex{};
    post_clip(queue,body,empty_clip(image));queue.run();
    assert(!field<std::uint64_t>(body,0x130+0x88));
    queue.reclaim();
    post_clip(queue,body,empty_clip(image));
    reinterpret_cast<void(*)(void*)>(image+0x27c2ac0)(body);
    // A pending native callback holds only weak cache ownership: the actual
    // constructor's guard must expire before it could touch a retired wrapper.
    std::memset(body,0xa5,sizeof body);queue.run();queue.reclaim();
    assert(live_blocks==before&&native_blocks==native_before);
    unsigned failures=0;bool constructed=false;
    for(long point=0;point<64;++point){
        std::memset(body,0,sizeof body);fail_after=point;
        try{
            reinterpret_cast<void(*)(void*,void*,void*,void*,unsigned)>(image+0x2974cf0)
                (body,queue.queues,nullptr,nullptr,0);
            fail_after=-1;constructed=true;
            reinterpret_cast<void(*)(void*)>(image+0x27c2ac0)(body);
        }catch(const std::bad_alloc&){fail_after=-1;++failures;}
        assert(live_blocks==before&&native_blocks==native_before);
        if(constructed)break;
    }
    assert(constructed&&failures>=4);
    std::printf("PASS stock clip constructor failures: %u allocation points, no leaks\n",failures);
    std::puts("PASS stock ClipAudioThreadCache constructor/destructor; no playback binding claimed");
    std::puts("PASS stock clip cache queue: actual source constructor, manager, swap and retirement; consumer allocation/delete forbidden");
    return 0;
}
