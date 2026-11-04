#ifndef AM_MIXER_SOURCES_H
#define AM_MIXER_SOURCES_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Native Copy context ctor/dtor owner. Stop before its native destructor. */
void mixer_sources_bind(uintptr_t image,void *context);
void mixer_sources_stop(void *context);
struct mixer_sources_snapshot {
    void *program;
    uint64_t source_epoch,archive_generation,present[2],serial,captured_ms;
};
/* UI owner requests an async refresh and takes only a copied snapshot.
 * No native model getter runs on the UI/audio input path. */
int mixer_sources_poll(void *program,uint64_t source_epoch,struct mixer_sources_snapshot*);
#ifdef __cplusplus
}
#endif
#endif
