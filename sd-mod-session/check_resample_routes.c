/* Compile the real production routing, recording only native receivers/UI. */
#define PHONE_RGB_TEST 1
#define MPC_RESAMPLE_TRIAL 1
#include "src/phone_native.c"
#include <assert.h>
static unsigned button_calls,pad_calls,control_calls,transport,ui_pads,ui_faults,ui_active,ui_shift,seq_request;
static unsigned char recorded_pad[2][40];
static uint64_t ui_ticket=27;
int resample_ui_active(void){return ui_active;}
void resample_ui_requested(unsigned code,int down){if(code==0x31)ui_shift=!!down;}
int resample_ui_owns(unsigned kind,unsigned code){return ui_active&&(kind!=PHONE_CONTROLLER||code!=0x13);}
int resample_ui_claim_pad(unsigned pad,int down,int blocked){if(ui_active)return 1;if(pad==10&&down&&ui_shift&&!blocked){ui_active=1;return 1;}return 0;}
uint64_t resample_ui_ticket(void){return ui_ticket;}
void resample_ui_pad(unsigned pad,int down,uint64_t t){assert(pad==10&&t==ui_ticket);(void)down;++ui_pads;}
void resample_ui_fault(void){++ui_faults;}
int resample_ui_take_sequence(void){unsigned value=seq_request;seq_request=0;return value;}
int note_native_active(void){return 0;}
int note_native_owns(unsigned cc){(void)cc;return 0;}
void note_native_button_requested(unsigned c,unsigned d){(void)c;(void)d;}
int music_ui_active(void){return 1;} /* Ordinary Sample is active, not modal. */
int music_ui_owns(unsigned cc){(void)cc;return 0;}
int music_ui_button_owns(unsigned c){(void)c;return 0;}
int step_native_active(void){return 0;}
int step_native_owns(unsigned c){(void)c;return 0;}
int step_native_button_owns(unsigned c){(void)c;return 0;}
uint64_t step_native_ticket(void){return 19;}
int step_native_pad_route(uint64_t t){assert(t==19);return 0;}
void step_native_requested(unsigned c,unsigned d){(void)c;(void)d;}
void step_native_queue_fault(void){}
int workflow_active(void){return 1;} /* Chop observer is active on normal Sample. */
int workflow_owns(unsigned kind,unsigned c){(void)kind;(void)c;return 0;}
uint64_t workflow_ticket(void){return 23;}
int workflow_button(void*e,uint64_t t){(void)e;(void)t;return 0;}
void workflow_fault(void){}
static void receive_button(void*e){unsigned c=FIELD(e,8,unsigned char),d=FIELD(e,9,unsigned char);++button_calls;if((c==0x51||c==0x52)&&d)++transport;}
static void receive_control(void*e){assert(FIELD(e,8,unsigned char)==0x13);++control_calls;}
static void receive_pad(void*e){assert(pad_calls<2);memcpy(recorded_pad[pad_calls++],e,40);}
static int midi_down(void*p,int unused){assert(!unused);return FIELD(p,0,unsigned char)==1;}
static int midi_up(void*p,int unused){assert(unused==1);return FIELD(p,0,unsigned char)==0;}
static void route_stub(uintptr_t off,void*fn){uint32_t words[]={0x58000050,0xd61f0200};memcpy((void*)(base+off),words,8);memcpy((void*)(base+off+8),&fn,8);}
int main(void){
 void*m=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(m!=MAP_FAILED);base=(uintptr_t)m;
 route_stub(phone_hooks[H_BUTTONS].trampoline,receive_button);route_stub(phone_hooks[H_CONTROLLER].trampoline,receive_control);route_stub(phone_hooks[H_PAD].trampoline,receive_pad);route_stub(0xddfed0,midi_down);route_stub(0xddff20,midi_up);__builtin___clear_cache(m,(char*)m+0x7500000);
 unsigned char key[16]={};struct phone_event e;key[8]=0x31;key[9]=1;buttons(key);assert(ui_shift&&!button_calls);
 assert(phone_events_pop(&e)&&e.kind==PHONE_BUTTON&&!e.pad);forward_buttons(e.native);assert(button_calls==1);
 unsigned char pad[40]={};pad[8]=10;pad[16]=1;FIELD(pad,32,int)=3;pad[18]=100;pad_event(pad);
 assert(ui_active&&atomic_load(&consumed_resample)==(1u<<10)&&!pad_calls);
 assert(phone_events_pop(&e)&&e.kind==PHONE_RESAMPLE_PAD&&e.pad==10&&e.down&&e.workflow_ticket==27&&!memcmp(e.native,pad,16)&&!memcmp(e.midi,pad+16,24));resample_pad_ui(&e);assert(resample_entry_mask==1&&ui_pads==1);
 /* Seq choice cannot replay the gesture while physical pad11 is held. */
 ui_active=0;seq_request=1;resample_sequence_tick();assert(!pad_calls&&atomic_load(&resample_sequence_pending));
 pad[16]=0;pad_event(pad);assert(!atomic_load(&consumed_resample));assert(phone_events_pop(&e)&&e.up&&!e.down);resample_pad_ui(&e);assert(resample_entry_mask==3&&ui_pads==2);
 key[9]=0;buttons(key);assert(!ui_shift&&button_calls==1);assert(phone_events_pop(&e)&&e.kind==PHONE_BUTTON&&!e.pad);forward_buttons(e.native);assert(button_calls==2);
 /* Native Shift already released: inject its exact captured pair around pad. */
 resample_sequence_tick();assert(pad_calls==2&&button_calls==4&&!atomic_load(&resample_sequence_pending));
 assert(recorded_pad[0][8]==10&&recorded_pad[0][16]==1&&recorded_pad[1][16]==0&&!memcmp(recorded_pad[1],pad,40));
 assert(!atomic_load(&native_buttons_down[0x31]));
 /* While open, Play/Stop and master bypass all modal FIFOs. */
 ui_active=1;key[8]=0x51;key[9]=1;buttons(key);key[9]=0;buttons(key);key[8]=0x52;key[9]=1;buttons(key);key[9]=0;buttons(key);assert(transport==2&&!phone_events_pending());
 key[8]=0x13;controller(key);assert(control_calls==1&&!phone_events_pending());
 key[8]=0x10;key[9]=67;controller(key);assert(phone_events_pop(&e)&&e.kind==PHONE_CONTROLLER&&e.pad==16&&e.workflow_ticket==27&&e.native[9]==67);
 key[8]=0x0d;key[9]=1;buttons(key);assert(phone_events_pop(&e)&&e.kind==PHONE_BUTTON&&e.pad==16&&e.workflow_ticket==27);
 assert(!ui_faults&&!phone_events_pending());munmap(m,0x7500000);
 puts("PASS production routes: ordinary music UI active does not block Sample entry; exact inline pad11 payload and paired release, FIFO owner/ticket, Seq waits for release and replays original Shift gesture after UI close, Play/Stop and Main Volume bypass modal queue");return 0;
}
