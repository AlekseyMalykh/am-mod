#include "resample_panel.h"
#include <string.h>
static int send(void*p,const struct resample_command*c){
    struct resample_panel*panel=p;
    if(c->action==RS_ACT_SEQUENCE){panel->sequence_requested=1;return 1;}
    if(c->action==RS_ACT_CHOOSE_FX||c->action==RS_ACT_EDIT_FX){
        panel->fx_request=(unsigned)c->action;panel->fx_slot=c->fx_slot;return 1;
    }
    if(!resample_worker_send(panel->worker,c))return 0;
    /* Copy succeeded even if native queue is full. Timer retries wake only;
     * returning failure here would duplicate Prepare/Apply on a UI retry. */
    resample_queue_wake(panel->queue);return 1;
}
void resample_panel_init(struct resample_panel*p,struct resample_worker*w,struct resample_queue*q){
    memset(p,0,sizeof(*p));p->worker=w;p->queue=q;
    struct resample_backend backend={p,send};resample_flow_init(&p->flow,&backend);
}
int resample_panel_open(struct resample_panel*p,unsigned pad,const char*sample,const char*sequence,
    int available,const uint64_t occupied[2]){
    if(!p||!p->worker||!p->queue||!resample_panel_can_detach(p))return 0;
    p->sequence_requested=p->fx_request=0;
    /* Native live Compare is not connected in the first bounded trial. */
    return resample_flow_open(&p->flow,pad,sample,sequence,available,0,occupied);
}
void resample_panel_tick(struct resample_panel*p){
    if(!p)return;
    struct resample_worker_event event;
    if(resample_worker_poll(p->worker,&p->after,&event))resample_flow_update(&p->flow,&event.update);
    resample_flow_retry_cancel(&p->flow);
    if(resample_queue_status(p->queue)<0)resample_queue_wake(p->queue);
}
int resample_panel_button(struct resample_panel*p,unsigned code,int down){return p?resample_flow_button(&p->flow,code,down):0;}
int resample_panel_knob(struct resample_panel*p,unsigned knob,int delta){return p?resample_flow_knob(&p->flow,knob,delta):0;}
void resample_panel_close(struct resample_panel*p){if(p)resample_flow_cancel(&p->flow);}
int resample_panel_can_detach(struct resample_panel*p){return p&&p->flow.page==RS_CLOSED&&resample_queue_status(p->queue)==0;}
void resample_panel_paint(const struct resample_panel*p,const struct resample_painter*paint){if(p)resample_view_paint(&p->flow,paint);}
