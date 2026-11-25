#ifndef AM_ARRANGEMENT_NATIVE_QUEUE_H
#define AM_ARRANGEMENT_NATIVE_QUEUE_H
#include "arrangement_native_material.h"
namespace am_pattern {
enum class NativeQueueResult {ok,busy,unavailable,invalid,unsupported,stale,allocation_failed,native_failure};
enum class NativeQueuePhase {empty,pending,running,applied,cancelled};
struct NativeQueueState;
/* Control-owned receipt. applied confirms an actual stock tree swap, not
 * just a queue barrier. It is historical completion, NOT a live destination
 * lease or permission to Start: revalidate Project/Track/Program on its owner.
 * cancel denies future Start; it cannot undo a swap already running/applied.
 * Retain metadata through voice retirement; release receipts on control only. */
class NativeQueueReceipt {
public:
    NativeQueuePhase phase()const noexcept;
    void cancel()noexcept;
    const NativeDestination*destination()const noexcept;
    /* Immutable index prepared before consumer visibility. No tree lease;
     * use only for this session/generation, including its retained tails. */
    const NativePlaybackOriginIndex*origins()const noexcept;
private:
    friend struct NativeQueueAccess;
    std::shared_ptr<NativeQueueState>state_;
};
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
 * Binding identity is immutable for the native cache guard's lifetime: retire
 * the old cache before replacing its destination; never repurpose a live cache
 * for another identity. Otherwise an already queued callback could target it.
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
/* Confirmed path: prebuild a separately owned stock Holder and origin index,
 * then queue one closure that runs the SAME stock swap and acknowledges it.
 * A full queue/error preserves out and the prior source. No global hooks.
 * All native/C++ preparation and eventual destruction happen on control. */
NativeQueueResult publish_native_cache(NativeCacheOwner&,const NativePlaybackMaterial&,NativeQueueReceipt&out)noexcept;
NativeQueueResult clear_native_cache(NativeCacheOwner&,const NativeDestination&expected,NativeQueueReceipt&out)noexcept;
}
#endif
