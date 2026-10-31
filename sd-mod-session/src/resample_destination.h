#ifndef MPC_RESAMPLE_DESTINATION_H
#define MPC_RESAMPLE_DESTINATION_H
#include "resample_view.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The simple workflow captures already configured native audio. There is no
 * per-resample FX draft and no raw-WAV/DSP fallback behind this contract. */
enum resample_source_kind { RESAMPLE_SEQUENCE=1,RESAMPLE_SOUND=2 };
enum resample_destination_page { RD_CLOSED,RD_SOURCE,RD_TARGET,RD_CONFIRM,RD_RUNNING,RD_CANCELLING,RD_ERROR };
struct resample_destination_request {
    uint64_t token,source_epoch;
    enum resample_source_kind kind;
    unsigned source_pad,target_pad,overwrite_confirmed;
};
struct resample_destination_backend {
    void *user;
    /* Copy to the native control owner queue. Never render/mutate in send.
     * It must validate epoch+target occupancy again before starting. */
    int (*start)(void*,const struct resample_destination_request*);
    int (*cancel)(void*,uint64_t token);
};
struct resample_destination {
    struct resample_destination_backend backend;
    enum resample_destination_page page;
    enum resample_source_kind kind;
    uint64_t serial,token,source_epoch,occupied[2],done,total;
    unsigned source_pad,target_pad,bank,has_sample,has_sequence,applied,cancel_retry;
    unsigned char held[3];
    char sample[80],sequence[80],error[96];
};
void resample_destination_init(struct resample_destination*,const struct resample_destination_backend*);
int resample_destination_open(struct resample_destination*,unsigned source_pad,const char*sample,
    const char*sequence,int has_sample,int has_sequence,uint64_t source_epoch,const uint64_t occupied[2]);
int resample_destination_button(struct resample_destination*,unsigned button,int down);
int resample_destination_pad(struct resample_destination*,unsigned physical_pad,int down);
int resample_destination_bank(struct resample_destination*,unsigned bank);
int resample_destination_target(struct resample_destination*,unsigned target);
void resample_destination_cancel(struct resample_destination*);
void resample_destination_retry_cancel(struct resample_destination*);
void resample_destination_progress(struct resample_destination*,uint64_t token,uint64_t done,uint64_t total);
/* Completion means native assignment and its undo transaction completed,
 * or failed work has fully stopped/restored. Recovery still in progress is
 * not a terminal reply and must keep the dialog alive.
 * A late cancellation must not disguise a committed result as Cancelled. */
void resample_destination_finished(struct resample_destination*,uint64_t token,int applied,const char*error);
void resample_destination_paint(const struct resample_destination*,const struct resample_painter*);
#ifdef __cplusplus
}
#endif
#endif
