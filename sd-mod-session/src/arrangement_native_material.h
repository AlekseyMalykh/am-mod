#ifndef AM_ARRANGEMENT_NATIVE_MATERIAL_H
#define AM_ARRANGEMENT_NATIVE_MATERIAL_H
#include "arrangement_native_plan.h"
#include <memory>
namespace am_pattern {
enum class NativeMaterialResult {ok,invalid,unsupported,allocation_failed,native_failure};
/* Owns a standalone stock Events object, never a Project/Track/Program/Clip.
 * Build/release on the control/native owner, never in an audio callback.
 * Retain this owner until every native iterator/user of events() is finished.
 * Shared ownership protects this material only; it is not a destination lease
 * and does not admit Start, change transport, attach a Clip or edit a Sequence.
 * Native readers/cache operations still require their normal serialization. */
class NativePlaybackMaterial {
public:
    ~NativePlaybackMaterial();
    NativePlaybackMaterial(const NativePlaybackMaterial&)=delete;
    NativePlaybackMaterial&operator=(const NativePlaybackMaterial&)=delete;
    const void*events()const noexcept{return storage_.data();}
    const NativeArrangementPlan&plan()const noexcept{return plan_;}
    /* Exact event address from this retained source, no pointer dereference
     * and no allocation. Distinguishes equal notes on different musical rows.
     * Returned metadata lives with this owner, never a native Track pointer. */
    const NativeBoundNote*origin(const void*native_event)const noexcept;
    static bool compatible(std::uintptr_t image)noexcept;
    /* Pinned, mapped image must remain alive through material destruction.
     * Revalidates bound inline notes before any stock allocation/mutation.
     * Failure preserves out; partial native trees are destroyed by stock code.
     * Tree ordering is the stock comparator, including simultaneous onsets. */
    static NativeMaterialResult prepare(std::uintptr_t image,const NativeArrangementPlan&,
        std::shared_ptr<const NativePlaybackMaterial>&out)noexcept;
private:
    NativePlaybackMaterial(std::uintptr_t image,const NativeArrangementPlan&plan):image_(image),plan_(plan){}
    std::uintptr_t image_;
    NativeArrangementPlan plan_;
    /* Stock deleting destructor 27ee0d0 passes exactly 0x148 to sized delete.
     * The non-deleting destructor owns all internal trees/listeners. */
    alignas(8) std::array<unsigned char,0x148>storage_{};
    struct Link {std::uintptr_t event;unsigned note;};
    std::vector<Link>links_;
    bool constructed_=false;
};
}
#endif
