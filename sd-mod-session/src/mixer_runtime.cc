#include "mixer_runtime.h"
#include "mixer_session.h"
#include "mixer_archive.h"

namespace {
mixer_session *current = nullptr;
mixer_session *retiring = nullptr;
mixer_native_binding source{};
mixer_native_binding rejected{};
uint64_t archive_generation = 0;
bool stopped = false;

bool same_source(const mixer_native_binding &a,const mixer_native_binding &b)
{
    return a.epoch == b.epoch && a.engine == b.engine && a.program == b.program &&
        a.rate == b.rate && a.capacity == b.capacity;
}

void retire()
{
    if (!current) return;
    mixer_phone_bind_ui(nullptr);
    if (!mixer_live_native_attach(nullptr)) return;
    retiring = current;
    current = nullptr;
}

bool collect()
{
    if (!retiring) return true;
    mixer_live *released = mixer_live_native_take_retired();
    if (!released) return false;
    /* This runtime is the sole attachment owner in the automatic candidate. */
    if (released != mixer_session_live(retiring)) return false;
    mixer_session_destroy(retiring);
    retiring = nullptr;
    return true;
}
}

extern "C" void mixer_runtime_ui_stop()
{
    stopped = true;
    mixer_live_native_invalidate();
    retire();
    collect();
}

extern "C" void mixer_runtime_ui_tick()
{
    if (!mixer_live_native_ops()) return;
    uint64_t generation = mixer_archive_generation();
    if (archive_generation != generation) {
        archive_generation = generation;
        mixer_live_native_invalidate();
        retire();
        rejected = {};
    }
    if (!collect() || stopped || mixer_archive_loading()) return;

    mixer_native_binding observed{};
    int observed_status = mixer_live_native_binding(&observed);
    if (observed_status < 0) {
        if (current) mixer_session_owner_tick(current);
        return;
    }
    bool available = observed_status == 1;
    if (current && (!available || !same_source(source,observed) ||
                    mixer_session_status(current) < 0)) {
        retire();
        if (!collect()) return;
    }
    if (current) {
        mixer_session_owner_tick(current);
        return;
    }
    if (!available || same_source(rejected,observed)) return;

    /* All prepared storage belongs to AM. Native pointers remain identities
     * and are dereferenced only inside a matching native audio callback. */
    mixer_session *prepared = mixer_session_create(observed.epoch,0,observed.rate,
        observed.capacity,observed.engine,observed.program,mixer_live_native_ops(),nullptr);
    if (!prepared) {
        rejected = observed;
        return;
    }
    if (mixer_archive_loading() || mixer_archive_generation() != generation ||
        !mixer_live_native_attach_binding(mixer_session_live(prepared),&observed)) {
        mixer_session_destroy(prepared);
        return;
    }
    current = prepared;
    source = observed;
    if (!mixer_phone_bind_ui(current)) {
        retire();
        collect();
    }
}
