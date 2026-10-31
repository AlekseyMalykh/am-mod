#define RUNTIME_PROOF_MAIN runtime_proof_main
#include "check_resample_runtime.c"
#include "src/resample_ui.h"
static unsigned repaints,texts,rectangles;
static char painted[8192];
static FILE*draw_trace;
static unsigned native_columns;
static int ui_visible(void*p){assert(p);return 1;}
static int ui_blocked(void*p){assert(p);return 0;}
static int ui_empty_priority_queue(void*p,uintptr_t tag){assert(p==owner_queue+8&&tag==((uintptr_t)(owner_context+0x48)&~(uintptr_t)3));return 0;}
static void ui_repaint(void*p,uint64_t a,uint64_t b){assert(p&&!a&&!b);++repaints;}
static void ui_rect(void*p,int x,int y,int w,int h,uint32_t colour){
 (void)p;assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240);assert((colour>>24)==255);if(!rectangles)assert(colour==0xff000000u);++rectangles;
 if(draw_trace)fprintf(draw_trace,"R %d %d %d %d %08x\n",x,y,w,h,colour);
}
static void ui_text(void*p,const char*s,int x,int y,int w,int h,unsigned size,uint32_t colour,int centre){
 (void)p;assert(s&&x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240&&size>0);
 if(draw_trace)fprintf(draw_trace,"T %d %d %d %d %u %08x %d %s\n",x,y,w,h,size,colour,centre,s);
 if(y==157&&size==17){assert(centre&&w<=102&&(x==3||x==109||x==215));native_columns|=1u<<(x/106);}
 assert(strlen(painted)+strlen(s)+2<sizeof(painted));strcat(painted,s);strcat(painted,"\n");++texts;
}
static void ui_draw(void){painted[0]=0;texts=rectangles=0;struct resample_painter p={NULL,ui_rect,ui_text};resample_ui_draw(&p);assert(texts&&rectangles);}
static void ui_drive(void){
 for(unsigned i=0,idle=0;i<1000;++i){
  if(native_queue_pending()){FN(0x2b59200,void(*)(void*,uint64_t*))(owner_queue+0x1d8,&queue_cursor);idle=0;}
  else ++idle;
  resample_ui_tick(0);if(idle>3)return;
 }
 assert(!"UI owner queue did not settle");
}
static void ui_key(unsigned code,int down){
 unsigned char event[16]={};event[8]=code;event[9]=down;
 resample_ui_requested(code,down);int handled=resample_ui_button(event,resample_ui_ticket());
 assert(handled==(code!=0x31&&code!=0x51&&code!=0x52));
}
static void ui_button(unsigned button){ui_key(0x0c+button,1);ui_key(0x0c+button,0);ui_drive();}
static void ui_knob(unsigned cc,unsigned value){unsigned char event[16]={};event[8]=cc;event[9]=value;assert(resample_ui_control(event,resample_ui_ticket()));ui_drive();}
static void ui_open(void){
 resample_ui_tick(0);resample_ui_requested(0x31,1);assert(resample_ui_claim_pad(10,1,0));
 assert(resample_ui_active()&&resample_ui_owns(1,0x10)&&!resample_ui_owns(1,0x13));
 resample_ui_pad(10,1,resample_ui_ticket());resample_ui_pad(10,0,resample_ui_ticket());ui_drive();ui_key(0x31,0);
}
int main(int argc,char**argv){
 assert(argc==2||argc==3);assert(!runtime_proof_main(2,argv));phase="physical resample UI over real owner runtime";
 source_refs=1;entries=enters=0;repaints=0;
 FN(0x292e310,void(*)(void*))(real_pool);FN(0xf535c0,void(*)(void*,const char*))(real_program+0xdc8,"ui-project");
 uintptr_t original[2]={};new_sample("UISource",original);assert(pad_sample_pool_register(base,real_pool,original));
 FIELD(source_layers[0],0x4a8,uintptr_t)=original[0];FIELD(source_layers[0],0x4b0,uintptr_t)=original[1];FIELD(source_layers[0],0x7b8,int64_t)=0;FIELD(source_layers[0],0x810,int64_t)=16;
 for(unsigned i=0;i<8;++i)FIELD(commit_layers[i],0x418,int)=60+i;
 native_context_fields();stub(0x112fd50,ui_visible);stub(0x112f6b0,ui_blocked);stub(0x1149880,ui_repaint);
 unsigned char screen[0x200]={};FIELD(screen,0,uintptr_t)=base+0x6e212a0;
 resample_ui_init(base);resample_ui_bind(owner_context);resample_ui_paint(screen,NULL);assert(resample_ui_tick(0));
 assert(!resample_ui_claim_pad(10,1,0));resample_ui_requested(0x31,1);
 assert(!resample_ui_claim_pad(10,1,1)&&!resample_ui_claim_pad(9,1,0));resample_ui_requested(0x31,0);
 ui_open();ui_draw();assert(strstr(painted,"Sample"));
 ui_key(0x51,1);ui_key(0x51,0);ui_key(0x52,1);ui_key(0x52,0);
 unsigned char volume[16]={};volume[8]=0x13;volume[9]=80;assert(!resample_ui_control(volume,resample_ui_ticket()));
 ui_button(2);ui_button(1);ui_knob(0x10,127);ui_button(2);
 ui_draw();assert(strstr(painted,"Choose effect")&&strstr(painted,"Category"));ui_button(2);
 draw_trace=argc>2?fopen(argv[2],"w"):NULL;ui_draw();if(draw_trace){fclose(draw_trace);draw_trace=NULL;}
 assert(strstr(painted,"Frequency")&&strstr(painted,"Gain")&&strstr(painted,"Hz"));
 ui_knob(0x12,0);ui_draw();assert(strstr(painted,"0.20"));ui_knob(0x10,127);ui_knob(0x11,100);ui_knob(0x64,1);
 ui_button(1);ui_draw();assert(strstr(painted,"Add point"));ui_button(1);ui_draw();assert(strstr(painted,"5/8 points"));ui_button(0);
 ui_key(0x31,1);ui_knob(0x10,0);ui_knob(0x11,127);ui_key(0x31,0);
 ui_button(2);ui_draw();assert(strstr(painted,"Enable"));ui_button(2);ui_button(0);ui_draw();assert(strstr(painted,"FX 1/5"));
 /* Empty slot asks what to insert; native parameters use actual complete
  * catalogue and do not silently add a second EQ. Remove before rendering
  * because this UI fixture intentionally has no device service registry. */
 ui_knob(0x10,127);ui_button(2);ui_knob(0x10,80);ui_draw();assert(strstr(painted,"Tube Drive"));ui_button(2);
 if(argc>2){char path[1024];snprintf(path,sizeof(path),"%s.native",argv[2]);draw_trace=fopen(path,"w");assert(draw_trace);}
 ui_draw();if(draw_trace){fclose(draw_trace);draw_trace=NULL;}assert(strstr(painted,"Headroom"));
 assert(native_columns==7);ui_knob(0x11,100);ui_knob(0x64,1);ui_draw();assert(strstr(painted,"Output"));
 /* Physical Shift+B2/B3 also moves while its parameter editor is open.
  * Both pages and the complete settings follow the selected processor. */
 ui_key(0x31,1);ui_button(1);ui_draw();assert(strstr(painted,"Tube Drive")&&strstr(painted,"Up")&&strstr(painted,"Down"));
 ui_button(1);ui_draw();assert(strstr(painted,"Already first in chain"));ui_button(2);ui_key(0x31,0);
 ui_button(0);ui_draw();assert(strstr(painted,"FX 2/5"));
 ui_key(0x09,1);ui_key(0x09,0);ui_draw();assert(strstr(painted,"Remove selected effect"));ui_button(2);ui_draw();assert(strstr(painted,"FX 1/5"));ui_button(0);
 ui_button(2);ui_draw();assert(strstr(painted,"Result"));
 /* Absolute K1 selects To Pad, K2 E bank, K3 second pad. */
 ui_knob(0x10,127);ui_knob(0x11,64);ui_knob(0x12,8);ui_button(2);
 assert(!resample_ui_active()&&entries==1&&enters==1&&repaints&&!FIELD(owner_context,0x50,unsigned));
 assert(!run_history(history_entry,9)&&!FIELD(commit_layers[0],0x4a8,void*));assert(!run_history(history_entry,8));assert(!run_history(history_entry,9));pad_assignment_release(base,history_entry);
 /* Seq choice closes owner resources before handing off its request. */
 ui_open();ui_button(1);assert(!resample_ui_active()&&resample_ui_take_sequence()&&!resample_ui_take_sequence());
 /* Cancellation owns its delayed button-up even after the window closes. */
 ui_open();ui_key(0x0c,1);ui_drive();assert(!resample_ui_active()&&resample_ui_owns(2,0x0c));ui_key(0x0c,0);assert(!resample_ui_owns(2,0x0c));
 /* A project change invalidates producer ticket before its queued pad reaches UI. */
 resample_ui_tick(0);resample_ui_requested(0x31,1);assert(resample_ui_claim_pad(10,1,0));uint64_t stale=resample_ui_ticket();
 resample_ui_project_changing();resample_ui_pad(10,1,stale);ui_drive();assert(!resample_ui_active()&&entries==1);resample_ui_requested(0x31,0);
 /* Context dies after producer claim but before UI captures it. */
 resample_ui_tick(0);resample_ui_requested(0x31,1);assert(resample_ui_claim_pad(10,1,0));stale=resample_ui_ticket();
 resample_ui_context_dead(owner_context);resample_ui_pad(10,1,stale);ui_drive();assert(!resample_ui_active());resample_ui_requested(0x31,0);
 resample_ui_context_dead(owner_context);FN(0x1517c70,void(*)(void*))(owner_context);resample_ui_tick(0);
 /* Native CopyContext destructor cancels a posted boot closure without
  * invoking it. Only the separate unused priority queue is a zero fixture;
  * cancellation in the real four-slot queue and counter subtraction are native.
  * UI retains that orphan runtime until process exit, never retries freed owner. */
 native_context_fields();resample_ui_bind(owner_context);resample_ui_tick(0);resample_ui_requested(0x31,1);
 assert(resample_ui_claim_pad(10,1,0));resample_ui_pad(10,1,resample_ui_ticket());assert(FIELD(owner_context,0x50,unsigned)==1);
 stub(0x2b55650,ui_empty_priority_queue);resample_ui_context_dead(owner_context);FN(0x1517c70,void(*)(void*))(owner_context);
 assert(!FIELD(owner_context,0x50,unsigned));memset(owner_context,0xa5,sizeof(owner_context));resample_ui_tick(0);assert(!resample_ui_active());resample_ui_requested(0x31,0);
 /* Active prepared source + queued Render, not just boot. Both context and
  * screen may die before the UI timer; newly arriving B1/K1 must not wake its
  * cancelled owner, including with the new immediate-close implementation. */
 native_context_fields();resample_ui_bind(owner_context);ui_open();ui_button(2);
 ui_key(0x0e,1);ui_key(0x0e,0);assert(FIELD(owner_context,0x50,unsigned)==1);
 resample_ui_context_dead(owner_context);FN(0x1517c70,void(*)(void*))(owner_context);assert(!FIELD(owner_context,0x50,unsigned));
 memset(owner_context,0xa5,sizeof(owner_context));ui_key(0x0c,1);ui_key(0x0c,0);
 unsigned char control[16]={};control[8]=0x10;control[9]=100;assert(resample_ui_control(control,resample_ui_ticket()));
 /* No retained UI pointer is touched after native screen destruction. */
 resample_ui_destroyed(screen);memset(screen,0xa5,sizeof(screen));resample_ui_tick(0);assert(!resample_ui_claim_pad(10,1,0));
 memset(source_layers,0,sizeof(source_layers));assert(pad_sample_pool_remove(base,real_pool,original));FN(0x29348b0,void(*)(void*))(real_pool);release_sample(original);FN(0xf0a290,void(*)(void*))(real_program+0xdc8);memory_release(pcm);
 puts("PASS physical UI Shift+pad11 claim -> chooser -> Sample -> EQ editor -> render -> free E02 Apply and native Undo/Redo; bounded text geometry, absolute knobs/relative encoder, bypass/band/type, Play/Stop/Main Volume passthrough, Seq handoff after close, delayed button-up, stale project ticket, context death before UI capture and poisoned destroyed screen; native CopyContext destructor cancels posted boot and active prepared-source Render through real queue and balances counter (separate priority queue empty fixture); late B1/K1 and simultaneous screen destruction never retry poisoned owner; orphan retained until process exit, no hardware test");return 0;
}
