#ifndef MPC_RESAMPLE_STOCK_H
#define MPC_RESAMPLE_STOCK_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Process RAM only. Pinned1.3.0.12, call once on the control owner before use.
 * The module remains loaded for the lifetime of the cloned application. */
int resample_stock_install(uintptr_t base);
/* Last serialized Sample export failure; a static string, never UI-owned. */
const char *resample_stock_error(void);
/* Native control/model owner only, AFTER destination confirmation and epoch /
 * occupancy validation. `mode` is the complete ResamplePadMode, NOT its +48
 * PadsBehaviour base. No native pointer may be obtained on the audio/UI input
 * producer. Caller preserves project/services through stock task completion.
 * Return1 means stock EXPORT START was called, NOT assignment/audio success.
 * mode+150 belongs to the async stock result callback; caller MUST serialize
 * jobs until it finishes. No independent PCM Sample or assignment is made. */
int resample_stock_start(uintptr_t base,void *mode,int single_pad,unsigned source,unsigned destination);
/* Completion is delivered after the native callback returned, or on final
 * release if it was never invoked. Retained callback copies do not delay it.
 * invoked means the native file/import callback returned; caller
 * must still verify the destination on its model owner before reporting
 * success. The observer only publishes numeric state; no UI/model calls. */
int resample_stock_start_observed(uintptr_t base,void *mode,int single_pad,unsigned source,unsigned destination,
    void (*completed)(void*,int invoked),void *user);
/* Source name is copied synchronously, before native filename allocation. */
int resample_stock_start_named_observed(uintptr_t base,void *mode,int single_pad,unsigned source,unsigned destination,
    const char *source_name,void (*completed)(void*,int invoked),void *user);
#ifdef __cplusplus
}
#endif
#endif
