#include "src/pad_palette.h"
#define STOCK_BACKEND_MAIN backend_proof_main
#include "check_resample_stock_backend.c"
#include "src/resample_ui.h"
#include "src/music_settings.h"
static unsigned test_colours[128];
int music_pad_colour(unsigned pad){assert(pad<128);return (int)test_colours[pad];}
const uint32_t music_sequence_colours[MUSIC_SEQUENCE_COLOURS]={0x40ff40,0x32c780,0xffc247,0x8830ff,0xff20b0,0xff0000,0xff6000,0xffe000,0x2030ff,0x98ff00,0x00ffb0,0xffffff,0x00c8ff,0xff70b8};
static unsigned char screen[0x1600];
static char drawn[2048];
static int is_visible(void*p){assert(p==screen);return 1;}
static int is_blocked(void*p){assert(p==screen);return 0;}
static void repaint_stub(void*p,uint64_t a,uint64_t b){assert(p==screen&&!a&&!b);}
static void ui_rect(void*u,int x,int y,int w,int h,uint32_t c){(void)u;assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240&&(c>>24)==255);}
static void ui_text(void*u,const char*s,int x,int y,int w,int h,unsigned z,uint32_t c,int centre){(void)u;(void)z;(void)c;(void)centre;assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);assert(strlen(drawn)+strlen(s)+2<sizeof(drawn));strcat(drawn,s);strcat(drawn,"\n");}
static void draw(void){drawn[0]=0;struct resample_painter p={NULL,ui_rect,ui_text};resample_ui_draw(&p);}
static void key(unsigned code,int down){unsigned char e[16]={0};e[8]=code;e[9]=down;resample_ui_requested(code,down);assert(resample_ui_button(e,resample_ui_ticket())==(code!=0x31&&code!=0x51&&code!=0x52));}
static void button(unsigned b){key(0x0c+b,1);key(0x0c+b,0);}
static void open_ui(void){resample_ui_tick(0);resample_ui_requested(0x31,1);assert(resample_ui_claim_pad(10,1,0));resample_ui_pad(10,1,resample_ui_ticket());resample_ui_pad(10,0,resample_ui_ticket());stock_queue_pump();resample_ui_tick(0);key(0x31,0);draw();assert(strstr(drawn,"Sample")&&strstr(drawn,"Seq"));}
int main(int argc,char**argv){
 assert(!backend_proof_main(argc,argv));phase="simple physical stock UI";
 pad_samples[19]=0;stock_callback_init_action(callback,assign_rendered);FN(0xf535c0,void(*)(void*,const char*))(path,"/run/fixture-not-written.wav");
 patch(0x112fd50,is_visible);patch(0x112f6b0,is_blocked);patch(0x1149880,repaint_stub);F(screen,0,uintptr_t)=base+0x6e212a0;
 resample_ui_init(base);resample_ui_transport(0,1);resample_ui_bind(copy_context);resample_ui_paint(screen,NULL);open_ui();button(0);assert(!resample_ui_active());
 open_ui();button(2);draw();assert(strstr(drawn,"Source")&&strstr(drawn,"Render")&&strstr(drawn,"Press a pad"));resample_ui_pad(3,1,resample_ui_ticket());resample_ui_pad(3,0,resample_ui_ticket());draw();assert(strstr(drawn,"B04"));
 for(unsigned hue=1;hue<MUSIC_SEQUENCE_COLOURS;++hue){
  test_colours[18]=hue;resample_ui_pad(3,1,resample_ui_ticket());
  uint64_t source_colour;assert(resample_ui_light(2,&source_colour)&&((uint32_t)source_colour==PAD_DANGER));
 }
 test_colours[18]=0;resample_ui_pad(3,1,resample_ui_ticket());
 puts("PASS actual resample UI palette retains all12 assigned source hues while choosing another pad; destination selection does not start audio");
 uint64_t selected,other;assert(resample_ui_light(3,&selected)&&resample_ui_light(2,&other)&&((uint32_t)selected==PAD_FREE)&&((uint32_t)other==PAD_DANGER));uint64_t palette_before=resample_ui_palette_generation();
 requested_target=19;button(2);assert(resample_ui_active());stock_queue_pump();resample_ui_tick(0);assert(!resample_ui_active());
 assert(!resample_ui_light(3,&selected)&&resample_ui_palette_generation()>palette_before);
 unsigned char e[16]={0};e[8]=0x13;assert(!resample_ui_control(e,resample_ui_ticket()));e[8]=0x52;e[9]=1;assert(!resample_ui_button(e,resample_ui_ticket()));
 stock_callback_release(1);resample_ui_tick(0);stock_queue_pump();resample_ui_tick(0);assert(pad_samples[19]==(uintptr_t)samples[1]);
 phase="playing transport stop confirmation precedes any render submission";
 pad_samples[19]=(uintptr_t)samples[0];
 resample_ui_transport(1,1);open_ui();button(2);resample_ui_pad(3,1,resample_ui_ticket());button(2);button(2);
 draw();assert(strstr(drawn,"Stop playback?")&&resample_ui_active());
 unsigned char stop[16]={0};assert(!resample_ui_take_stop(stop));button(2);draw();assert(strstr(drawn,"To B04")&&!strstr(drawn,"Stop playback?")&&!resample_ui_take_stop(stop));
 button(2);button(2);draw();assert(strstr(drawn,"Stop playback?"));button(0);
 assert(resample_ui_take_stop(stop)&&stop[8]==0x51&&stop[9]==1&&!resample_ui_take_stop(stop));
 resample_ui_tick(0);draw();assert(strstr(drawn,"Stopping...")&&resample_ui_active());
 resample_ui_transport(0,1);resample_ui_tick(0);stock_queue_pump();resample_ui_tick(0);assert(!resample_ui_active());
 stock_callback_release(1);resample_ui_tick(0);stock_queue_pump();resample_ui_tick(0);
 open_ui();button(2);resample_ui_pad(3,1,resample_ui_ticket());button(2);draw();assert(strstr(drawn,"Replace")&&strstr(drawn,"press again to replace"));button(0);button(0);assert(!resample_ui_active());
 open_ui();button(0);assert(!resample_ui_active());resample_ui_context_dead(copy_context);resample_ui_tick(0);resample_ui_requested(0x31,1);assert(!resample_ui_claim_pad(10,1,0));
 stock_callback_destroy(callback);FN(0xf0a290,void(*)(void*))(path);
 puts("PASS simple physical UI: Seq/Sample, silent destination and banks, successful native handoff, callback completion, occupied confirmation/Cancel/reopen, Play/Stop/Master passthrough and stopped context; graphics and render transport are fixtures");return 0;
}
