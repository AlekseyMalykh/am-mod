/* Cold-start hook of the pinned native shutdown question. The native No/Yes
 * dialog is retained. Yes cancels the native engine shutdown, first saving
 * a durable SD copy. Only the host supervisor can then request OS poweroff. */
#include "power_control.h"
#include "project_exit.h"
extern "C" {
#include "workflow_ui.h"
}
#include <atomic>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

static uintptr_t base;
static int (*question)(void*);
enum {IDLE,PENDING,SAVING,REQUESTED,FAILED};
static std::atomic<int> state{IDLE};
static std::atomic<bool> stop_pending{false};
static std::atomic<int> button_action{0};
static unsigned char owned_buttons[112]; // Button producer only.
static char error[160];
extern "C" int power_control_operation_busy(void);
extern "C" int power_control_wake(void);
static void *surface; // UI only, while the modal blocks screen changes.
#define FN(o,T) reinterpret_cast<T>(base+(o))
#ifndef MPC_POWER_REQUEST
#define MPC_POWER_REQUEST "/run/mpc-trial-app/power-off.request"
#endif
extern "C" int power_control_busy(void){return state.load()!=IDLE;}
static int confirm(void *environment){
    if(power_control_busy())return 0;
    if(!question(environment))return 0;
    if(!power_control_wake()){
        std::fprintf(stderr,"POWER_BUTTON QUESTION_WITHOUT_UI_CONTEXT shutdown_cancelled\n");
        return 0;
    }
    int expected=IDLE;
    if(state.compare_exchange_strong(expected,PENDING)){
        stop_pending.store(true);
        std::fprintf(stderr,"POWER_BUTTON CONFIRMED_SD_COPY_PENDING\n");
    }
    return 0; // Keep the native engine alive until its save futures finish.
}
extern "C" int power_control_take_stop(void){return !power_control_operation_busy()&&stop_pending.exchange(false);}
extern "C" void power_control_tick(void){
    int s=state.load();
    int action=button_action.exchange(0);
    if(s==FAILED&&action==1){
        state.store(IDLE);
        if(surface)FN(0x1149880,void(*)(void*,uint64_t,uint64_t))(surface,0,*reinterpret_cast<uint64_t*>(static_cast<char*>(surface)+0x28));
        surface=nullptr;
        return;
    }
    if(s==FAILED&&action==2){stop_pending.store(true);state.store(PENDING);s=PENDING;}
    if(s==PENDING){
        if(power_control_operation_busy()){
            stop_pending.store(false);std::snprintf(error,sizeof(error),"Finish copying or resampling first");state.store(FAILED);
        }else if(!project_exit_start(base,workflow_project_services())){
            std::snprintf(error,sizeof(error),"%s",project_exit_message());state.store(FAILED);
        }else state.store(SAVING);
    }else if(s==SAVING){
        if(project_exit_status()==3){
            std::snprintf(error,sizeof(error),"%s",project_exit_message());state.store(FAILED);
        }else if(project_exit_status()==2){
            int fd=open(MPC_POWER_REQUEST,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
            bool ok=fd>=0&&write(fd,"1",1)==1;
            if(fd>=0)close(fd);
            if(ok){state.store(REQUESTED);std::fprintf(stderr,"POWER_BUTTON SD_COPY_READY_HOST_REQUESTED\n");}
            else{std::snprintf(error,sizeof(error),"Could not request power off");state.store(FAILED);}
        }
    }
    if(surface&&power_control_busy())FN(0x1149880,void(*)(void*,uint64_t,uint64_t))(surface,0,*reinterpret_cast<uint64_t*>(static_cast<char*>(surface)+0x28));
}
extern "C" int power_control_button(unsigned code,int down){
    if(code<112&&!down&&owned_buttons[code]){owned_buttons[code]=0;return 1;}
    if(!power_control_busy())return 0;
    if(code<112&&down)owned_buttons[code]=1;
    if(down&&state.load()==FAILED){
        if(code==0x0c)button_action.store(1);
        else if(code==0x0e)button_action.store(2);
    }
    return 1;
}
static void ink(void*g,uint32_t c){FN(0x102d5e0,void(*)(void*,uint32_t))(g,c);}
static void text(void*g,const char*s,int y,int h,float size){
    uint64_t value=0,font=0;
    FN(0xf535c0,void(*)(uint64_t*,const char*))(&value,s);
    FN(0x1082810,void(*)(uint64_t*,int,float))(&font,0,size);
    FN(0x102dca0,void(*)(void*,const uint64_t*))(g,&font);
    FN(0x10817b0,void(*)(void*,const uint64_t*,int,int,int,int,int,int,float))(g,&value,5,y,310,h,0x24,1,.75f);
    FN(0x1070d60,void(*)(uint64_t*))(&font);FN(0xf0a290,void(*)(uint64_t*))(&value);
}
extern "C" void power_control_paint(void*p,void*g){
    int s=state.load();if(s==IDLE)return;
    surface=p;
    ink(g,0xff000000);FN(0x102e000,void(*)(void*,int,int,int,int))(g,0,0,320,240);
    ink(g,s==FAILED?0xffff4040:0xffffc247);
    text(g,s==FAILED?"Power off cancelled":s==REQUESTED?"Shutting down":"Saving project copy",68,28,20);
    ink(g,0xffffffff);
    if(s==FAILED){text(g,"B1: back   B3: retry",6,23,14);text(g,error,111,45,13);text(g,"The device is still on",177,22,14);}
    else{text(g,"Keep the SD card inserted",116,24,15);text(g,"Please wait",168,22,14);}
}
extern "C" int power_control_install(uintptr_t b){
    if(question)return base==b;
    if(!b||sysconf(_SC_PAGESIZE)!=4096)return 0;
    const uint32_t expected[]={0xd503233f,0xd106c3ff,0xaa0003e2,0xb002e3c1};
    void *entry=reinterpret_cast<void*>(b+0x14eb910);
    if(std::memcmp(entry,expected,16))return 0;
    void *tr=mmap(nullptr,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(tr==MAP_FAILED)return 0;
    /* Relocate the displaced ADRP x1 explicitly. Native continuation starts
     * at14eb920; no other PC-relative instruction is copied. */
    const uint32_t code[]={0xd503245f,expected[0],expected[1],expected[2],
                           0x58000081,0x580000b0,0xd61f0200,0xd503201f};
    std::memcpy(tr,code,sizeof(code));
    *reinterpret_cast<uintptr_t*>(static_cast<char*>(tr)+32)=b+0x7164000;
    *reinterpret_cast<uintptr_t*>(static_cast<char*>(tr)+40)=b+0x14eb920;
    __builtin___clear_cache(static_cast<char*>(tr),static_cast<char*>(tr)+48);
    if(mprotect(tr,4096,PROT_READ|PROT_EXEC)){munmap(tr,4096);return 0;}
    void *page=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(entry)&~uintptr_t(4095));
    if(mprotect(page,4096,PROT_READ|PROT_WRITE)){munmap(tr,4096);return 0;}
    base=b;question=reinterpret_cast<int(*)(void*)>(tr);
    const uint32_t jump[]={0x58000050,0xd61f0200};std::memcpy(entry,jump,8);
    *reinterpret_cast<uintptr_t*>(static_cast<char*>(entry)+8)=reinterpret_cast<uintptr_t>(confirm);
    __builtin___clear_cache(static_cast<char*>(entry),static_cast<char*>(entry)+16);
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))_exit(121);
    std::fprintf(stderr,"POWER_BUTTON NATIVE_QUESTION_INSTALLED\n");return 1;
}
