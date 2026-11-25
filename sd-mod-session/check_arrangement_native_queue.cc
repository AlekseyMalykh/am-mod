/* Real stock queue post/runner/reclaim and cache copy/swap. Ring storage,
 * cache wrapper, lifecycle owner and destination metadata remain fixtures. */
#define check_arrangement_material check_material_before_queue
#include "check_arrangement_native_material.cc"
#undef check_arrangement_material
#include "src/arrangement_native_queue.h"
#include <pthread.h>
#include <mutex>

namespace {
std::uintptr_t queue_image;
bool running_queue=false;
const NativeQueueReceipt*paused_receipt=nullptr;
std::atomic<unsigned>swap_paused{0},swap_continue{0};
void pause_after_swap(void*cache){
    assert(paused_receipt&&paused_receipt->phase()==NativeQueuePhase::running);
    assert(field<std::uint64_t>(static_cast<unsigned char*>(cache)+0x98,0x88)==0);
    swap_paused.store(1,std::memory_order_release);
    while(!swap_continue.load(std::memory_order_acquire))std::this_thread::yield();
}
void forbid_native_allocate(){assert(!running_queue);}
void forbid_native_delete(){assert(!running_queue);}
template<class T>void put(void*p,unsigned offset,T value){std::memcpy(static_cast<char*>(p)+offset,&value,sizeof value);}
struct QueueOwner : am_pattern::NativeCacheOwner {
    alignas(8) unsigned char cache[0x248]{},guard[0x160]{},queues[0x260]{},slots[8][0xc0]{};
    NativeDestination destination;
    std::mutex lifecycle;bool available=true,live=true,entered=false;
    unsigned enters=0,leaves=0;std::uint64_t reader=0;
    QueueOwner(){
        auto ctor=reinterpret_cast<void(*)(void*)>(queue_image+0x170a2f0);ctor(cache+0x98);ctor(guard);
        put(cache,0,queue_image+0x6d906c8);put(cache,8,static_cast<void*>(queues+0x1d8));
        put(cache,0x90,static_cast<void*>(queues));put(cache,0x28,static_cast<void*>(cache));
        auto*control=field<unsigned char*>(guard,0x150);
        __atomic_fetch_add(reinterpret_cast<unsigned*>(control+12),1u,__ATOMIC_ACQ_REL);put(cache,0x30,control);
        put(queues+0x1d8,0,std::uint64_t(8));put(queues+0x1d8,0x30,static_cast<void*>(slots));
        for(auto&s:slots){put(s,0,queue_image+0xb57a7c);put(s,8,queue_image+0x15265b0);}
        reinterpret_cast<void(*)(void*)>(queue_image+0xf406b0)(queues+0x220);
    }
    ~QueueOwner(){
        assert(!entered&&field<std::uint64_t>(queues+0x1d8,0x20)==reader);reclaim();
        if(live)expire();
        reinterpret_cast<void(*)(void*)>(queue_image+0x13bb720)(field<void*>(cache,0x30));
        reinterpret_cast<void(*)(void*)>(queue_image+0x14c4a20)(cache+0x98);
        assert(!pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t*>(queues+0x220)));
    }
    bool enter(NativeCacheBinding&out)override{
        lifecycle.lock();if(!available){lifecycle.unlock();return false;}
        try{out.image=queue_image;out.cache=cache;out.queues=queues;out.destination=destination;}
        catch(...){lifecycle.unlock();throw;}
        assert(!entered);entered=true;++enters;return true;
    }
    void leave()noexcept override{assert(entered);entered=false;++leaves;lifecycle.unlock();}
    void expire(){assert(live&&!entered);available=false;reinterpret_cast<void(*)(void*)>(queue_image+0x14c4a20)(guard);live=false;}
    void replace_expired_guard(){
        assert(!live&&!entered);auto*old=field<void*>(cache,0x30);
        reinterpret_cast<void(*)(void*)>(queue_image+0x170a2f0)(guard);
        auto*control=field<unsigned char*>(guard,0x150);assert(control!=old);
        __atomic_fetch_add(reinterpret_cast<unsigned*>(control+12),1u,__ATOMIC_ACQ_REL);put(cache,0x30,control);
        reinterpret_cast<void(*)(void*)>(queue_image+0x13bb720)(old);
        ++destination.project_epoch;++destination.destination_revision;live=available=true;
    }
    void reclaim(){reinterpret_cast<void(*)(void*)>(queue_image+0x2b58c70)(queues+0x1d8);}
    std::uint64_t count()const{return field<std::uint64_t>(cache+0x98,0x88);}
    unsigned char*next_slot(){auto write=field<std::uint64_t>(queues+0x1d8,0x20);assert(write<8);return slots[write];}
    void run_one(){
        assert(reader!=field<std::uint64_t>(queues+0x1d8,0x20));
        // A distinct consumer thread executes the REAL native runner. It is
        // not the application's driver thread or its real frame clock.
        std::thread worker([&]{
            running_queue=true;auto all=live_blocks.load(),native=native_blocks.load(),bytes=native_bytes.load();
            native_alloc_observer=forbid_native_allocate;native_delete_observer=forbid_native_delete;fail_after=0;
            reinterpret_cast<void(*)(void*,std::uint64_t*)>(queue_image+0x2b59200)(queues+0x1d8,&reader);
            fail_after=-1;native_alloc_observer=nullptr;native_delete_observer=nullptr;
            assert(live_blocks==all&&native_blocks==native&&native_bytes==bytes);running_queue=false;
        });worker.join();
    }
};
}
extern "C" int check_arrangement_material(std::uintptr_t image){
    assert(!check_material_before_queue(image));queue_image=image;
    auto all=live_blocks.load(),native=native_blocks.load();
    {
        QueueOwner owner;NativeArrangementPlan plan;plan.arrangement_revision=plan.library_revision=1;
        auto&d=plan.destination;d.project_epoch=d.routing_revision=d.destination_revision=1;
        d.program_key="queue-proof";d.sequence_ticks=61440;d.transport_valid=true;
        for(unsigned i=0;i<128;++i)d.pad_to_note[i]=d.note_to_pad[i]=int(i);
        auto make=reinterpret_cast<step_event(*)(unsigned,std::int64_t,std::int64_t,unsigned,unsigned,float)>(image+0x27e54d0);
        for(unsigned i=0;i<7;++i){NativeBoundNote n;n.track=i+1;n.clip=i+8;n.pattern=21;n.event=i+1;n.pad=n.source_note=50;
            n.native=make(50,i<4?0:std::int64_t(i-3)*7680,40000,100,2,.8f);plan.notes.push_back(n);}
        std::shared_ptr<const NativePlaybackMaterial>source;
        assert(NativePlaybackMaterial::prepare(image,plan,source)==NativeMaterialResult::ok);owner.destination=d;const auto expected_destination=d;
        ++owner.destination.project_epoch;
        assert(publish_native_cache(owner,*source)==NativeQueueResult::stale&&!owner.count());
        owner.destination=d;++owner.destination.destination_revision;
        assert(publish_native_cache(owner,*source)==NativeQueueResult::stale&&!owner.count());
        owner.destination=d;owner.destination.pad_to_note[50]=49;
        assert(publish_native_cache(owner,*source)==NativeQueueResult::stale&&!owner.count());owner.destination=d;
        owner.destination.playing=true;assert(publish_native_cache(owner,*source)==NativeQueueResult::stale);owner.destination=d;
        ++owner.destination.project_epoch;
        assert(clear_native_cache(owner,expected_destination)==NativeQueueResult::stale);owner.destination=d;
        assert(clear_native_cache(owner,NativeDestination{})==NativeQueueResult::invalid);
        put(owner.cache,0,queue_image+0x6d90668);
        assert(publish_native_cache(owner,*source)==NativeQueueResult::unsupported);put(owner.cache,0,queue_image+0x6d906c8);
        put(owner.queues+0x1d8,0,std::uint64_t(1));assert(clear_native_cache(owner,expected_destination)==NativeQueueResult::invalid);
        put(owner.queues+0x1d8,0,std::uint64_t(8));
        // Failure and saturation must not enter stock inline execution/wait.
        auto before=live_blocks.load();unsigned failures=0;bool posted=false;
        auto*slot=owner.next_slot();
        for(long point=0;point<64;++point){
            fail_after=point;auto result=publish_native_cache(owner,*source);fail_after=-1;
            if(result==NativeQueueResult::ok){posted=true;break;}
            assert(result==NativeQueueResult::allocation_failed&&owner.count()==0&&live_blocks==before);++failures;
        }
        assert(posted&&failures>=8&&owner.count()==0&&(field<std::uintptr_t>(slot,0xb0)&3u)==1);
        auto*copy=field<const void*>(slot,16);assert(copy&&copy!=source->events());
        // No consumer is running yet: this is an explicit serialized index
        // preparation boundary, not permission to peek a live queue in app.
        NativePlaybackOriginIndex origins;assert(NativePlaybackOriginIndex::prepare(*source,copy,origins)==NativeMaterialResult::ok);
        source.reset();plan=NativeArrangementPlan{};
        owner.run_one();assert(owner.count()==7);owner.reclaim();assert(owner.count()==7);
        const void*node=field<const void*>(owner.cache+0x98,0x78);
        for(unsigned i=0;i<7;++i){auto*event=static_cast<const unsigned char*>(node)+0x20;assert(origins.origin(event));
            node=reinterpret_cast<const void*(*)(const void*)>(image+0xb51738)(node);}
        assert(reinterpret_cast<std::int64_t(*)(const void*)>(image+0x27eb1d0)(owner.cache+0x98)==63040);
        // Fill a real ring: the eighth publication must return Busy promptly,
        // even if stock's full-ring policy would run callbacks inline.
        owner.queues[0x1d8+0x2c]=1;
        for(unsigned i=0;i<7;++i)assert(clear_native_cache(owner,expected_destination)==NativeQueueResult::ok);
        before=live_blocks.load();assert(clear_native_cache(owner,expected_destination)==NativeQueueResult::busy);
        assert(owner.count()==7&&live_blocks==before);
        for(unsigned i=0;i<7;++i){owner.run_one();}
        assert(!owner.count());owner.reclaim();origins=NativePlaybackOriginIndex{};
        // Repeat through wrap-around and native reclamation, not a mock FIFO.
        for(unsigned i=0;i<24;++i){assert(clear_native_cache(owner,expected_destination)==NativeQueueResult::ok);owner.run_one();owner.reclaim();}
        owner.destination.playing=true;owner.destination.recording=true;
        assert(clear_native_cache(owner,expected_destination)==NativeQueueResult::ok);owner.run_one();owner.reclaim();owner.destination=expected_destination;
        owner.queues[0x1d8+0x2d]=1;assert(clear_native_cache(owner,expected_destination)==NativeQueueResult::unavailable);owner.queues[0x1d8+0x2d]=0;
        // Expire the real native guard before consuming a queued clear.
        step_event one=make(50,0,9000,100,2,.8f);reinterpret_cast<void*(*)(void*,const void*)>(image+0x27ee5d0)(owner.cache+0x98,&one);
        assert(clear_native_cache(owner,expected_destination)==NativeQueueResult::ok);owner.expire();
        owner.run_one();assert(owner.count()==1);owner.reclaim();
        owner.available=true;assert(clear_native_cache(owner,expected_destination)==NativeQueueResult::unavailable);owner.available=false;
        assert(clear_native_cache(owner,expected_destination)==NativeQueueResult::unavailable&&owner.enters==owner.leaves);
    }
    {
        QueueOwner owner;NativeArrangementPlan plan;plan.arrangement_revision=plan.library_revision=1;
        auto&d=plan.destination;d.project_epoch=d.routing_revision=d.destination_revision=1;
        d.program_key="ack-proof";d.sequence_ticks=61440;d.transport_valid=true;
        for(unsigned i=0;i<128;++i)d.pad_to_note[i]=d.note_to_pad[i]=int(i);
        auto make=reinterpret_cast<step_event(*)(unsigned,std::int64_t,std::int64_t,unsigned,unsigned,float)>(image+0x27e54d0);
        for(unsigned i=0;i<7;++i){NativeBoundNote n;n.track=i+1;n.clip=i+8;n.pattern=21;n.event=i+1;n.pad=n.source_note=50;
            n.native=make(50,i<4?0:std::int64_t(i-3)*7680,40000,100,2,.8f);plan.notes.push_back(n);}
        std::shared_ptr<const NativePlaybackMaterial>source;
        assert(NativePlaybackMaterial::prepare(image,plan,source)==NativeMaterialResult::ok);
        owner.destination=d;auto expected=d;NativeQueueReceipt receipt;
        assert(receipt.phase()==NativeQueuePhase::empty&&!receipt.destination()&&!receipt.origins());
        assert(clear_native_cache(owner,expected,receipt)==NativeQueueResult::ok&&receipt.phase()==NativeQueuePhase::pending);
        owner.run_one();assert(receipt.phase()==NativeQueuePhase::applied);owner.reclaim();
        auto*old_destination=receipt.destination();auto before=live_blocks.load(),native_before=native_blocks.load();
        unsigned failures=0;bool accepted=false;
        for(long point=0;point<128;++point){
            fail_after=point;auto result=publish_native_cache(owner,*source,receipt);fail_after=-1;
            if(result==NativeQueueResult::ok){accepted=true;break;}
            assert(result==NativeQueueResult::allocation_failed&&receipt.phase()==NativeQueuePhase::applied
                &&receipt.destination()==old_destination&&!owner.count()&&live_blocks==before&&native_blocks==native_before);++failures;
        }
        assert(accepted&&failures>=12&&receipt.phase()==NativeQueuePhase::pending&&receipt.origins()->size()==7);
        assert(clear_native_cache(owner,expected,receipt)==NativeQueueResult::busy);
        source.reset();plan=NativeArrangementPlan{};
        owner.run_one();assert(receipt.phase()==NativeQueuePhase::applied&&owner.count()==7);
        owner.reclaim();assert(receipt.origins()->size()==7);
        unsigned seen=0;const void*node=field<const void*>(owner.cache+0x98,0x78);
        for(unsigned i=0;i<7;++i){auto*origin=receipt.origins()->origin(static_cast<const unsigned char*>(node)+0x20);
            assert(origin&&origin->track==origin->event&&origin->event>=1&&origin->event<=7);
            seen|=1u<<(origin->event-1);node=reinterpret_cast<const void*(*)(const void*)>(image+0xb51738)(node);}
        assert(seen==127&&reinterpret_cast<std::int64_t(*)(const void*)>(image+0x27eb1d0)(owner.cache+0x98)==63040);
        // Cancelling a receipt after application does not pretend to undo it.
        receipt.cancel();assert(receipt.phase()==NativeQueuePhase::cancelled&&owner.count()==7&&receipt.origins()->size()==7);
        NativeQueueReceipt cancelled;
        assert(clear_native_cache(owner,expected,cancelled)==NativeQueueResult::ok);cancelled.cancel();
        owner.run_one();assert(cancelled.phase()==NativeQueuePhase::cancelled&&owner.count()==7);owner.reclaim();
        // A test-only change receiver pauses the REAL swap after tree mutation
        // but before it returns. Receipt must stay Running, and cancellation
        // here must win over the later Applied CAS without pretending undo.
        NativeQueueReceipt during;assert(clear_native_cache(owner,expected,during)==NativeQueueResult::ok);
        auto*notification=reinterpret_cast<void*>(image+0x6d906c8+24);
        auto old_notification=field<std::uintptr_t>(notification,0);
        put(notification,0,reinterpret_cast<std::uintptr_t>(pause_after_swap));paused_receipt=&during;
        std::thread consumer([&]{owner.run_one();});
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(!swap_paused.load(std::memory_order_acquire)){
            assert(std::chrono::steady_clock::now()<deadline);std::this_thread::yield();
        }
        assert(during.phase()==NativeQueuePhase::running);during.cancel();swap_continue.store(1,std::memory_order_release);
        consumer.join();put(notification,0,old_notification);paused_receipt=nullptr;
        assert(during.phase()==NativeQueuePhase::cancelled&&!owner.count());owner.reclaim();
        // Give the dropped receipt an observable nonempty source to clear.
        step_event restored=make(50,0,9000,100,2,.8f);
        reinterpret_cast<void*(*)(void*,const void*)>(image+0x27ee5d0)(owner.cache+0x98,&restored);
        // The queue owns job/state even if the UI discards its last receipt.
        {NativeQueueReceipt dropped;assert(clear_native_cache(owner,expected,dropped)==NativeQueueResult::ok);}
        owner.run_one();assert(!owner.count());owner.reclaim();receipt=NativeQueueReceipt{};
        // A real native slot purge must never produce a false Applied receipt.
        step_event one=make(50,0,9000,100,2,.8f);reinterpret_cast<void*(*)(void*,const void*)>(image+0x27ee5d0)(owner.cache+0x98,&one);
        NativeQueueReceipt purged;auto*slot=owner.next_slot();
        assert(clear_native_cache(owner,expected,purged)==NativeQueueResult::ok);
        assert(reinterpret_cast<unsigned(*)(void*,std::uintptr_t)>(image+0x2b59000)(slot,0)==1);
        owner.run_one();assert(owner.count()==1&&purged.phase()==NativeQueuePhase::pending);
        owner.reclaim();assert(purged.phase()==NativeQueuePhase::cancelled);
        // Full-ring refusal preserves an existing receipt; no inline execution.
        owner.queues[0x1d8+0x2c]=1;
        for(unsigned i=0;i<7;++i){assert(clear_native_cache(owner,expected)==NativeQueueResult::ok);}
        assert(clear_native_cache(owner,expected,purged)==NativeQueueResult::busy&&purged.phase()==NativeQueuePhase::cancelled&&owner.count()==1);
        for(unsigned i=0;i<7;++i){owner.run_one();}owner.reclaim();
        reinterpret_cast<void*(*)(void*,const void*)>(image+0x27ee5d0)(owner.cache+0x98,&one);
        NativeQueueReceipt expired;
        assert(clear_native_cache(owner,expected,expired)==NativeQueueResult::ok);owner.expire();
        // Reuse the exact wrapper address for a new guarded identity before
        // the old closure runs. The old control block cannot be resurrected.
        owner.replace_expired_guard();owner.run_one();
        assert(expired.phase()==NativeQueuePhase::cancelled&&owner.count()==1);owner.reclaim();
        assert(clear_native_cache(owner,expected,expired)==NativeQueueResult::stale);
        NativeQueueReceipt fresh;assert(clear_native_cache(owner,owner.destination,fresh)==NativeQueueResult::ok);
        owner.run_one();assert(fresh.phase()==NativeQueuePhase::applied&&!owner.count());owner.reclaim();
        assert(owner.enters==owner.leaves);
        printf("PASS confirmed native swap: prebuilt origins before queue visibility, actual swap before Applied, %u allocation failures preserve receipt/cache, cancel/dropped receipt/native purge/full ring/expired guard; job/state/tree destruction outside consumer; no transport Start or destination lease\n",failures);
    }
    assert(live_blocks==all&&native_blocks==native);
    puts("PASS native Arrange queue: unpatched stock producer/runner/reclaim with real recursive mutex, distinct consumer thread, copied events/retained origins, tails, allocation failure, full-ring no-inline refusal, wrap-around, stopped queue and expired guard; cache storage/owner/destination are fixtures; no driver transport or Track/Program lease");
    return 0;
}
