#ifndef AM_PATTERN_LIFECYCLE_H
#define AM_PATTERN_LIFECYCLE_H
#ifdef __cplusplus
extern "C" {
#endif
/* Optional phone hook adapters. Admission remains closed: no production
 * binding/publish API is exposed before native writer coverage is complete.
 * Preserve original callback arguments, results and archive composition. */
void am_pattern_lifecycle_load(void*,void*,void(*original)(void*,void*));
int am_pattern_lifecycle_read(void*,void*,int(*original)(void*,void*));
void am_pattern_lifecycle_new(void*,int*,void(*original)(void*,int*));
#ifdef __cplusplus
}
#ifdef AM_PATTERN_LIFECYCLE_TEST_ACCESS
#include "pattern_source.h"
namespace am_pattern {SourceDomain&lifecycle_test_domain();}
#endif
#endif
#endif
