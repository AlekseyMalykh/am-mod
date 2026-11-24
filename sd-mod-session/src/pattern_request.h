#ifndef AM_PATTERN_REQUEST_H
#define AM_PATTERN_REQUEST_H
#include "pattern_native.h"

namespace am_pattern {
class ReadCurrency {
public:
    virtual ~ReadCurrency(){}
    /* Value-only check, no native dereferences. Safe after context teardown. */
    virtual bool current()const noexcept=0;
};
/* Owned across enqueue/cancellation; no borrowed callback user pointer.
 * enter runs ONLY inside the retained native context's worker callback.
 * It must acquire the Project/Program/event-list owner lease described by
 * NativeReadAccess. This interface does not manufacture that lease from a
 * context counter. Destruction may occur after native context teardown (a
 * purged closure is reclaimed later): never dereference borrowed native
 * pointers in this object's destructor. Production binding is not wired yet. */
class NativeReadOwner {
public:
    virtual ~NativeReadOwner(){}
    virtual bool enter(void*native_context,NativeReadContext&)=0;
    virtual bool valid(void*native_context,const NativeReadContext&)=0;
    virtual void leave(void*native_context,const NativeReadContext&)noexcept=0;
    /* Required for enqueue_snapshot; null is refused. Must use value metadata
     * only and remain valid after this owner/native context is destroyed. */
    virtual std::shared_ptr<const ReadCurrency> currency()const{return {};}
};
enum class RequestPhase {empty,queued,running,complete,cancelled};
enum class SubmitResult {accepted,invalid,unsupported,busy,unavailable,allocation_failed,exception};
struct RequestStatus {RequestPhase phase=RequestPhase::empty;NativeReadResult result=NativeReadResult::unavailable;};
struct RequestState;
class SnapshotRequest {
public:
    RequestStatus status()const noexcept;
    void cancel()noexcept;
    /* Consumes a successful immutable result once. False preserves out. */
    bool take(std::shared_ptr<const Snapshot>&out)noexcept;
private:
    friend SubmitResult enqueue_snapshot(std::uintptr_t,void*,const NativeReadRequest&,
        std::shared_ptr<NativeReadOwner>,SnapshotRequest&)noexcept;
    std::shared_ptr<RequestState> state_;
};
/* UI/control owner must keep context valid DURING submission. Thereafter the
 * pinned Copy context+50 counter and native destruction protocol take over.
 * Only Copy-context vptr 6d8cd88 is accepted (its destructor is verified).
 * No waiting/inline execution on full queue: preflight under native producer
 * mutex rejects busy slots. No new thread, UI completion callback or Undo.
 * Failure preserves the previous handle; one pending operation per handle.
 * Queued cancellation is terminal for the handle, but native counter release
 * still belongs to execution or the native context destructor's tag purge. */
SubmitResult enqueue_snapshot(std::uintptr_t,void*,const NativeReadRequest&,
    std::shared_ptr<NativeReadOwner>,SnapshotRequest&)noexcept;
}
#endif
