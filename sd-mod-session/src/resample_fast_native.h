#ifndef MPC_RESAMPLE_FAST_NATIVE_H
#define MPC_RESAMPLE_FAST_NATIVE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Pinned clone RAM adapter. Install ONLY from the preload constructor,
 * before the native application's audio threads start. Never install when
 * Render is pressed: input-handler pages must not lose execute permission
 * while another thread may be executing them. */
int resample_fast_native_install(uintptr_t base);
int resample_fast_native_ready(uintptr_t base);
/* Capture the request on its model owner. Render receives AudioExport's
 * SERVICE CONTEXT, not the AudioExportSettings copied at submission. Never
 * read its +6e8 as a filename or +218 as a sample rate. Scope by context and
 * selected source; retain the copied output path only for the space check. */
int resample_fast_native_arm(void *service_context,unsigned source,const char *output_path);
/* After the native task closes/cleans its files, before the native import:
 * finalize the exact private job's observed stem at its requested WAV path. */
int resample_fast_native_prepare_import(const char *output_path);
/* Last single-export outcome. Zero means no terminal failure. */
const char*resample_fast_native_error(void);
#ifdef __cplusplus
}
#endif
#endif
