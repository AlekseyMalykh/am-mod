#ifndef MPC_MOD_FX_NATIVE_H
#define MPC_MOD_FX_NATIVE_H
#include "fx_chain.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Borrowed formats belong to the native PluginManager. Their owner and
 * preset services must outlive every prepared program. Control queue only. */
struct fx_native_context {
    uintptr_t base;
    void *air_format,*mpc_format;
    double bpm;
    unsigned numerator,denominator;
    /* Optional live clock: copied native PositionInfo plus a distinct audio
     * block identity. Called on audio only. NULL preserves offline transport. */
    void *position_user;
    int (*position)(void*,void *position88,uint64_t *block);
    unsigned char initial_position[88];
};
int fx_native_context_from_manager(uintptr_t,void*,struct fx_native_context*);
void fx_native_registry_bind(void*);
int fx_native_context_capture(uintptr_t,struct fx_native_context*);
/* Control owner: borrow only application-owned formats, without registry
 * insertion or reading the playhead. Caller supplies a proven clock snapshot. */
int fx_native_application_formats(uintptr_t,struct fx_native_context*);
void fx_native_backend(struct fx_native_context*,struct fx_backend*);
#ifdef __cplusplus
}
#endif
#endif
