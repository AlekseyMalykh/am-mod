#ifndef AM_PATTERN_NATIVE_H
#define AM_PATTERN_NATIVE_H
#include "pattern_model.h"
#include <array>

namespace am_pattern {
enum class NativeReadResult {ok,invalid,unsupported,limit,stale,unavailable,allocation_failed,exception};
struct NativeReadRequest {Id snapshot_id=0;std::string sequence_key;int sequence_index=-1;};
struct NativeReadContext {
    std::uintptr_t image=0;void *project=nullptr,*things=nullptr;
    std::uint64_t generation=0,routing_revision=0;
    bool read_safe=false;
    /* Value snapshot acquired on the routing owner. Native mixer encoding:
     * -1 unknown, 0 Master Direct, 1..8 A..H, indexed by physical pad. */
    std::array<int,128> outputs;
    NativeReadContext(){outputs.fill(-1);}
};
struct NativeReadAccess {
    void *user=nullptr;
    /* enter must acquire a real owner/lifetime lease, resolve current handles
     * on that owner, and exclude mutation/destruction for the entire call.
     * If false or throwing, enter must clean up its own partial acquisition.
     * A UI pointer, generation counter or double scan alone is NOT a lease. */
    bool (*enter)(void*,NativeReadContext&)=nullptr;
    /* Validate project/load + routing epochs; never release the lease here. */
    bool (*valid)(void*,const NativeReadContext&)=nullptr;
    /* Exactly once after successful enter, including errors. Must not throw. */
    void (*leave)(void*,const NativeReadContext&)=nullptr;
};
/* Pinned firmware adapter, synchronous owner/control boundary only. Not yet
 * connected to a production executor. No native pointer is published.
 * Failure preserves out. No native command, list creation or event mutation.
 * Caller supplies stable sequence identity; index is a lookup hint, not ID. */
NativeReadResult read_native(const NativeReadRequest&,const NativeReadAccess&,
                            std::shared_ptr<const Snapshot>&out)noexcept;
}
#endif
