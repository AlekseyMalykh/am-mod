#ifndef AM_PATTERN_SOURCE_H
#define AM_PATTERN_SOURCE_H
#include "pattern_request.h"

namespace am_pattern {
struct SourceBinding {
    std::uintptr_t image=0;void*project=nullptr,*things=nullptr,*program=nullptr;
    std::uint64_t routing_revision=0;
    std::array<int,128> outputs;
    SourceBinding(){outputs.fill(-1);}
};
struct SourceState;
/* All invalidation/destruction/mutation of bound native objects MUST enter
 * change() before touching them. A counter/epoch without participating writers
 * is not memory protection. Production writer coverage is not enabled yet. */
class SourceDomain {
public:
    class Change {
    public:
        Change()=default;Change(Change&&)noexcept;Change&operator=(Change&&)noexcept;
        ~Change();Change(const Change&)=delete;Change&operator=(const Change&)=delete;
    private:
        friend class SourceDomain;std::shared_ptr<SourceState> state_;
        explicit Change(std::shared_ptr<SourceState>);
        void finish()noexcept;
    };
    SourceDomain();~SourceDomain();
    SourceDomain(const SourceDomain&)=delete;SourceDomain&operator=(const SourceDomain&)=delete;
    /* Closes admission, invalidates completed results, waits for active readers
     * WITHOUT holding a native/render lock. No mutex held across caller work.
     * Must be outside native locks the reader needs; never on the audio thread.
     * Nested/asynchronous changes keep admission closed. */
    Change change();
    std::uint64_t epoch()const noexcept;
    /* Explicit publication AFTER the actual mutation/async completion, on its
     * owner. End of Change alone never reopens admission (including failure).
     * Require captured epoch; a late old publisher cannot reopen a new project. */
    bool publish(const SourceBinding&,std::uint64_t expected_epoch);
    /* Captures the current epoch only; queued owner retains no native pointers. */
    std::shared_ptr<NativeReadOwner> owner()const;
    /* Permanently closes admission and drains readers before native teardown. */
    void retire()noexcept;
private:
    std::shared_ptr<SourceState> state_;
};
/* Owner-side boundaries, not GUI entrypoints. They never reopen admission on
 * return: async native work may continue after the initiating call returns.
 * A completion on the actual owner must explicitly publish the new binding. */
void source_load(SourceDomain&,void*,void*,void(*original)(void*,void*));
int source_read(SourceDomain&,void*,void*,int(*original)(void*,void*));
void source_new(SourceDomain&,void*,int*,void(*original)(void*,int*));
void source_program(SourceDomain&,void*,void*,int,void(*original)(void*,void*,int));
}
#endif
