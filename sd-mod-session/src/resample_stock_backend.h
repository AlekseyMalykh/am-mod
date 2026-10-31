#ifndef MPC_RESAMPLE_STOCK_BACKEND_H
#define MPC_RESAMPLE_STOCK_BACKEND_H
#include "resample_destination.h"
#ifdef __cplusplus
extern "C" {
#endif
struct resample_stock_backend;
struct resample_stock_snapshot {uint64_t epoch,occupied[2];unsigned source,has_sample,has_sequence;char sample[80];};
enum { RSB_NONE,RSB_CAPTURED,RSB_SUBMITTED,RSB_APPLIED,RSB_ERROR,RSB_CANCELLED };
struct resample_stock_reply {unsigned status;uint64_t token;struct resample_stock_snapshot snapshot;char error[96];};
struct resample_stock_backend*resample_stock_backend_create(uintptr_t,void*copy_context);
void resample_stock_backend_stop(struct resample_stock_backend*);
void resample_stock_backend_changed(struct resample_stock_backend*);
int resample_stock_backend_capture(struct resample_stock_backend*);
/* Queue program binding independently of native resample availability. */
int resample_stock_backend_bind_eq(struct resample_stock_backend*);
int resample_stock_backend_start(void*,const struct resample_destination_request*);
int resample_stock_backend_cancel(void*,uint64_t);
/* UI tick; bounded wake, then copy published POD only. No model reads. */
int resample_stock_backend_poll(struct resample_stock_backend*,struct resample_stock_reply*);
int resample_stock_backend_busy(struct resample_stock_backend*);
#ifdef __cplusplus
}
#endif
#endif
