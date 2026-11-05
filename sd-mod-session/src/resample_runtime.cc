#include "resample_runtime.h"
#include <mutex>
#include <atomic>
#include <new>
#include <cstring>
/* UI and queue touch disjoint state except numeric lifecycle flags, the
 * published worker pair, and worker's copied send/poll queue. No UI calls
 * ever occur inside the native queue callback. */
struct resample_runtime {
    std::mutex mutex;
    resample_context *context;
    resample_context_hook hook{};
    resample_queue *queue=nullptr;
    resample_native *native=nullptr;
    resample_worker *worker=nullptr;
    resample_selection selection{};
    resample_panel panel{}; /* UI only. */
    std::atomic<unsigned> status{0}; /*0 closed,1 boot,2 ready,3 fail,4 close */
    std::atomic<bool> close_requested{false};
    bool panel_bound=false,applied=false;
    unsigned char held[112]{};
    bool sequence_requested=false;
    unsigned fx_request=0,fx_slot=0;
    unsigned focus=0;
    const char *error="Select a sample pad and retry";
};
static void stop_queue(void*p){resample_queue_stop(static_cast<resample_queue*>(p));}
static int run(void*p){
    auto*r=static_cast<resample_runtime*>(p);
    unsigned status=r->status.load(std::memory_order_acquire);
    if(status==1){
        if(r->close_requested.load(std::memory_order_acquire)){r->status.store(0,std::memory_order_release);return 0;}
        resample_native_config config{};resample_selection selected{};
        if(!resample_context_capture(r->context,&config,&selected)){r->error=resample_context_error(r->context);r->status.store(3,std::memory_order_release);return 0;}
        resample_native*n=resample_native_create(&config);config.commit.release(config.commit.user);
        resample_worker*w=nullptr;
        if(n){resample_worker_backend backend{};resample_native_backend(n,&backend);w=resample_worker_create(&backend);}
        if(!n||!w){resample_native_destroy(n);r->error="Cannot create resample worker";r->status.store(3,std::memory_order_release);return 0;}
        {std::lock_guard<std::mutex> lock(r->mutex);r->native=n;r->worker=w;r->selection=selected;}
        r->status.store(2,std::memory_order_release);
        return r->close_requested.load(std::memory_order_acquire)?1:0;
    }
    if(status==2){
        /* UI owns sending Cancel; callback only pumps copied commands. A
         * close arriving before panel binding has no source job to cancel. */
        resample_worker_step(r->worker);
        return resample_worker_work_pending(r->worker);
    }
    if(status==4){
        /* UI switched status only after its panel was closed and queue idle;
         * no sender/poller can still borrow worker/native from now on. */
        if(!resample_worker_destroy(r->worker))return -1;
        r->worker=nullptr;
        if(!resample_native_destroy(r->native))return -1;
        r->native=nullptr;r->status.store(0,std::memory_order_release);return 0;
    }
    return 0;
}
extern "C" resample_runtime *resample_runtime_create(uintptr_t base,void*copy,resample_context*context){
    if(!context||!resample_context_retain(context))return nullptr;
    auto*r=new(std::nothrow)resample_runtime{};if(!r){resample_context_release(context);return nullptr;}r->context=context;
    r->queue=resample_queue_create(base,copy,run,r);
    if(!r->queue){resample_context_release(context);delete r;return nullptr;}
    r->hook.stop=stop_queue;r->hook.user=r->queue;
    if(!resample_context_add_hook(context,&r->hook)){resample_queue_destroy(r->queue);resample_context_release(context);delete r;return nullptr;}
    return r;
}
extern "C" int resample_runtime_open(resample_runtime*r){
    if(!r||!resample_runtime_closed(r))return 0;
    r->close_requested.store(false,std::memory_order_release);r->panel_bound=false;r->applied=false;
    r->sequence_requested=false;r->fx_request=0;r->error="Select a sample pad and retry";
    r->status.store(1,std::memory_order_release);resample_queue_wake(r->queue);return 1;
}
extern "C" void resample_runtime_tick(resample_runtime*r){
    if(!r)return;
    if(resample_queue_status(r->queue)<0)resample_queue_wake(r->queue);
    unsigned status=r->status.load(std::memory_order_acquire);
    if(status==2){
        if(!r->panel_bound){
            std::lock_guard<std::mutex> lock(r->mutex);
            resample_panel_init(&r->panel,r->worker,r->queue);
            if(!resample_panel_open(&r->panel,r->selection.pad,r->selection.sample,"Current sequence",1,r->selection.occupied))return;
            r->panel_bound=true;
        }
        if(r->close_requested.load(std::memory_order_acquire))resample_panel_close(&r->panel);
        resample_panel_tick(&r->panel);
        if(r->panel.sequence_requested){r->sequence_requested=true;r->panel.sequence_requested=0;}
        if(r->panel.fx_request){r->fx_request=r->panel.fx_request;r->fx_slot=r->panel.fx_slot;r->panel.fx_request=0;}
        if(resample_panel_can_detach(&r->panel)){
            r->applied=r->panel.flow.applied!=0;r->panel_bound=false;r->status.store(4,std::memory_order_release);resample_queue_wake(r->queue);
        }
    }else if(status==3&&r->close_requested.load(std::memory_order_acquire)&&resample_queue_status(r->queue)==0){
        r->status.store(0,std::memory_order_release);
    }
    if(resample_queue_status(r->queue)<0)resample_queue_wake(r->queue);
}
extern "C" int resample_runtime_button(resample_runtime*r,unsigned code,int down){
    if(!r)return 0;
    if(code==0x51||code==0x52||code==0x31)return r->panel_bound?resample_panel_button(&r->panel,code,down):0;
    bool active=r->status.load(std::memory_order_acquire)!=0;
    bool paired=code<112&&r->held[code];
    if(!active&&!paired)return 0;
    if(code<112)r->held[code]=down?1:0;
    if(r->panel_bound){resample_panel_button(&r->panel,code,down);return 1;}
    if(code==0x6b&&down)resample_runtime_close(r);
    return 1;
}
extern "C" int resample_runtime_knob(resample_runtime*r,unsigned k,int delta){return r&&r->panel_bound?resample_panel_knob(&r->panel,k,delta):r&&r->status.load(std::memory_order_acquire)!=0;}
extern "C" void resample_runtime_close(resample_runtime*r){
    if(!r)return;
    r->close_requested.store(true,std::memory_order_release);
    /* UI-owner request: copy Cancel now, before a prepared native commit's
     * next tick. Waiting for the next UI timer would unnecessarily let that
     * commit win even when the owner had not started its dispatch. */
    if(r->panel_bound)resample_panel_close(&r->panel);
}
extern "C" int resample_runtime_closed(resample_runtime*r){return r&&r->status.load(std::memory_order_acquire)==0&&resample_queue_status(r->queue)==0;}
extern "C" int resample_runtime_destroy(resample_runtime*r){
    if(!r)return 1;
    if(!resample_runtime_closed(r))return 0;
    resample_context_remove_hook(r->context,&r->hook);
    if(!resample_queue_destroy(r->queue))return 0;
    resample_context_release(r->context);delete r;return 1;
}
extern "C" resample_page resample_runtime_page(resample_runtime*r){
    if(!r)return RS_CLOSED;
    if(r->panel_bound)return r->panel.flow.page;
    unsigned state=r->status.load(std::memory_order_acquire);return state==0?RS_CLOSED:state==3?RS_ERROR:state==4?RS_CANCELLING:RS_PREPARING;
}
extern "C" void resample_runtime_paint(resample_runtime*r,const resample_painter*p){
    if(!r||!p||!p->text||!p->rect)return;
    if(r->panel_bound){resample_panel_paint(&r->panel,p);return;}
    resample_page page=resample_runtime_page(r);if(page==RS_CLOSED)return;
    p->rect(p->user,0,5,320,235,0xff000000u);
    p->text(p->user,"Cancel",0,7,106,24,15,0xffff4040u,1);
    p->text(p->user,"Resample",8,40,304,25,21,0xffffffffu,0);
    p->text(p->user,page==RS_ERROR?r->error:page==RS_CANCELLING?"Closing...":"Preparing selected sound...",8,113,304,25,18,page==RS_ERROR?0xffff4040u:0xffffffffu,0);
    p->text(p->user,page==RS_ERROR?"Cancel and reopen Sample":"Waiting for model queue...",8,165,304,22,14,0xffaaaaaau,0);
}
extern "C" int resample_runtime_set_fx(resample_runtime*r,const fx_chain_config*fx){return r&&r->panel_bound&&resample_flow_set_fx(&r->panel.flow,fx);}
extern "C" int resample_runtime_applied(resample_runtime*r){return r&&r->applied;}
extern "C" int resample_runtime_take_sequence(resample_runtime*r){if(!r||!resample_runtime_closed(r))return 0;bool requested=r->sequence_requested;r->sequence_requested=false;return requested;}
extern "C" int resample_runtime_take_fx_request(resample_runtime*r,unsigned*slot){
    if(!r)return 0;
    unsigned request=r->panel_bound?r->fx_request:0;r->fx_request=0;if(slot)*slot=r->fx_slot;return (int)request;
}
extern "C" int resample_runtime_fx_snapshot(resample_runtime*r,fx_chain_config*out){
    if(!r||!out||!r->panel_bound)return 0;
    *out=r->panel.flow.spec.fx;return 1;
}
extern "C" int resample_runtime_select_fx(resample_runtime*r,unsigned slot){
    if(!r||!r->panel_bound||r->panel.flow.page!=RS_FX||slot>=FX_CHAIN_MAX)return 0;
    r->panel.flow.fx_slot=slot;return 1;
}
extern "C" double resample_runtime_rate(resample_runtime*r){return r&&r->panel_bound?r->panel.flow.rate:0;}
extern "C" unsigned resample_runtime_fx_slot(resample_runtime*r){return r&&r->panel_bound?r->panel.flow.fx_slot:FX_CHAIN_MAX;}
extern "C" void resample_runtime_progress(resample_runtime*r,struct resample_runtime_progress*out){
    if(!out)return;
    *out={};if(!r)return;
    out->page=resample_runtime_page(r);
    if(!r->panel_bound)return;
    const auto&f=r->panel.flow;
    out->source_done=f.captured;out->source_total=f.capture_total;out->done=f.done;out->total=f.total;
    if(f.page==RS_APPLYING||f.page==RS_CANCELLING)resample_native_progress(r->native,&out->ram_done,&out->ram_total);
}
extern "C" int resample_runtime_control(resample_runtime*r,unsigned cc,unsigned value){
    if(!r||value>127||cc==0x13)return 0;
    if(cc!=0x64&&(cc<0x10||cc>0x12))return 0;
    if(!r->panel_bound)return r->status.load(std::memory_order_acquire)!=0;
    if(cc==0x64)return resample_runtime_knob(r,r->focus,value>64?(int)value-128:(int)value);
    unsigned k=cc-0x10;r->focus=k;auto&f=r->panel.flow;
    if(f.page==RS_SAMPLE){
        if(k==0)f.spec.repeats=1+value*64/128;
        else if(k==1)f.spec.tail_ms=value*10000/127;
        else return 1;
        ++f.spec.revision;return 1;
    }
    if(f.page==RS_READY){
        f.spec.overwrite_confirmed=0;
        if(k==0)f.replace=value<64;
        else if(!f.replace){if(k==1)f.target_bank=value*8/128;else f.target_pad=value*16/128;}
        return 1;
    }
    if(f.page==RS_FX&&k==0){unsigned count=f.spec.fx.count<FX_CHAIN_MAX?f.spec.fx.count+1:FX_CHAIN_MAX;f.fx_slot=value*count/128;return 1;}
    return 1;
}
