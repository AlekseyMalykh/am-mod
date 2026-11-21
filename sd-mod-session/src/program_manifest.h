#ifndef AM_PROGRAM_MANIFEST_H
#define AM_PROGRAM_MANIFEST_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
struct program_sample {unsigned pad,layer;char name[256],file[1024];};
struct program_manifest {unsigned count;struct program_sample samples[512];};
/* XML metadata only. Expat is resolved from the firmware; no native model,
 * external entities or DTD. Caller allocates output; failures leave it intact. */
int program_xpm_read(const void*,size_t,struct program_manifest*,char error[96]);
#ifdef __cplusplus
}
#endif
#endif
