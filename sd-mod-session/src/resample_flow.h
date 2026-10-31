#ifndef MPC_RESAMPLE_FLOW_H
#define MPC_RESAMPLE_FLOW_H
#include "fx_chain.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* UI-owner controller only. Native adapters MUST enqueue these commands;
 * they must not perform DSP, IO or model mutation inside send(). Worker
 * replies are copied numeric snapshots, delivered to update() on UI. */
enum resample_page { RS_CLOSED,RS_CHOOSE,RS_PREPARING,RS_SAMPLE,RS_FX,
    RS_RENDERING,RS_READY,RS_CONFIRM,RS_SWITCHING,RS_APPLYING,RS_CANCELLING,RS_ERROR };
enum resample_action { RS_ACT_SEQUENCE,RS_ACT_PREPARE,RS_ACT_RENDER,
    RS_ACT_COMPARE,RS_ACT_APPLY,RS_ACT_CANCEL,RS_ACT_RELEASE_STALE,RS_ACT_CHOOSE_FX,RS_ACT_EDIT_FX };
enum resample_reply { RS_REPLY_PREPARED,RS_REPLY_PROGRESS,RS_REPLY_RENDERED,
    RS_REPLY_COMPARED,RS_REPLY_APPLIED,RS_REPLY_CANCELLED,RS_REPLY_FAILED };
struct resample_spec {
    unsigned source,destination,repeats,tail_ms,velocity,overwrite_confirmed;
    uint64_t revision;
    struct fx_chain_config fx;
};
struct resample_command {
    enum resample_action action;
    uint64_t token;
    struct resample_spec spec;
    unsigned rendered; /* requested comparison;0=Original,1=Rendered */
    unsigned fx_slot;
};
struct resample_update {
    enum resample_reply reply;
    uint64_t token,revision;
    uint64_t captured,capture_total,done,total;
    double rate,peak;
    unsigned rendered;
    unsigned velocity_supported;
    const char *error; /* copied during update; never retained */
};
struct resample_backend {void *user;int (*send)(void*,const struct resample_command*);};
struct resample_flow {
    enum resample_page page;
    struct resample_backend backend;
    struct resample_spec spec;
    uint64_t serial,token,captured,capture_total,done,total;
    double rate,peak;
    unsigned has_sample,compare_rendered,compare_supported,velocity_supported,replace,target_bank,target_pad,fx_slot,shift;
    unsigned cancel_to_sample,cancel_retry;
    unsigned applied; /* Terminal outcome survives closing for host feedback. */
    uint64_t occupied[2];
    unsigned char held[3];
    char sample[80],sequence[80],message[96];
};
void resample_flow_init(struct resample_flow *,const struct resample_backend *);
int resample_flow_open(struct resample_flow *,unsigned source,const char *sample,
    const char *sequence,int has_sample,int compare_supported,const uint64_t occupied[2]);
/* Physical pad11 is index10; native shortcut routing remains a host concern.
 * Never claim it over naming, Steps or another modal. */
int resample_flow_shortcut(unsigned pad,int shift,int blocked);
/* B1=6b B2=6c B3=6d.1=owned,0=pass; both edges paired across page changes.
 * Shift=31 is observed and passed through: forward both edges. In FX,
 * Shift+B2/B3 move the selected stage up/down with its settings.
 * Play/Stop, Main and Shift are left to the ordinary app. */
int resample_flow_button(struct resample_flow *,unsigned code,int down);
int resample_flow_knob(struct resample_flow *,unsigned knob,int delta);
int resample_flow_set_fx(struct resample_flow *,const struct fx_chain_config *);
void resample_flow_update(struct resample_flow *,const struct resample_update *);
/* Context change/host close requests cleanup and waits for its acknowledgement. */
void resample_flow_cancel(struct resample_flow *);
void resample_flow_retry_cancel(struct resample_flow *);
unsigned resample_flow_destination(const struct resample_flow *);
const char *resample_flow_button_label(const struct resample_flow *,unsigned button);
#ifdef __cplusplus
}
#endif
#endif
