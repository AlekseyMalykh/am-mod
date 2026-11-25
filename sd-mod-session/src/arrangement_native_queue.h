#ifndef AM_ARRANGEMENT_NATIVE_QUEUE_H
#define AM_ARRANGEMENT_NATIVE_QUEUE_H
#include "arrangement_native_material.h"
namespace am_pattern {
enum class NativeQueueResult {ok,busy,unavailable,invalid,unsupported,stale,allocation_failed,native_failure};
struct NativeCacheBinding {
    std::uintptr_t image=0;void*cache=nullptr,*queues=nullptr;
    NativeDestination destination;
};
/* Actual control owner only. enter must serialize cache/queue teardown and
 * destination changes and keep native objects alive through leave. An epoch
 * comparison, GUI pointer or snapshot alone is NOT such a lease. No provider
 * is installed in production until native lifecycle coverage is confirmed.
 * enter/leave must not retain Store/render locks: reclaim destroys old trees.
 * destination_revision must change with cache/Track/Program identity.
 * This contract covers this publication, not queued transport or Track voices. */
class NativeCacheOwner {
public:
    virtual ~NativeCacheOwner(){}
    virtual bool enter(NativeCacheBinding&)=0;
    virtual void leave()noexcept=0;
};
/* Reuses stock cache copy and existing ThreadQueues producer/runner protocol.
 * Under the native recursive producer mutex, reclaim completed jobs and admit
 * only an immediately available slot. Never enter stock full-queue wait/inline
 * fallback. Consumer alone may free slots while the mutex is held.
 * Caller checks Store currency before submission. Native cache weak self
 * guards the queued callback; this function grants no Project/Track lease and
 * does not Start/Stop transport. Copy allocates on control, never call in audio.
 * Copy origin indexing requires a separately serialized pre-consumer boundary;
 * a posted closure is already consumer-visible when this function returns. */
/* ok means accepted by the queue, not completed, audible or safe to Start.
 * A transport owner must order/acknowledge application and revalidate binding. */
NativeQueueResult publish_native_cache(NativeCacheOwner&,const NativePlaybackMaterial&)noexcept;
/* Clear only the captured destination identity, even if transport flags have
 * since changed. A late Stop must never clear a newly resolved Project/cache.
 * expected is the session's retained value, not a fresh current snapshot. */
NativeQueueResult clear_native_cache(NativeCacheOwner&,const NativeDestination&expected)noexcept;
}
#endif
