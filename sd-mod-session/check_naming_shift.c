/* Production button handler with a destructive native Shift/cache fixture.
 * Text model and glyph receivers are fixtures; no physical screen claim. */
#define PHONE_RGB_TEST 1
#include "src/phone_native.c"
#include "src/phone_events.c"
#include <assert.h>
static unsigned native_calls,rebuilds,invalidates,other_routes,claim_background;
static unsigned musical_calls,musical_pads[8],musical_edges[8];
static void native_musical_pad(void*e){assert(musical_calls<8);musical_pads[musical_calls]=FIELD(e,8,unsigned char);musical_edges[musical_calls]=FIELD(e,16,unsigned char);++musical_calls;}
static char text_buffer[513];static uint64_t glyphs[128];
static char painted_name[600];static unsigned paints;
static void original_paint(void*p,void*g){assert(p==editor&&g==(void*)1);++paints;}
static void paint_colour(void*g,uint32_t colour){assert(g==(void*)1);(void)colour;}
static void paint_rect(void*g,int x,int y,int w,int h){assert(g==(void*)1&&x>=0&&y>=0&&w>0&&h>0&&x+w<=312&&y+h<=32);}
static void paint_font(uint64_t*p,int style,float size){assert(!style&&size>27.f&&size<28.f);*p=1;}
static void paint_use_font(void*g,const uint64_t*p){assert(g==(void*)1&&*p==1);}
static void paint_end_font(uint64_t*p){assert(*p==1);}
static void paint_text(void*g,const uint64_t*p,int x,int y,int w,int h,int a,int b,float scale){assert(g==(void*)1&&x==7&&y==2&&w==298&&h==28&&a==0x21&&b==1&&scale==1.f);snprintf(painted_name,sizeof(painted_name),"%s",(const char*)(uintptr_t)*p);}
int step_native_active(void){return 0;}
int workflow_button(void*e,uint64_t t){(void)e;(void)t;return 0;}
int step_native_button(void*e,uint64_t t){(void)e;(void)t;++other_routes;return 0;}
int music_ui_button(void*e){(void)e;++other_routes;return 0;}
int note_native_tick(void){++other_routes;return 0;}
int music_ui_tick(void){++other_routes;return 0;}
int music_ui_exit_saving(void){return 0;}
int step_native_owns(unsigned cc){(void)cc;return 0;}
int note_native_owns(unsigned cc){(void)cc;return (int)claim_background;}
int music_ui_owns(unsigned cc){(void)cc;return (int)claim_background;}
int music_ui_button_owns(unsigned cc){(void)cc;return (int)claim_background;}
int step_native_button_owns(unsigned cc){(void)cc;return (int)claim_background;}
uint64_t step_native_ticket(void){return 1;}
int step_native_pad_route(uint64_t ticket){assert(ticket==1);return 0;}
uint64_t workflow_ticket(void){return 2;}
int workflow_owns(unsigned kind,unsigned code){(void)kind;(void)code;return (int)claim_background;}
int step_native_control(void*p,uint64_t t){(void)p;(void)t;return 0;}
int music_ui_control(void*p){(void)p;return 0;}
int note_native_control(void*p){(void)p;return 0;}
int note_native_button(void*p){(void)p;return 0;}
int step_native_tick(void){++other_routes;return 0;}
void step_native_queue_fault(void){}
void workflow_fault(void){}
static int shown(void*p){return p==editor;}
static int unblocked(void*p){(void)p;return 0;}
static void native_shift(void*e){
 unsigned code=FIELD(e,8,unsigned char);++native_calls;
 if(code==0x31){strcpy(text_buffer,"stale");phone_sync(&state,"stale",1);}
 else{assert(code==0x36||code==0x37);if(FIELD(e,9,unsigned char)){
   int at=FIELD(editor,0x2c0,int)+(code==0x36?1:-1);
   if(at<0)at=0;
   if(at>(int)state.length)at=(int)state.length;
   FIELD(editor,0x2c0,int)=at;
 }}
}
static int midi_down(void*p,int flag){assert(!flag);return FIELD(p,0,unsigned char)==1;}
static int midi_up(void*p,int flag){assert(flag==1);return FIELD(p,0,unsigned char)==0;}
static void naming_pad_edge(unsigned pad,int down){
 unsigned char event[40]={0};event[8]=(unsigned char)(pad-1);event[16]=(unsigned char)down;
 struct phone_event received;pad_event(event);
 assert(phone_events_pop(&received)&&received.kind==PHONE_PAD&&received.pad==pad-1&&received.down==(unsigned)down);
 assert(phone_pad(&state,pad,down,received.when_ms));write_native();
}
static void make_string(uint64_t*p,const char*s){*p=(uintptr_t)s;}
static void no_string(uint64_t*p){(void)p;}
static void write_text(void*p,const uint64_t*s,int notify){assert(p==editor&&notify==1);snprintf(text_buffer,sizeof(text_buffer),"%s",(char*)(uintptr_t)*s);}
static void glyph_rebuild(void*p){assert(p==editor);++rebuilds;for(unsigned i=0;i<128;++i){FIELD(glyphs,i*8,int)=(int)i;FIELD(glyphs,i*8+4,unsigned char)=0;}}
static void cursor_range(void*p,const int*r){assert(p==editor&&r[1]==r[0]+!state.enabled);}
static void invalidate_screen(void*p,uint64_t a,uint64_t n){(void)a;(void)n;assert(p==screen);++invalidates;}
static void stub(uintptr_t off,void*fn){uint32_t w[]={0x58000050,0xd61f0200};memcpy((void*)(base+off),w,8);memcpy((void*)(base+off+8),&fn,8);}
int main(void){void*p=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(p!=MAP_FAILED);base=(uintptr_t)p;
 unsigned char naming[0x400]={0},parent[0x40]={0};editor=naming;screen=parent;ui_thread=pthread_self();ui_known=1;
 FIELD(editor,0,uintptr_t)=base+0x6e18e20;FIELD(editor,0x2c8,int)=128;FIELD(editor,0x2e0,void*)=glyphs;FIELD(editor,0x2e8,void*)=glyphs+128;FIELD(editor,0x2d0,void*)=text_buffer;FIELD(editor,0x2c0,int)=4;
 stub(0x112fd50,shown);stub(0x112f6b0,unblocked);stub(0x1149880,invalidate_screen);stub(0x11685a0,cursor_range);stub(0xf535c0,make_string);stub(0xf0a290,no_string);stub(0x11c4ec0,write_text);stub(phone_hooks[H_REBUILD].trampoline,glyph_rebuild);stub(phone_hooks[H_BUTTONS].trampoline,native_shift);__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 phone_init(&state,128);strcpy(text_buffer,"abcd_E#");assert(phone_sync(&state,text_buffer,4));phone_enable(&state,1);state.uppercase=1;
 unsigned char event[16]={0};event[8]=0x31;
 for(unsigned edge=0;edge<2;++edge){event[9]=(unsigned char)!edge;buttons_ui(event,0);assert(!strcmp(text_buffer,"abcd_E#")&&state.cursor==4&&state.uppercase&&state.enabled&&FIELD(editor,0x2c0,int)==4);}
 assert(native_calls==2&&rebuilds==2&&invalidates==2&&!other_routes);
 unsigned char move[16]={0};move[8]=0x64;move[9]=127;controller_ui(move,0);assert(state.cursor==3&&!strcmp(text_buffer,"abcd_E#")&&FIELD(editor,0x2c0,int)==3);
 move[9]=1;controller_ui(move,0);assert(state.cursor==4&&!strcmp(text_buffer,"abcd_E#")&&FIELD(editor,0x2c0,int)==4);
 FIELD(editor,0x28,int)=312;FIELD(editor,0x2c,int)=32;
 stub(0x1134ad0,original_paint);stub(0x102d5e0,paint_colour);stub(0x102e000,paint_rect);stub(0x1082810,paint_font);stub(0x102dca0,paint_use_font);stub(0x1070d60,paint_end_font);stub(0x10817b0,paint_text);
 FIELD((void*)base,0x6e18e20+0xd8,uintptr_t)=base+0x1134ad0;assert(install_name_cursor());
 assert(FIELD((void*)base,0x6e18e20+0xd8,uintptr_t)==(uintptr_t)name_cursor_paint);
 stub(0xddfed0,midi_down);stub(0xddff20,midi_up);__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 name_cursor_paint(editor,(void*)1);assert(paints==1&&!strcmp(painted_name,"abcd|_E#")&&!strcmp(text_buffer,"abcd_E#"));
 /* Run producer ownership selection and real FIFO, with every background
  * screen claiming these same controls. Naming must still own the events. */
 claim_background=1;atomic_store(&naming_active,1);atomic_store(&requested_pads,1);
 struct phone_event queued;
 assert(queue_native(move,PHONE_CONTROLLER)&&phone_events_pop(&queued)&&!queued.pad);
 size_t before_cursor=state.cursor;controller_ui(queued.native,queued.step_ticket);assert(state.cursor==before_cursor+1);
 unsigned calls_before_move=native_calls;
 unsigned char nav[16]={0};nav[8]=0x37;nav[9]=1;
 for(unsigned edge=0;edge<2;++edge){nav[9]=(unsigned char)!edge;assert(queue_native(nav,PHONE_BUTTON)&&phone_events_pop(&queued)&&!queued.pad);buttons_ui(queued.native,queued.step_ticket);}
 assert(state.cursor==before_cursor&&native_calls==calls_before_move&&!strcmp(text_buffer,"abcd_E#"));
 nav[8]=0x36;
 for(unsigned edge=0;edge<2;++edge){nav[9]=(unsigned char)!edge;buttons_ui(nav,0);}
 assert(state.cursor==before_cursor+1&&native_calls==calls_before_move);
 naming_pad_edge(1,1);naming_pad_edge(1,0);assert(state.cursor==before_cursor);
 naming_pad_edge(3,1);naming_pad_edge(3,0);assert(state.cursor==before_cursor+1);
 phone_move(&state,-2);write_native();assert(state.cursor==3);
 event[9]=1;assert(queue_native(event,PHONE_BUTTON)&&phone_events_pop(&queued)&&!queued.pad);buttons_ui(queued.native,queued.step_ticket);
 assert(state.shift&&state.cursor==3&&!strcmp(text_buffer,"abcd_E#"));
 naming_pad_edge(16,1);
 assert(!strcmp(text_buffer,"abc_E#")&&state.cursor==3);
 event[9]=0;assert(queue_native(event,PHONE_BUTTON)&&phone_events_pop(&queued)&&!queued.pad);buttons_ui(queued.native,queued.step_ticket);
 naming_pad_edge(16,0);
 assert(!state.shift&&state.cursor==3&&!strcmp(text_buffer,"abc_E#"));
 naming_pad_edge(16,1);naming_pad_edge(16,0);assert(!strcmp(text_buffer,"ab_E#")&&state.cursor==2);
 name_cursor_paint(editor,(void*)1);assert(!strcmp(painted_name,"ab|_E#"));
 phone_enable(&state,0);unsigned encoder_calls=native_calls;
 nav[8]=0x37;nav[9]=1;buttons_ui(nav,0);nav[9]=0;buttons_ui(nav,0);
 assert(native_calls==encoder_calls+2&&state.cursor==1&&!strcmp(text_buffer,"ab_E#"));
 /* With naming gone, background ownership remains available. */
 atomic_store(&naming_active,0);assert(queue_native(move,PHONE_CONTROLLER)&&phone_events_pop(&queued)&&queued.pad);
 assert(!phone_events_pending()&&!other_routes);
 /* Ordinary Sample has no staged-selection semantics: simultaneous pad
  * downs and every later repeat go to the native recorder unmodified. */
 claim_background=0;stub(phone_hooks[H_PAD].trampoline,native_musical_pad);
 __builtin___clear_cache((char*)(base+phone_hooks[H_PAD].trampoline),(char*)(base+phone_hooks[H_PAD].trampoline+16));
 unsigned char musical[40]={0};FIELD(musical,32,int)=3;
 for(unsigned repeat=0;repeat<2;++repeat){
  musical[16]=1;musical[8]=2;pad_event(musical);musical[8]=7;pad_event(musical);
  assert(passed==((1u<<2)|(1u<<7)));
  musical[16]=0;musical[8]=2;pad_event(musical);musical[8]=7;pad_event(musical);assert(!passed);
 }
 assert(musical_calls==8&&!phone_events_pending());
 for(unsigned i=0;i<8;++i){assert(musical_pads[i]==(i%2?7u:2u));assert(musical_edges[i]==(i%4<2));}
 puts("PASS ordinary Sample producer passes simultaneous pads, independent releases and repeated notes directly to native input; no selection toggles or dropped chord edges; native recording/DSP not executed");
 puts("PASS naming FIFO ownership over background screens; Pads +/- encoder and pad1/pad3 share one cursor; Shift+pad16 Delete and Backspace preserve tail and caret, both button edges consumed");
 puts("PASS naming caret overlay and vtable installation; visible insertion position without modifying saved name; encoder moves only cursor in Pads mode");
 puts("PASS production naming Shift down/up preserves exact pad text, case and cursor despite native stale cache; no underlying Sample/Lights/Steps routes; glyph/GUI receivers are fixtures");
}
