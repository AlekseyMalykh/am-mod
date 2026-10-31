#ifndef MPC_PAD_SAMPLE_SOURCE_H
#define MPC_PAD_SAMPLE_SOURCE_H
#include "pad_render.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Local prototype, pinned1.3.0.12 only. Owner/worker thread, never audio/UI.
 * sample_shared is a live native shared_ptr pair owned by the caller during
 * open. On success out owns a separate reference and native read stream;
 * caller may release its reference. No Sample/Program mutation or DSP.
 * Select layer/range on the control side before calling. */
enum pad_render_status pad_sample_stream_open(uintptr_t base,
    const uintptr_t sample_shared[2],struct pad_render_stream *out);
struct pad_sample_selection {
    struct pad_render_stream stream;
    uint64_t first,end;
    unsigned pad;
};
/* Snapshot layer0 Sample and Trim of one explicit Program pad0..127.
 * Caller owns/stabilizes Program/parameters during open. After success the
 * selection owns only the Sample/reader, never borrowed Program/layer data.
 * Pitch/envelopes/filter/FX and multi-layer mixing are NOT baked here. */
enum pad_render_status pad_sample_stream_open_pad(uintptr_t base,void *program,
    unsigned pad,struct pad_sample_selection *out);
#ifdef __cplusplus
}
#endif
#endif
