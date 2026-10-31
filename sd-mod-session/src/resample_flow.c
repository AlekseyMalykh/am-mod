#include "resample_flow.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int clamp(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
static void copy(char*out,size_t capacity,const char*text){
    if(!text)text="";
    size_t n=strlen(text);if(n>=capacity){n=capacity-1;while(n&&((unsigned char)text[n]&0xc0)==0x80)--n;}
    memcpy(out,text,n);out[n]=0;
}
static void message(struct resample_flow*f,const char*text){copy(f->message,sizeof(f->message),text);}
static int send(struct resample_flow*f,enum resample_action action,unsigned rendered,uint64_t token){
    struct resample_command c={action,token,f->spec,rendered,f->fx_slot};
    c.spec.destination=resample_flow_destination(f);
    return f->backend.send&&f->backend.send(f->backend.user,&c);
}
static uint64_t next(struct resample_flow*f){if(!++f->serial)++f->serial;return f->serial;}
void resample_flow_init(struct resample_flow*f,const struct resample_backend*b){
    memset(f,0,sizeof(*f));if(b)f->backend=*b;
}
int resample_flow_open(struct resample_flow*f,unsigned source,const char*sample,const char*sequence,
    int available,int compare,const uint64_t occupied[2]){
    if(!f||f->page!=RS_CLOSED||source>=128)return 0;
    f->page=RS_CHOOSE;f->has_sample=!!available;f->compare_supported=!!compare;
    f->spec=(struct resample_spec){.source=source,.destination=source,.repeats=1,.velocity=127,.revision=1};
    f->replace=1;f->target_bank=source/16;f->target_pad=source%16;f->compare_rendered=f->velocity_supported=0;
    f->captured=f->capture_total=f->done=f->total=0;f->rate=f->peak=0;f->token=0;
    f->cancel_to_sample=f->cancel_retry=f->fx_slot=0;
    f->applied=0;
    copy(f->sample,sizeof(f->sample),sample);copy(f->sequence,sizeof(f->sequence),sequence);message(f,"");
    f->occupied[0]=occupied?occupied[0]:0;f->occupied[1]=occupied?occupied[1]:0;
    if(available)f->occupied[source/64]|=UINT64_C(1)<<(source%64);
    return 1;
}
int resample_flow_shortcut(unsigned pad,int shift,int blocked){return pad==10&&shift&&!blocked;}
unsigned resample_flow_destination(const struct resample_flow*f){return f->replace?f->spec.source:f->target_bank*16+f->target_pad;}
static void cancel_to(struct resample_flow*f,unsigned sample){
    if(f->page==RS_CLOSED)return;
    if(f->page==RS_CHOOSE){f->page=RS_CLOSED;return;}
    /* Backend may still be preparing its native Sample. Ask for cancellation
     * and keep the window until its final acknowledgement; committed Apply
     * may win and must be reported as Applied, never disguised as Cancel. */
    if(f->page==RS_CANCELLING)return;
    f->cancel_to_sample=sample;f->page=RS_CANCELLING;
    /* Keep the same job token: backend cancels queued work, restores
     * Original and releases any staged result before replying CANCELLED. */
    f->cancel_retry=!send(f,RS_ACT_CANCEL,0,f->token);
    message(f,f->cancel_retry?"Cancel pending: queue busy":"Restoring original...");
}
void resample_flow_cancel(struct resample_flow*f){if(f)cancel_to(f,0);}
void resample_flow_retry_cancel(struct resample_flow*f){
    if(f&&f->page==RS_CANCELLING&&f->cancel_retry){f->cancel_retry=!send(f,RS_ACT_CANCEL,0,f->token);}
}
static void prepare(struct resample_flow*f){
    uint64_t token=next(f);f->token=token;f->page=RS_PREPARING;message(f,"Reading sample...");
    if(!send(f,RS_ACT_PREPARE,0,token)){f->page=RS_CHOOSE;f->token=0;message(f,"Cannot start: queue busy");}
}
static void render(struct resample_flow*f){
    if(!f->rate||!f->capture_total||!fx_chain_valid(&f->spec.fx,f->rate)){message(f,"Source/FX not ready");return;}
    f->captured=f->done=f->total=0;f->page=RS_RENDERING;message(f,"");
    if(!send(f,RS_ACT_RENDER,0,f->token)){f->page=RS_SAMPLE;message(f,"Cannot render: queue busy");}
}
static void apply(struct resample_flow*f){
    f->page=RS_APPLYING;message(f,"Applying...");
    if(!send(f,RS_ACT_APPLY,0,f->token)){f->page=RS_READY;message(f,"Cannot apply: queue busy");}
}
int resample_flow_button(struct resample_flow*f,unsigned code,int down){
    if(!f)return 0;
    /* Observe Shift without swallowing its native edge. The host must forward
     * both edges even when the modal page is open. Top-key holds remain paired. */
    if(code==0x31){f->shift=!!down;return 0;}
    if(code<0x6b||code>0x6d)return 0;
    unsigned k=code-0x6b;
    if(!down){if(!f->held[k])return 0;f->held[k]=0;return 1;}
    if(f->held[k])return 1;
    if(f->page==RS_CLOSED)return 0;
    f->held[k]=1;
    if(f->page==RS_CHOOSE){
        if(k==0)f->page=RS_CLOSED;
        else if(k==1){if(send(f,RS_ACT_SEQUENCE,0,0))f->page=RS_CLOSED;else message(f,"Sequence unavailable");}
        else if(f->has_sample)prepare(f);else message(f,"Selected pad is empty");
    }else if(f->page==RS_SAMPLE){
        if(k==0)cancel_to(f,0);else if(k==1)f->page=RS_FX;else render(f);
    }else if(f->page==RS_FX){
        if(k==0)f->page=RS_SAMPLE;
        else if(f->shift){
            if(f->fx_slot>=f->spec.fx.count)message(f,"Select an effect");
            else {
                int to=(int)f->fx_slot+(k==1?-1:1);
                if(to>=0&&(unsigned)to<f->spec.fx.count&&fx_chain_move(&f->spec.fx,f->fx_slot,(unsigned)to)==FX_OK){
                    f->fx_slot=(unsigned)to;++f->spec.revision;message(f,"");
                }else message(f,to<0?"Already first in chain":"Already last in chain");
            }
        }
        else if(k==1&&f->fx_slot<f->spec.fx.count){
            f->spec.fx.stage[f->fx_slot].bypass^=1u;++f->spec.revision;
        }else if(k==2&&!send(f,f->fx_slot<f->spec.fx.count?RS_ACT_EDIT_FX:RS_ACT_CHOOSE_FX,0,f->token))message(f,"FX editor unavailable");
    }else if(f->page==RS_READY){
        if(k==0)cancel_to(f,1);
        else if(k==1){
            if(!f->compare_supported)message(f,"Mix comparison not connected");
            else {f->page=RS_SWITCHING;if(!send(f,RS_ACT_COMPARE,!f->compare_rendered,f->token)){f->page=RS_READY;message(f,"Compare: queue busy");}}
        }else{
            unsigned d=resample_flow_destination(f);
            if((f->occupied[d/64]>>(d%64))&1u){f->page=RS_CONFIRM;message(f,"Replace occupied pad?");}
            else {f->spec.overwrite_confirmed=0;apply(f);}
        }
    }else if(f->page==RS_CONFIRM){
        if(k==0){f->page=RS_READY;f->spec.overwrite_confirmed=0;message(f,"");}else if(k==2){f->spec.overwrite_confirmed=1;apply(f);}
    }else if(f->page==RS_ERROR){
        if(k==0)cancel_to(f,0);
    }else if(k==0)cancel_to(f,0);
    return 1;
}
int resample_flow_knob(struct resample_flow*f,unsigned k,int delta){
    if(!f||k>=3||!delta)return 0;
    delta=clamp(delta,-127,127);
    if(f->page==RS_SAMPLE){
        if(k==0)f->spec.repeats=(unsigned)clamp((int)f->spec.repeats+delta,1,64);
        else if(k==1)f->spec.tail_ms=(unsigned)clamp((int)f->spec.tail_ms+delta*10,0,10000);
        else if(f->velocity_supported)f->spec.velocity=(unsigned)clamp((int)f->spec.velocity+delta,1,127);
        else return 1; /* Keep this unavailable knob away from native controls. */
        ++f->spec.revision;return 1;
    }
    if(f->page==RS_READY){
        f->spec.overwrite_confirmed=0;
        if(k==0)f->replace=delta<0;
        else if(!f->replace){if(k==1)f->target_bank=(unsigned)clamp((int)f->target_bank+delta,0,7);else f->target_pad=(unsigned)clamp((int)f->target_pad+delta,0,15);}
        return 1;
    }
    if(f->page==RS_FX&&k==0){unsigned last=f->spec.fx.count<FX_CHAIN_MAX?f->spec.fx.count:FX_CHAIN_MAX-1;f->fx_slot=(unsigned)clamp((int)f->fx_slot+delta,0,(int)last);message(f,"");return 1;}
    return 0;
}
int resample_flow_set_fx(struct resample_flow*f,const struct fx_chain_config*fx){
    if(!f||!fx||(f->page!=RS_SAMPLE&&f->page!=RS_FX)||!fx_chain_valid(fx,f->rate))return 0;
    f->spec.fx=*fx;++f->spec.revision;return 1;
}
void resample_flow_update(struct resample_flow*f,const struct resample_update*u){
    if(!f||!u)return;
    if(u->token!=f->token||f->page==RS_CLOSED){
        if(u->reply==RS_REPLY_RENDERED||u->reply==RS_REPLY_PREPARED)send(f,RS_ACT_RELEASE_STALE,0,u->token);
        return;
    }
    if(f->page==RS_CANCELLING){
        if(u->reply==RS_REPLY_APPLIED){
            f->applied=1;f->compare_rendered=0;f->page=RS_CLOSED;f->token=0;
            message(f,"Applied; Undo available");return;
        }
        if(u->reply==RS_REPLY_FAILED){f->cancel_retry=1;message(f,u->error?u->error:"Restore pending");return;}
        if(u->reply==RS_REPLY_CANCELLED){
            f->compare_rendered=0;message(f,"");f->page=f->cancel_to_sample?RS_PREPARING:RS_CLOSED;
            if(f->cancel_to_sample)prepare(f);else f->token=0;
        }
        return;
    }
    if(u->revision!=f->spec.revision)return;
    if(u->reply==RS_REPLY_FAILED){
        message(f,u->error?u->error:"Resample failed");f->page=RS_ERROR;return;
    }
    if(f->page==RS_PREPARING&&u->reply==RS_REPLY_PREPARED){
        if(!isfinite(u->rate)||u->rate<8000||u->rate>192000||!u->capture_total){f->page=RS_ERROR;message(f,"Invalid sample metadata");return;}
        f->rate=u->rate;f->capture_total=u->capture_total;f->velocity_supported=!!u->velocity_supported;f->page=RS_SAMPLE;message(f,"");
    }else if(f->page==RS_RENDERING&&(u->reply==RS_REPLY_PROGRESS||u->reply==RS_REPLY_RENDERED)){
        if(u->capture_total!=f->capture_total||(f->total&&u->total!=f->total)||u->captured>u->capture_total||u->done>u->total||u->captured<f->captured||u->done<f->done||!isfinite(u->peak)||u->peak<0){f->page=RS_ERROR;message(f,"Invalid render progress");return;}
        f->captured=u->captured;f->capture_total=u->capture_total;f->done=u->done;f->total=u->total;f->peak=u->peak;
        if(u->reply==RS_REPLY_RENDERED){
            if(!u->total||u->done!=u->total||u->captured!=u->capture_total){f->page=RS_ERROR;message(f,"Incomplete render");return;}
            f->page=RS_READY;f->compare_rendered=0;message(f,u->peak>1?"Peak over 0 dBFS":"");
        }
    }else if(f->page==RS_SWITCHING&&u->reply==RS_REPLY_COMPARED){
        f->compare_rendered=!!u->rendered;f->page=RS_READY;message(f,"");
    }else if(f->page==RS_APPLYING&&u->reply==RS_REPLY_APPLIED){
        /* Reply means the native assignment+reset has completed as one
         * undoable transaction, not just that work was enqueued. */
        f->page=RS_CLOSED;f->token=0;f->compare_rendered=0;f->applied=1;
    }
}
const char*resample_flow_button_label(const struct resample_flow*f,unsigned k){
    static const char*choose[]={"Cancel","Seq","Sample"},*sample[]={"Cancel","FX","Render"},*ready[]={"Back","Compare","Apply"},*confirm[]={"Cancel","","Replace"},*fx[]={"Back","Bypass","Edit"};
    if(!f||k>=3||f->page==RS_CLOSED)return "";
    if(f->page==RS_CHOOSE)return choose[k];
    if(f->page==RS_SAMPLE)return sample[k];
    if(f->page==RS_READY)return k==1&&!f->compare_supported?"":ready[k];
    if(f->page==RS_CONFIRM)return confirm[k];
    if(f->page==RS_FX)return f->shift?(k==0?"Back":k==1?"Up":"Down"):k==2&&f->fx_slot>=f->spec.fx.count?"Add":fx[k];
    return k==0?"Cancel":"";
}
