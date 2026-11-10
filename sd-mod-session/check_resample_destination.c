/* Native destination callback + simple UI state machine. No audio devices. */
#define main unused_eq_probe_main
#include "check_fx_native.c"
#undef main
#include "src/resample_destination.h"
static struct resample_destination state;
static struct resample_destination_request sent;
static unsigned starts,cancels,busy,cancel_busy,native_target,native_starts;
static uint64_t valid_epoch=77;
static unsigned char native_mode[0x158];
static unsigned char native_queue[0x200],queued_function[40];
static uintptr_t queue_vtable[3];
static unsigned queue_calls;
static int start_request(void*user,const struct resample_destination_request*r){
    assert(user==&state&&r->token&&r->target_pad<128&&r->source_pad==18);
    if(busy||r->source_epoch!=valid_epoch)return 0;
    sent=*r;++starts;return 1;
}
static int cancel_request(void*user,uint64_t token){assert(user==&state&&token==sent.token);++cancels;return !cancel_busy;}
static void button(unsigned key){assert(resample_destination_button(&state,key,1));assert(resample_destination_button(&state,key,0));}
static void open_dialog(int sample,int sequence){uint64_t used[2]={UINT64_C(1)<<3,0};assert(resample_destination_open(&state,18,"Drum","Verse",sample,sequence,valid_epoch,used));}
static FILE*trace;
static void rect(void*u,int x,int y,int w,int h,uint32_t colour){assert(u==&state&&x>=0&&y>=5&&w>0&&h>0&&x+w<=320&&y+h<=240);fprintf(trace,"R %d %d %d %d %08x\n",x,y,w,h,colour);}
static void text(void*u,const char*s,int x,int y,int w,int h,unsigned size,uint32_t colour,int centre){assert(u==&state&&x>=0&&y>=5&&w>0&&h>0&&x+w<=320&&y+h<=240&&size>=11);assert(!strstr(s,"Compare")&&!strstr(s,"FX"));fprintf(trace,"T %d %d %d %d %u %08x %d %s\n",x,y,w,h,size,colour,centre,s);}
static void paint(const char*folder,const char*name){char file[1024];snprintf(file,sizeof(file),"%s/%s.txt",folder,name);trace=fopen(file,"w");assert(trace);struct resample_painter p={&state,rect,text};resample_destination_paint(&state,&p);fclose(trace);}
static void capture_native_start(void*mode,int target){assert(mode==native_mode&&target>=0&&target<128);native_target=(unsigned)target;++native_starts;}
static void replace_native(uintptr_t at,void*callback){uint32_t jump[]={0x58000050,0xd61f0200};memcpy((void*)(base+at),jump,8);memcpy((void*)(base+at+8),&callback,8);__builtin___clear_cache((char*)(base+at),(char*)(base+at+16));}
static void*no_thread_queue(unsigned key){(void)key;return NULL;}
static int queue_destination(void*queue,void*function,uintptr_t tag){
    assert(queue==native_queue+0x30&&tag==((uintptr_t)(native_mode+0x78)&~(uintptr_t)3));
    memcpy(queued_function,function,sizeof(queued_function));++queue_calls;return 1;
}
int main(int argc,char**argv){
    assert(argc==3);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);sigaction(SIGILL,&sa,NULL);load(argv[1]);
    struct resample_destination_backend backend={&state,start_request,cancel_request};resample_destination_init(&state,&backend);
    open_dialog(1,1);paint(argv[2],"choose");button(2);assert(state.page==RD_TARGET&&!starts&&state.target_pad==128);paint(argv[2],"target-empty");
    button(2);assert(!starts&&strstr(state.error,"Choose"));assert(!resample_destination_target(&state,128));
    assert(resample_destination_bank(&state,1));assert(resample_destination_pad(&state,2,1));assert(resample_destination_pad(&state,2,0));assert(state.target_pad==18);
    paint(argv[2],"target-source");assert(resample_destination_button(&state,2,1)&&state.page==RD_CONFIRM&&!starts);paint(argv[2],"confirm");
    assert(resample_destination_button(&state,2,1)&&!starts);assert(resample_destination_button(&state,2,0));
    button(0);assert(state.page==RD_TARGET&&!starts);button(2);button(2);
    assert(starts==1&&sent.kind==RESAMPLE_SOUND&&sent.source_pad==sent.target_pad&&sent.overwrite_confirmed&&state.page==RD_RUNNING);
    resample_destination_progress(&state,sent.token,25,100);paint(argv[2],"running");resample_destination_progress(&state,sent.token,10,100);assert(state.done==25);
    resample_destination_progress(&state,sent.token+1,100,100);assert(state.done==25);resample_destination_finished(&state,sent.token+1,1,NULL);assert(state.page==RD_RUNNING);
    cancel_busy=1;resample_destination_cancel(&state);assert(state.page==RD_CANCELLING&&state.cancel_retry&&cancels==1);cancel_busy=0;resample_destination_retry_cancel(&state);assert(!state.cancel_retry&&cancels==2);
    resample_destination_finished(&state,sent.token,1,NULL);assert(state.page==RD_CLOSED&&state.applied); /* Commit won a late Cancel. */
    open_dialog(0,1);button(2);assert(state.page==RD_SOURCE&&!state.applied&&strstr(state.error,"empty"));button(1);assert(state.page==RD_TARGET&&state.kind==RESAMPLE_SEQUENCE);
    for(unsigned target=0;target<128;++target){assert(resample_destination_bank(&state,target/16));assert(resample_destination_pad(&state,target%16,1));assert(state.target_pad==target);assert(resample_destination_pad(&state,target%16,0));}
    assert(resample_destination_bank(&state,7)&&state.target_pad==128);assert(!resample_destination_bank(&state,8));assert(resample_destination_target(&state,127));paint(argv[2],"seq-target");
    busy=1;button(2);assert(state.page==RD_TARGET&&starts==1);busy=0;button(2);assert(state.page==RD_RUNNING&&starts==2&&sent.kind==RESAMPLE_SEQUENCE&&sent.target_pad==127&&!sent.overwrite_confirmed);
    uint64_t old_token=sent.token;resample_destination_cancel(&state);resample_destination_finished(&state,old_token,0,NULL);assert(state.page==RD_CLOSED&&!state.applied);
    open_dialog(1,1);button(2);assert(resample_destination_target(&state,3));button(2);assert(state.page==RD_CONFIRM);button(0);assert(resample_destination_target(&state,4));button(2);assert(sent.target_pad==4&&!sent.overwrite_confirmed);
    resample_destination_finished(&state,old_token,1,NULL);assert(state.page==RD_RUNNING);resample_destination_finished(&state,sent.token,0,"Native render failed");assert(state.page==RD_ERROR);paint(argv[2],"error");button(0);assert(state.page==RD_CLOSED);
    open_dialog(1,1);button(2);assert(resample_destination_target(&state,5));++valid_epoch;unsigned before=starts;button(2);assert(starts==before&&state.page==RD_TARGET);button(0);
    open_dialog(1,0);button(1);assert(state.page==RD_SOURCE&&strstr(state.error,"sequence"));button(0);assert(state.page==RD_CLOSED);
    open_dialog(1,1);assert(resample_destination_button(&state,0,1)&&state.page==RD_CLOSED);
    assert(resample_destination_button(&state,0,1));assert(resample_destination_button(&state,0,0));assert(!resample_destination_button(&state,0,1));
    puts("PASS destination controller: Seq/Sample choice without rendering, explicit target for128 pads, same-source/occupied confirmation, held-key protection, no implicit first-free, late completion/Cancel, queue failure/retry, source epoch rejection and black320x240 grouped layout; no FX editor or comparison");
    phase="native queued sequence destination";replace_native(0x153ed50,capture_native_start);
    /* Execute real153f1c0. Its queue payload retains the selected target at10,
     * invokes the real entry ABI and balances the owner's queue counter. The
     * entry itself is a capture boundary: no renderer/device setup runs. */
    for(unsigned target=0;target<128;++target){uint64_t owner[2]={0,1};struct{void*counter;void*mode;int target;int padding;} payload={owner,native_mode,(int)target,0};
        FN(0x153f1c0,void(*)(void*))(&payload);assert(native_target==target&&*(unsigned*)((char*)owner+8)==0);}
    assert(native_starts==128);
    puts("PASS actual native153f1c0 forwards every destination0..127 unchanged and balances owner counter; render/assignment/audio boundaries are not executed");
    phase="native pad behaviour enqueues destination";replace_native(0xb53508,no_thread_queue);
    queue_vtable[2]=(uintptr_t)queue_destination;*(void**)(native_queue+0x30)=queue_vtable;*(void**)(native_mode+0x78)=native_queue;
    for(unsigned target=0;target<128;++target){assert(!*(unsigned*)(native_mode+0x80));
        FN(0x153eab0,void(*)(void*,unsigned))(native_mode,target);assert(*(unsigned*)(native_mode+0x80)==1);
        ((void(*)(void*))*(uintptr_t*)queued_function)(queued_function+16);
        assert(native_target==target&&!*(unsigned*)(native_mode+0x80));}
    assert(queue_calls==128&&native_starts==256);
    puts("PASS actual native153eab0 builds and dispatches selected-pad payload through its owner queue,153f1c0 invokes entry with the same target, owner counter balances; queue transport and final render entry are capture fixtures");return 0;
}
