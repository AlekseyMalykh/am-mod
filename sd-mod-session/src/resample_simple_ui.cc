#include "resample_ui.h"
#include "resample_destination.h"
#include "resample_stock_backend.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include "pad_palette.h"
#include "pad_workflow_colour.h"
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
static uintptr_t base;
static std::atomic<resample_stock_backend*>backend{nullptr};
static std::atomic<void*>bound_copy{nullptr};
static resample_stock_backend *session;
static resample_destination dialog{};
static void*root;
static std::atomic<uintptr_t>root_id{0};
static std::atomic<unsigned>active{0},ready{0},shifted{0},dead{0},fault{0};
static std::atomic<uint64_t>generation{1};
static std::atomic<unsigned char>held[112];
static unsigned capturing=0,handoff=0;
static unsigned transport_running,transport_valid,stop_phase,stop_requested;
static unsigned char stop_event[16];
static uint64_t stop_deadline;
static resample_destination_request pending_request{};
static uint64_t now_ms(){timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);return uint64_t(t.tv_sec)*1000+t.tv_nsec/1000000;}
extern "C" void resample_ui_transport(unsigned running,unsigned valid){transport_running=!!running;transport_valid=!!valid;}
extern "C" int resample_ui_take_stop(void*event){if(!stop_requested||!event)return 0;stop_requested=0;std::memcpy(event,stop_event,16);return 1;}
static int start_after_stop(void*user,const resample_destination_request*r){
    if(transport_running||!transport_valid){pending_request=*r;stop_phase=1;return 1;}
    return resample_stock_backend_start(user,r);
}
static std::atomic<uint64_t>palette[16],palette_epoch{1};
static std::atomic<unsigned>palette_active{0};
static void publish_palette(){
    unsigned shown=active.load()&&!capturing&&(dialog.page==RD_TARGET||dialog.page==RD_CONFIRM);
    if(palette_active.exchange(shown)!=shown)palette_epoch.fetch_add(1);
    if(!shown)return;
    for(unsigned i=0;i<16;++i){unsigned pad=dialog.bank*16+i;bool used=(dialog.occupied[pad/64]>>(pad%64))&1;
        bool target=pad==dialog.target_pad;
        uint64_t colour=pad_target_colour(pad,used,target,target,now_ms());
        if(palette[i].exchange(colour)!=colour)palette_epoch.fetch_add(1);
    }
}
static bool visible(){return root&&!dead.load()&&F(root,0,uintptr_t)==base+0x6e212a0&&reinterpret_cast<int(*)(void*)>(base+0x112fd50)(root)&&!reinterpret_cast<int(*)(void*)>(base+0x112f6b0)(root);}
static void repaint(){publish_palette();if(root&&!dead.load())reinterpret_cast<void(*)(void*,uint64_t,uint64_t)>(base+0x1149880)(root,0,F(root,0x28,uint64_t));}
extern "C" void resample_ui_init(uintptr_t b){base=b;}
extern "C" int resample_ui_shutdown_busy(void){return capturing||stop_phase||(session&&resample_stock_backend_busy(session));}
extern "C" void resample_ui_bind(void*copy){
    auto*next=resample_stock_backend_create(base,copy);auto*old=backend.exchange(next);
    if(old)resample_stock_backend_stop(old);
    bound_copy.store(copy);fault.store(1);ready.store(0);generation.fetch_add(1);
    std::fprintf(stderr,"RESAMPLE STOCK_BIND ready=%u\n",next!=nullptr);
}
extern "C" void resample_ui_context_dead(void*p){if(bound_copy.load()!=p)return;auto*b=backend.exchange(nullptr);if(b)resample_stock_backend_stop(b);ready.store(0);fault.store(1);generation.fetch_add(1);}
extern "C" void resample_ui_project_changing(){auto*b=backend.load();if(b)resample_stock_backend_changed(b);ready.store(0);fault.store(1);generation.fetch_add(1);}
extern "C" void resample_ui_destroyed(void*p){if(root_id.load()==reinterpret_cast<uintptr_t>(p)){dead.store(1);ready.store(0);fault.store(1);generation.fetch_add(1);}}
extern "C" void resample_ui_requested(unsigned code,int down){if(code==0x31)shifted.store(down!=0);if(code<112&&down&&active.load()&&code!=0x31&&code!=0x51&&code!=0x52)held[code].store(1);}
extern "C" int resample_ui_owns(unsigned kind,unsigned code){
    if(kind==1)return active.load()&&code!=0x13;
    if(kind==2)return active.load()||(code<112&&held[code].load());
    return 0;
}
extern "C" int resample_ui_claim_pad(unsigned pad,int down,int blocked){
    if(active.load())return 1;
    if(pad!=10||!down||blocked||!ready.load()||!shifted.load())return 0;
    unsigned expected=0;return active.compare_exchange_strong(expected,1);
}
extern "C" uint64_t resample_ui_ticket(){return generation.load();}
extern "C" void resample_ui_pad(unsigned pad,int down,uint64_t ticket){
    if(ticket!=generation.load())return;
    if(!session&&pad==10&&down&&active.load()){
        session=backend.load();capturing=1;handoff=0;
        resample_destination_backend adapter{session,start_after_stop,resample_stock_backend_cancel};resample_destination_init(&dialog,&adapter);
        if(!session||!resample_stock_backend_capture(session)){capturing=0;dialog.page=RD_ERROR;std::snprintf(dialog.error,sizeof(dialog.error),"Cannot capture selected source");}
    }else if(session&&!capturing)resample_destination_pad(&dialog,pad,down);
    repaint();
}
static void close_dialog(){active.store(0);ready.store(0);capturing=0;stop_phase=stop_requested=0;dialog.page=RD_CLOSED;if(session&&!resample_stock_backend_busy(session))session=nullptr;generation.fetch_add(1);repaint();}
extern "C" int resample_ui_button(void*e,uint64_t ticket){
    unsigned code=F(e,8,unsigned char),down=F(e,9,unsigned char)!=0;
    if(code==0x31){shifted.store(down);return 0;}if(code==0x51||code==0x52)return 0;
    bool own=code<112&&held[code].load();if(!active.load()&&!own)return 0;
    if(!down&&code<112)held[code].store(0);
    if(ticket!=generation.load())return 1;
    unsigned key=code>=0x6b&&code<=0x6d?code-0x6b:code>=0x0c&&code<=0x0e?code-0x0c:3;
    if(stop_phase){
        if(!down&&key<3)dialog.held[key]=0;
        if(down&&stop_phase==1&&key==0){
            std::memcpy(stop_event,e,16);stop_event[8]=0x51;stop_event[9]=1;
            stop_requested=1;stop_phase=2;stop_deadline=now_ms()+3000;
            std::fprintf(stderr,"RESAMPLE STOP_REQUESTED before_submit=1\n");
        }else if(down&&key==2){stop_phase=0;dialog.page=RD_TARGET;}
        repaint();return 1;
    }
    if(capturing){if(down&&key==0)close_dialog();return 1;}
    if(key<3)resample_destination_button(&dialog,key,down);
    else if(down&&dialog.page==RD_TARGET){
        if(code==0x36||code==0x37||code==0x23){int delta=(code==0x37||(code==0x23&&shifted.load()))?-1:1;resample_destination_bank(&dialog,(dialog.bank+8+delta)%8);}
    }
    if(dialog.page==RD_CLOSED)close_dialog();else repaint();return 1;
}
extern "C" int resample_ui_control(void*e,uint64_t ticket){
    unsigned cc=F(e,8,unsigned char),value=F(e,9,unsigned char);if(cc==0x13)return 0;
    if(!active.load())return ticket!=generation.load();
    if(ticket!=generation.load())return 1;
    if(dialog.page==RD_TARGET&&!capturing){if(cc==0x10)resample_destination_bank(&dialog,value*8/128);
        else if(cc==0x64){int delta=value>64?(int)value-128:(int)value;int target=(int)(dialog.target_pad<128?dialog.target_pad:dialog.bank*16)+delta;if(target<0)target=0;if(target>127)target=127;resample_destination_target(&dialog,(unsigned)target);}}
    repaint();return 1;
}
extern "C" int resample_ui_tick(int blocked){
    if(dead.exchange(0)){root=nullptr;root_id.store(0);}
    if(fault.exchange(0)){close_dialog();session=nullptr;handoff=0;}
    auto*current=backend.load();auto*poller=session?session:current;
    if(stop_phase==2){
        if(transport_valid&&!transport_running){
            stop_phase=0;
            if(!session||!resample_stock_backend_start(session,&pending_request)){dialog.page=RD_ERROR;std::snprintf(dialog.error,sizeof(dialog.error),"Render submission failed");}
            std::fprintf(stderr,"RESAMPLE STOP_CONFIRMED before_submit=1\n");repaint();
        }else if(now_ms()>=stop_deadline){stop_phase=0;dialog.page=RD_ERROR;std::snprintf(dialog.error,sizeof(dialog.error),"Transport did not stop; destination unchanged");repaint();}
    }
    if(poller){resample_stock_reply reply{};
        if(resample_stock_backend_poll(poller,&reply)){
            if(reply.status==RSB_CAPTURED&&capturing&&active.load()){
                capturing=0;resample_destination_open(&dialog,reply.snapshot.source,reply.snapshot.sample,"Current sequence",reply.snapshot.has_sample,reply.snapshot.has_sequence,reply.snapshot.epoch,reply.snapshot.occupied);repaint();
            }else if(reply.status==RSB_SUBMITTED){
                /* Stock background task owns progress and cancellation now.
                 * Submission is not completion and must not set applied. */
                handoff=1;close_dialog();std::fprintf(stderr,"RESAMPLE STOCK_PROGRESS_HANDOFF\n");
            }else if(reply.status==RSB_APPLIED){
                std::fprintf(stderr,"RESAMPLE STOCK_APPLIED token=%llu\n",(unsigned long long)reply.token);handoff=0;session=nullptr;
            }else if(reply.status==RSB_CANCELLED){close_dialog();session=nullptr;handoff=0;}
            else if(reply.status==RSB_ERROR){
                capturing=0;handoff=0;if(session){dialog.page=RD_ERROR;std::snprintf(dialog.error,sizeof(dialog.error),"%s",reply.error);if(visible())active.store(1);repaint();}
                std::fprintf(stderr,"RESAMPLE STOCK_ERROR %s\n",reply.error);
            }
        }
    }
    bool shown=visible();if(active.load()&&!shown)close_dialog();
    ready.store(current&&!blocked&&!active.load()&&!handoff&&!resample_stock_backend_busy(current)&&shown);
    return active.load()||ready.load()||handoff;
}
extern "C" int resample_ui_active(){return active.load()!=0;}
extern "C" int resample_ui_light(unsigned pad,uint64_t*out){if(!out||pad>=16||!palette_active.load())return 0;*out=palette[pad].load();return 1;}
extern "C" uint64_t resample_ui_palette_generation(){return palette_epoch.load();}
extern "C" int resample_ui_take_sequence(){return 0;}
extern "C" void resample_ui_fault(){fault.store(1);}
extern "C" void resample_ui_prime_eq(){auto*b=backend.load();if(b&&!active.load()&&!resample_stock_backend_busy(b))resample_stock_backend_bind_eq(b);}
extern "C" void resample_ui_draw(const resample_painter*p){
    if(!p||!active.load())return;
    if(stop_phase){
        p->rect(p->user,0,5,320,235,0xff000000);
        p->text(p->user,stop_phase==1?"OK":"Stopping...",0,7,106,24,15,0xffffffff,1);
        p->text(p->user,"Cancel",212,7,108,24,15,0xffff4040,1);
        p->text(p->user,"Stop playback?",8,75,304,30,23,0xffffffff,0);
        p->text(p->user,"Resampling starts after transport stops",8,124,304,36,14,0xffaaaaaa,0);return;
    }
    if(capturing){p->rect(p->user,0,5,320,235,0xff000000);p->text(p->user,"Cancel",0,7,106,24,15,0xffff4040,1);p->text(p->user,"Reading source...",8,95,304,30,22,0xffffffff,0);return;}
    resample_destination_paint(&dialog,p);
}
static void rect(void*g,int x,int y,int w,int h,uint32_t colour){reinterpret_cast<void(*)(void*,uint32_t)>(base+0x102d5e0)(g,colour);reinterpret_cast<void(*)(void*,int,int,int,int)>(base+0x102e000)(g,x,y,w,h);}
static void text(void*g,const char*s,int x,int y,int w,int h,unsigned size,uint32_t colour,int centre){
    uint64_t string=0,font=0;reinterpret_cast<void(*)(void*,const char*)>(base+0xf535c0)(&string,s);reinterpret_cast<void(*)(void*,int,float)>(base+0x1082810)(&font,0,(float)size);reinterpret_cast<void(*)(void*,const void*)>(base+0x102dca0)(g,&font);reinterpret_cast<void(*)(void*,uint32_t)>(base+0x102d5e0)(g,colour);
    reinterpret_cast<void(*)(void*,const void*,int,int,int,int,int,int,float)>(base+0x10817b0)(g,&string,x,y,w,h,centre?36:33,1,.75f);reinterpret_cast<void(*)(void*)>(base+0x1070d60)(&font);reinterpret_cast<void(*)(void*)>(base+0xf0a290)(&string);
}
extern "C" void resample_ui_paint(void*p,void*g){if(root_id.load()!=reinterpret_cast<uintptr_t>(p)){root=p;root_id.store(reinterpret_cast<uintptr_t>(p));dead.store(0);}resample_painter painter{g,rect,text};resample_ui_draw(&painter);}
