#ifndef AM_SAMPLE_LOAD_TRACE_H
#define AM_SAMPLE_LOAD_TRACE_H
#include <stdint.h>
typedef int (*sample_file_copy_fn)(const uint64_t *,const uint64_t *);
/* Diagnostic-only forwarding. Caller is the original native return address. */
int sample_load_copy_trace(uintptr_t image,uintptr_t caller,const uint64_t *source,
                           const uint64_t *destination,sample_file_copy_fn original);
#endif
