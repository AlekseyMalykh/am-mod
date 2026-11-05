#include "resample_ui.h"
#include "resample_runtime.h"
#include "fx_eq_editor.h"
#include "fx_catalog.h"
#include <atomic>
#include <mutex>
#include <new>
#include <cmath>
#include <cstdio>
#include <cstring>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
static uintptr_t base;
struct Binding {void *copy;resample_context *context;std::atomic<bool> dead{false};};
static std::mutex binding_mutex;
static Binding *binding,*session_binding;
static resample_runtime *runtime; /* UI only, never accessed by producer. */
static void *root;
static std::atomic<uintptr_t> root_id{0};
static std::atomic<unsigned> active{0},ready{0},shift{0},root_dead{0},fault{0};
static std::atomic<uint64_t> ticket{1};
static std::atomic<unsigned char> held[112];
static int error,sequence,eq_editor,remove_confirm;
/* editor:1 graphic EQ,2 effect catalogue,3 native parameters. */
static unsigned slot,catalog_index,native_page;
static const char*move_message="";
static fx_eq_editor eq{};
static fx_chain_config fx{};
static double sample_rate(){double rate=resample_runtime_rate(runtime);return rate>0?rate:48000;}
static int visible(void*p){return !root_dead.load(std::memory_order_acquire)&&p&&F(p,0,uintptr_t)==base+0x6e212a0&&reinterpret_cast<int(*)(void*)>(base+0x112fd50)(p)&&!reinterpret_cast<int(*)(void*)>(base+0x112f6b0)(p);}
static void repaint(){if(root&&!root_dead.load(std::memory_order_acquire))reinterpret_cast<void(*)(void*,uint64_t,uint64_t)>(base+0x1149880)(root,0,F(root,0x28,uint64_t));}
extern "C" void resample_ui_init(uintptr_t b){base=b;}
extern "C" void resample_ui_bind(void*copy){
    auto*context=resample_context_create(base,copy);if(!context){std::fprintf(stderr,"RESAMPLE CONTEXT_BIND_REJECTED\n");return;}
    auto*b=new(std::nothrow)Binding{};if(!b){resample_context_release(context);return;}b->copy=copy;b->context=context;
    std::lock_guard<std::mutex> lock(binding_mutex);
    if(binding){binding->dead.store(true,std::memory_order_release);resample_context_stop(binding->context);}
    /* Binding stays valid through outstanding input/runtime lifetime; app
     * registry is constructed once. No native queue counter held by history. */
    binding=b;if(active.load(std::memory_order_acquire))fault.store(1,std::memory_order_release);
    std::fprintf(stderr,"RESAMPLE CONTEXT_BOUND\n");
    ready.store(0,std::memory_order_release);ticket.fetch_add(1,std::memory_order_acq_rel);
}
extern "C" void resample_ui_context_dead(void*copy){
    std::lock_guard<std::mutex> lock(binding_mutex);
    if(binding&&binding->copy==copy){binding->dead.store(true,std::memory_order_release);resample_context_stop(binding->context);}
    fault.store(1,std::memory_order_release);ready.store(0,std::memory_order_release);ticket.fetch_add(1,std::memory_order_acq_rel);
}
extern "C" void resample_ui_project_changing(){
    std::lock_guard<std::mutex> lock(binding_mutex);
    if(binding&&!binding->dead.load(std::memory_order_acquire))resample_context_changed(binding->context);
    fault.store(1,std::memory_order_release);ready.store(0,std::memory_order_release);ticket.fetch_add(1,std::memory_order_acq_rel);
}
extern "C" void resample_ui_destroyed(void*p){if(root_id.load(std::memory_order_acquire)==reinterpret_cast<uintptr_t>(p)){root_dead.store(1,std::memory_order_release);ready.store(0,std::memory_order_release);ticket.fetch_add(1,std::memory_order_acq_rel);}}
extern "C" void resample_ui_requested(unsigned code,int down){
    if(code==0x31)shift.store(down!=0,std::memory_order_release);
    if(code<112&&down&&code!=0x31&&code!=0x51&&code!=0x52&&active.load(std::memory_order_acquire))held[code].store(1,std::memory_order_release);
}
extern "C" int resample_ui_owns(unsigned kind,unsigned code){
    if(kind==1)return active.load(std::memory_order_acquire)&&code!=0x13;
    if(kind==2)return (code<112&&held[code].load(std::memory_order_acquire))||active.load(std::memory_order_acquire);
    return 0;
}
extern "C" int resample_ui_claim_pad(unsigned pad,int down,int blocked){
    if(active.load(std::memory_order_acquire))return 1;
    if(pad!=10||!down||blocked||!ready.load(std::memory_order_acquire)||!shift.load(std::memory_order_acquire))return 0;
    unsigned expected=0;return active.compare_exchange_strong(expected,1,std::memory_order_acq_rel);
}
extern "C" uint64_t resample_ui_ticket(){return ticket.load(std::memory_order_acquire);}
extern "C" void resample_ui_pad(unsigned pad,int down,uint64_t generation){
    if(pad!=10||!down||generation!=resample_ui_ticket()||runtime||error)return;
    std::lock_guard<std::mutex> lock(binding_mutex);
    if(!binding||binding->dead.load(std::memory_order_acquire)||!visible(root)){error=1;repaint();return;}
    session_binding=binding;runtime=resample_runtime_create(base,binding->copy,binding->context);
    if(!runtime||!resample_runtime_open(runtime)){error=1;repaint();return;}
    eq_editor=remove_confirm=0;std::fprintf(stderr,"RESAMPLE OPEN owner_capture_pending\n");repaint();
}
static void eq_commit(){
    fx.stage[slot].eq=eq.config;
    fx.stage[slot].bypass=eq.config.bypass;
    /* One global bypass owner: FX list can re-enable the same EQ. */
    fx.stage[slot].eq.bypass=0;
    resample_runtime_set_fx(runtime,&fx);
}
static void open_slot(){
    if(slot>=fx.count){eq_editor=2;catalog_index=0;return;}
    if(fx.stage[slot].kind==FX_STAGE_NATIVE){eq_editor=3;native_page=0;move_message="";return;}
    fx_eq_config config=fx.stage[slot].eq;config.bypass=fx.stage[slot].bypass;
    eq_editor=fx_eq_editor_open(&eq,&config,sample_rate());eq.shifted=shift.load(std::memory_order_acquire);
}
static unsigned native_parameters(unsigned*indices){
    auto*e=slot<fx.count?fx_catalog_find(fx.stage[slot].native_id):nullptr;unsigned n=0;
    if(e)for(unsigned i=0;i<e->count;++i)if(e->parameter[i]&&*e->parameter[i])indices[n++]=i;
    return n;
}
static void editor_button(unsigned key){
    if(eq_editor==2){
        if(key==0x6b){eq_editor=0;return;}
        if(key==0x6c){auto*e=fx_catalog_at(catalog_index);unsigned i=catalog_index;do{i=(i+1)%fx_catalog_count();}while(i!=catalog_index&&!std::strcmp(e->category,fx_catalog_at(i)->category));catalog_index=i;return;}
        if(key==0x6d||key==0x6f){fx_stage selected{};if(!fx_catalog_stage(catalog_index,sample_rate(),&selected))return;
            if(slot>=fx.count){slot=fx.count;if(fx_chain_add(&fx,&selected,sample_rate())!=FX_OK)return;}else fx.stage[slot]=selected;
            resample_runtime_set_fx(runtime,&fx);resample_runtime_select_fx(runtime,slot);open_slot();}
        return;
    }
    if(eq_editor==3){
        if(key==0x6b){eq_editor=0;return;}
        if(shift.load(std::memory_order_acquire)&&(key==0x6c||key==0x6d)){
            int next=(int)slot+(key==0x6c?-1:1);
            if(next<0||(unsigned)next>=fx.count){move_message=next<0?"Already first in chain":"Already last in chain";return;}
            if(fx_chain_move(&fx,slot,(unsigned)next)==FX_OK){slot=(unsigned)next;resample_runtime_set_fx(runtime,&fx);resample_runtime_select_fx(runtime,slot);move_message="";}
            return;
        }
        if(key==0x6c){eq_editor=2;catalog_index=0;return;}
        if(key==0x6d){fx.stage[slot].bypass^=1u;resample_runtime_set_fx(runtime,&fx);}
        return;
    }
    unsigned button=key>=0x6b&&key<=0x6d?key-0x6b:key==0x6f?3:4;
    int result=fx_eq_editor_button(&eq,button,shift.load(std::memory_order_acquire));
    if(result==EQ_EDITOR_CLOSE)eq_editor=0;
    if(result==EQ_EDITOR_CHANGED)eq_commit();
}
static unsigned logical_button(unsigned code){return code>=0x0c&&code<=0x0e?0x6b+code-0x0c:code;}
extern "C" int resample_ui_button(void*event,uint64_t generation){
    unsigned code=F(event,8,unsigned char),down=F(event,9,unsigned char)!=0;
    bool paired=code<112&&held[code].load(std::memory_order_acquire);
    if(code==0x31){shift.store(down,std::memory_order_release);eq.shifted=down;if(runtime&&session_binding&&!session_binding->dead.load(std::memory_order_acquire))resample_runtime_button(runtime,0x31,down);repaint();return 0;}
    if(code==0x51||code==0x52)return 0;
    if(!active.load(std::memory_order_acquire)&&!paired)return 0;
    if(!down&&code<112)held[code].store(0,std::memory_order_release);
    if(generation!=resample_ui_ticket())return 1;
    if(session_binding&&session_binding->dead.load(std::memory_order_acquire))return 1;
    unsigned key=logical_button(code);
    if(!down&&runtime)resample_runtime_button(runtime,key,0);
    if(error){if(down&&key==0x6b){error=0;active.store(0,std::memory_order_release);ticket.fetch_add(1,std::memory_order_acq_rel);repaint();}return 1;}
    if(remove_confirm){if(down&&(key==0x6b||key==0x6d)){if(key==0x6d){fx_chain_remove(&fx,slot);resample_runtime_set_fx(runtime,&fx);}remove_confirm=0;repaint();}return 1;}
    if(eq_editor){
        if(down){editor_button(key);repaint();}
        return 1;
    }
    if(runtime&&down&&resample_runtime_page(runtime)==RS_FX){
        slot=resample_runtime_fx_slot(runtime);
        if(key==0x09&&resample_runtime_fx_snapshot(runtime,&fx)&&slot<fx.count){remove_confirm=1;repaint();return 1;}
        if(key==0x6f&&shift.load(std::memory_order_acquire)&&resample_runtime_fx_snapshot(runtime,&fx)){catalog_index=0;eq_editor=2;repaint();return 1;}
    }
    if(runtime)resample_runtime_button(runtime,key,down);
    repaint();return 1;
}
extern "C" int resample_ui_control(void*event,uint64_t generation){
    unsigned cc=F(event,8,unsigned char),value=F(event,9,unsigned char);
    if(cc==0x13)return 0;
    if(!active.load(std::memory_order_acquire))return generation!=resample_ui_ticket();
    if(generation!=resample_ui_ticket()||!runtime)return 1;
    if(session_binding&&session_binding->dead.load(std::memory_order_acquire))return 1;
    if(remove_confirm)return 1;
    if(eq_editor==2){
        int delta=cc==0x64?(value>64?(int)value-128:(int)value):0;
        if(cc==0x10)catalog_index=value*fx_catalog_count()/128;
        else if(cc==0x64){int i=(int)catalog_index+delta;if(i<0)i=0;if(i>=(int)fx_catalog_count())i=(int)fx_catalog_count()-1;catalog_index=(unsigned)i;}
    }else if(eq_editor==3){
        unsigned indices[FX_CHAIN_PARAMETERS],n=native_parameters(indices),pages=(n+2)/3;
        if(cc==0x64&&pages){int delta=value>64?(int)value-128:(int)value;int next=(int)native_page+delta;if(next<0)next=0;if(next>=(int)pages)next=(int)pages-1;native_page=(unsigned)next;}
        if(cc>=0x10&&cc<=0x12){unsigned i=native_page*3+cc-0x10;if(i<n){fx.stage[slot].parameter[indices[i]]=value/127.f;resample_runtime_set_fx(runtime,&fx);}}
    }else if(eq_editor==1){
        if(fx_eq_editor_control(&eq,cc,value,shift.load(std::memory_order_acquire))==EQ_EDITOR_CHANGED)eq_commit();
    }else resample_runtime_control(runtime,cc,value);
    repaint();return 1;
}
extern "C" int resample_ui_tick(int blocked){
    bool screen_dead=root_dead.exchange(0,std::memory_order_acq_rel);
    if(screen_dead){root=nullptr;root_id.store(0,std::memory_order_release);}
    if(session_binding&&session_binding->dead.load(std::memory_order_acquire)){
        /* Native destructor may already have cancelled tagged callbacks.
         * Never retry a post through its freed context. In this experimental
         * process-exit path retain the orphan until OS teardown, no wait or
         * claim of a clean dlclose. Hardware shutdown order is still untested. */
        if(runtime)std::fprintf(stderr,"RESAMPLE CONTEXT_STOPPED runtime_retained_until_process_exit\n");
        runtime=nullptr;session_binding=nullptr;error=eq_editor=0;active.store(0,std::memory_order_release);
    }
    if(screen_dead&&runtime)resample_runtime_close(runtime);
    if(fault.exchange(0,std::memory_order_acq_rel)){
        if(runtime)resample_runtime_close(runtime);
        else{active.store(0,std::memory_order_release);error=0;}
    }
    if(runtime){
        if(!visible(root))resample_runtime_close(runtime);
        resample_runtime_tick(runtime);
        unsigned selected=0;
        if(resample_runtime_take_fx_request(runtime,&selected)&&resample_runtime_fx_snapshot(runtime,&fx)){
            slot=selected<fx.count?selected:fx.count;if(slot<FX_CHAIN_MAX)open_slot();
        }
        if(resample_runtime_closed(runtime)){
            sequence=resample_runtime_take_sequence(runtime);int applied=resample_runtime_applied(runtime);
            if(resample_runtime_destroy(runtime)){runtime=nullptr;session_binding=nullptr;eq_editor=0;active.store(0,std::memory_order_release);ticket.fetch_add(1,std::memory_order_acq_rel);std::fprintf(stderr,"RESAMPLE CLOSED applied=%d sequence=%d\n",applied,sequence);}
        }
        repaint();
    }
    bool can_enter=false;{std::lock_guard<std::mutex> lock(binding_mutex);can_enter=binding&&!binding->dead.load(std::memory_order_acquire);}
    bool shown=visible(root);
    static unsigned last_state=~0u,reports;
    unsigned entry_state=(can_enter?1u:0u)|(shown?2u:0u)|(blocked?4u:0u)|(active.load(std::memory_order_acquire)?8u:0u);
    if(entry_state!=last_state&&reports<24){++reports;last_state=entry_state;std::fprintf(stderr,"RESAMPLE ENTRY_STATE context=%u sample=%u blocked=%u open=%u\n",!!(entry_state&1),!!(entry_state&2),!!(entry_state&4),!!(entry_state&8));}
    ready.store(can_enter&&!blocked&&!active.load(std::memory_order_acquire)&&shown,std::memory_order_release);
    /* Keep the UI timer alive while Sample can accept the entry chord, even
     * when no other custom page is polling. Producer never starts a UI timer. */
    return active.load(std::memory_order_acquire)||ready.load(std::memory_order_acquire);
}
extern "C" int resample_ui_active(){return active.load(std::memory_order_acquire)!=0;}
extern "C" int resample_ui_take_sequence(){int requested=sequence;sequence=0;return requested;}
extern "C" void resample_ui_fault(){fault.store(1,std::memory_order_release);}
extern "C" void resample_ui_draw(const resample_painter*p){
    if(!p||!p->rect||!p->text||!active.load(std::memory_order_acquire))return;
    if(!eq_editor&&!error&&!remove_confirm&&runtime){resample_runtime_paint(runtime,p);return;}
    p->rect(p->user,0,5,320,235,0xff000000u);
    p->text(p->user,error?"Cancel":"Back",0,7,106,24,15,0xffff4040u,1);
    if(error){p->text(p->user,"Select a sample pad and retry",8,100,304,25,18,0xffff4040u,0);return;}
    if(remove_confirm){p->text(p->user,"Remove",212,7,106,24,15,0xffff4040u,1);p->text(p->user,"Remove selected effect?",8,95,304,30,20,0xffffffffu,0);p->text(p->user,fx_catalog_stage_name(&fx.stage[slot]),8,132,304,26,20,0xffffc247u,0);return;}
    if(eq_editor==1){fx_eq_editor_paint(&eq,p);return;}
    if(eq_editor==2){
        p->text(p->user,"Cancel",0,7,106,24,15,0xffff4040u,1);p->text(p->user,"Category",106,7,106,24,15,0xffffffffu,1);p->text(p->user,slot<fx.count?"Replace":"Add",212,7,106,24,15,0xffffffffu,1);
        p->text(p->user,"Choose effect",8,40,304,25,21,0xffffffffu,0);
        unsigned first=catalog_index>=4?catalog_index-4:0;
        for(unsigned row=0;row<5&&first+row<fx_catalog_count();++row){auto*e=fx_catalog_at(first+row);p->text(p->user,e->name,8,78+(int)row*25,304,25,19,first+row==catalog_index?0xffffc247u:0xffffffffu,0);}
        p->text(p->user,fx_catalog_at(catalog_index)->category,8,210,304,24,16,0xff39c1d6u,0);return;
    }
    bool moving=shift.load(std::memory_order_acquire)!=0;
    p->text(p->user,moving?"Up":"Change",106,7,106,24,15,0xffffffffu,1);p->text(p->user,moving?"Down":fx.stage[slot].bypass?"Enable":"Bypass",212,7,106,24,15,0xffffffffu,1);
    p->text(p->user,fx_catalog_stage_name(&fx.stage[slot]),8,40,304,28,22,0xffffffffu,0);
    unsigned indices[FX_CHAIN_PARAMETERS],n=native_parameters(indices);auto*entry=fx_catalog_find(fx.stage[slot].native_id);char value[80];
    snprintf(value,sizeof(value),"Page %u/%u%s",native_page+1,(n+2)/3,fx.stage[slot].bypass?"   BYPASS":"");p->text(p->user,value,8,78,304,24,17,0xff39c1d6u,0);
    if(*move_message)p->text(p->user,move_message,8,112,304,24,15,0xffffc247u,0);
    for(unsigned k=0;k<3;++k){unsigned i=native_page*3+k;if(i>=n)continue;unsigned parameter=indices[i];
        int x=(int)k*106,w=k==2?108:106;
        p->text(p->user,entry->parameter[parameter],x+3,157,w-6,32,17,0xffffffffu,1);
        snprintf(value,sizeof(value),"%.1f%%",fx.stage[slot].parameter[parameter]*100);
        p->text(p->user,value,x+3,193,w-6,29,21,0xffffc247u,1);
        p->rect(p->user,x+7,232,w-14,3,0xff303030u);
        int fill=(int)std::lround(fx.stage[slot].parameter[parameter]*(w-14));
        if(fill)p->rect(p->user,x+7,232,fill,3,0xffffc247u);
    }
}
static void rect(void*g,int x,int y,int w,int h,uint32_t colour){reinterpret_cast<void(*)(void*,uint32_t)>(base+0x102d5e0)(g,colour);reinterpret_cast<void(*)(void*,int,int,int,int)>(base+0x102e000)(g,x,y,w,h);}
static void text(void*g,const char*s,int x,int y,int w,int h,unsigned size,uint32_t colour,int centre){
    uint64_t string=0,font=0;reinterpret_cast<void(*)(void*,const char*)>(base+0xf535c0)(&string,s);
    reinterpret_cast<void(*)(void*,int,float)>(base+0x1082810)(&font,0,(float)size);reinterpret_cast<void(*)(void*,const void*)>(base+0x102dca0)(g,&font);
    reinterpret_cast<void(*)(void*,uint32_t)>(base+0x102d5e0)(g,colour);
    reinterpret_cast<void(*)(void*,const void*,int,int,int,int,int,int,float)>(base+0x10817b0)(g,&string,x,y,w,h,centre?36:33,1,.75f);
    reinterpret_cast<void(*)(void*)>(base+0x1070d60)(&font);reinterpret_cast<void(*)(void*)>(base+0xf0a290)(&string);
}
extern "C" void resample_ui_paint(void*p,void*g){
    if(root_id.load(std::memory_order_acquire)!=reinterpret_cast<uintptr_t>(p)){root=p;root_id.store(reinterpret_cast<uintptr_t>(p),std::memory_order_release);root_dead.store(0,std::memory_order_release);}
    resample_painter painter{g,rect,text};resample_ui_draw(&painter);
}
