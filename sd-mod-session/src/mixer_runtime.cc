#include "mixer_runtime.h"
#include "mixer_session.h"
#include "mixer_archive.h"
#include "mixer_sources.h"
#include "mixer_fx.h"
#include <cstdio>
#include <cstring>
#include <ctime>

namespace {
/* Lab diagnostic: one stderr line per state change of automatic binding. */
const char *last_state = "";
unsigned diag_lines = 0;
void diag(const char *state)
{
    if (!std::strcmp(state,last_state) || diag_lines >= 80) return;
    last_state = state; ++diag_lines;
    std::fprintf(stderr,"MIXER_RUNTIME %s\n",state);
}
mixer_session *current = nullptr;
mixer_session *retiring = nullptr;
mixer_fx *current_fx=nullptr,*retiring_fx=nullptr,*preparing_fx=nullptr;
mixer_native_binding preparing_source{};
mixer_native_binding source{};
uint64_t prepare_after=0;
uint64_t archive_generation = 0;
bool stopped = false;
uint64_t presence_poll_ms = 0, presence_seen_ms = 0;
uint64_t occupied[2]{};

uint64_t now_ms()
{
    timespec time{};
    clock_gettime(CLOCK_MONOTONIC,&time);
    return static_cast<uint64_t>(time.tv_sec)*1000+time.tv_nsec/1000000;
}

void refresh_presence()
{
    if (!current) return;
    mixer_control *control = mixer_session_control(current);
    if (!mixer_control_active(control)) {
        presence_poll_ms = presence_seen_ms = 0;
        return;
    }
    uint64_t now = now_ms();
    if (!presence_poll_ms || now-presence_poll_ms >= 250) {
        presence_poll_ms = now;
        mixer_sources_snapshot snapshot{};
        if (mixer_sources_poll(source.program,source.epoch,&snapshot)) {
            presence_seen_ms = snapshot.captured_ms;
            occupied[0] = snapshot.present[0];
            occupied[1] = snapshot.present[1];
        } else {
            presence_seen_ms = 0;
        }
    }
    mixer_view_state view;
    mixer_control_view(control,now,&view);
    bool fresh = presence_seen_ms && now >= presence_seen_ms && now-presence_seen_ms <= 750;
    mixer_control_presence(control,fresh?occupied[0]:0,fresh?occupied[1]:0,view.bank);
}

bool same_source(const mixer_native_binding &a,const mixer_native_binding &b)
{
    return a.epoch == b.epoch && a.engine == b.engine && a.program == b.program &&
        a.rate == b.rate && a.capacity == b.capacity;
}
int fx_healthy(void *context){return !mixer_fx_fault(static_cast<mixer_fx*>(context));}

void retire()
{
    if (!current) return;
    mixer_phone_bind_ui(nullptr);
    if (!mixer_live_native_attach(nullptr)) return;
    retiring = current;
    retiring_fx=current_fx;current_fx=nullptr;
    current = nullptr;
    presence_poll_ms = presence_seen_ms = 0;
    occupied[0] = occupied[1] = 0;
}

bool collect()
{
    if (!retiring) return true;
    mixer_live *released = mixer_live_native_take_retired();
    if (!released) return false;
    /* This runtime is the sole attachment owner in the automatic candidate. */
    if (released != mixer_session_live(retiring)) return false;
    mixer_session_destroy(retiring);
    mixer_fx_destroy(retiring_fx);retiring_fx=nullptr;
    retiring = nullptr;
    return true;
}
}

extern "C" void mixer_runtime_ui_stop()
{
    stopped = true;
    mixer_fx_destroy(preparing_fx);preparing_fx=nullptr;
    mixer_live_native_invalidate();
    retire();
    collect();
}

extern "C" void mixer_runtime_ui_tick()
{
    if (!mixer_live_native_ops()) { diag("NO_NATIVE_OPS"); return; }
    uint64_t generation = mixer_archive_generation();
    if (archive_generation != generation) {
        archive_generation = generation;
        mixer_live_native_invalidate();
        retire();
        prepare_after=0;
        mixer_fx_destroy(preparing_fx);preparing_fx=nullptr;
    }
    if (!collect() || stopped || mixer_archive_loading()) { diag(stopped?"STOPPED":mixer_archive_loading()?"ARCHIVE_LOADING":"COLLECTING"); return; }
    if(mixer_fx_fault(current_fx)){
        retire();if(!collect())return;
    }

    mixer_native_binding observed{};
    int observed_status = mixer_live_native_binding(&observed);
    if (observed_status < 0) {
        diag(current?"BOUND_BINDING_BUSY":"BINDING_BUSY");
        if (current) {
            mixer_session_owner_tick(current);
            refresh_presence();
        }
        return;
    }
    bool available = observed_status == 1;
    if (current && (!available || !same_source(source,observed) || mixer_fx_fault(current_fx) ||
                    mixer_session_status(current) < 0)) {
        retire();
        if (!collect()) return;
    }
    if (current) {
        diag("BOUND");
        mixer_fx_refresh(current_fx,&observed);
        mixer_session_owner_tick(current);
        refresh_presence();
        return;
    }
    if (!available) {diag("NO_AUDIO_BINDING_OBSERVED");mixer_fx_destroy(preparing_fx);preparing_fx=nullptr;return;}

    if(!preparing_fx||!same_source(preparing_source,observed)){
        mixer_fx_destroy(preparing_fx);
        preparing_fx=mixer_fx_create(mixer_live_native_image(),&observed);preparing_source=observed;
        prepare_after=0;
    }
    if(!preparing_fx){diag("FX_BRIDGE_CREATE_FAILED");return;}
    int was_ready=mixer_fx_ready(preparing_fx);mixer_fx_refresh(preparing_fx,&observed);
    if(!was_ready&&mixer_fx_ready(preparing_fx))prepare_after=0;
    if(now_ms()<prepare_after){diag(mixer_fx_ready(preparing_fx)?"WAIT_RETRY":"WAIT_FX_READY");return;}
    /* Engine/Program remain opaque. The FX bridge borrows independently
     * validated application formats; its clock is copied on native audio. */
    mixer_session *prepared = mixer_session_create(observed.epoch,0,observed.rate,
        observed.capacity,observed.engine,observed.program,mixer_live_native_ops(),mixer_fx_backend(preparing_fx));
    if (!prepared) {
        diag("SESSION_CREATE_FAILED");
        prepare_after=now_ms()+(mixer_fx_ready(preparing_fx)?1000:250);
        return;
    }
    mixer_session_health(prepared,fx_healthy,preparing_fx);
    if (mixer_archive_loading() || mixer_archive_generation() != generation ||
        !mixer_live_native_attach_binding(mixer_session_live(prepared),&observed)) {
        diag("ATTACH_FAILED");
        mixer_session_destroy(prepared);
        return;
    }
    current = prepared;
    current_fx=preparing_fx;preparing_fx=nullptr;
    source = observed;
    diag("SESSION_CREATED");
    if (!mixer_phone_bind_ui(current)) {
        diag("UI_BIND_FAILED");
        retire();
        collect();
    }
}
