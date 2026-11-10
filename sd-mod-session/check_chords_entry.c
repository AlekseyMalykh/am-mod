/* Keep the real workflow ownership predicate, including Sample's broad
 * observer-active flag. Unrelated native paths are link-GC'd, not executed. */
#include "src/workflow_ui.c"
#include "src/chords.h"
#include "src/chords_ui.h"
#include <assert.h>
static int sample_visible=1,render_dialog,render_busy,level_on=1,level_type=1;
static unsigned repaints;
int music_ui_chords_available(void){return sample_visible;}
int music_ui_knob_fx_active(void){return 0;}
int music_ui_shift_active(void){return 0;}
/* Arpeggiator/playback seams: no sequence playback or held arp notes here. */
int step_native_playback_pitches(int*o,int m){(void)o;(void)m;return 0;}
int64_t step_native_clock_ticks(uint64_t*s,unsigned*r){if(s)*s=0;if(r)*r=0;return 0;}
int poly_record_arp_notes(int*n,int m,float*v){(void)n;(void)m;(void)v;return 0;}
int poly_record_arp_emit(int p,int on,float v){(void)p;(void)on;(void)v;return 0;}
void voice_guard_lock(void){}
int music_ui_repeat_rate(void){return -1;}
unsigned phone_native_button_light(unsigned c){(void)c;return 1;}
void voice_guard_unlock(void){}
int resample_ui_tick(int blocked){(void)blocked;return 1;} /* Ready for Shift+pad11. */
int resample_ui_active(void){return render_dialog;}
int resample_ui_shutdown_busy(void){return render_busy;}
void *note_native_context(void){return NULL;}
int note_native_request_poly(void){return 0;}
int note_native_pad_index(unsigned*p){*p=12;return 1;}
void note_native_repaint_sample(void){++repaints;}
int music_lighting_levels_state(unsigned*on,unsigned*mode,unsigned*origin){*on=(unsigned)level_on;*mode=(unsigned)level_type;*origin=3;return 1;}
static int press(unsigned code,int down){unsigned char e[16]={0};e[8]=(unsigned char)code;e[9]=(unsigned char)down;return chords_ui_button(e);}
static void opens(void){chords_ui_poll(0);assert(chords_ui_button_owns(0x4b));assert(press(0x4b,1)&&press(0x4b,0)&&chords_active());chords_close();}
static void blocked(void){chords_ui_poll(0);assert(!chords_ui_button_owns(0x4b));assert(!press(0x4b,1)&&!chords_active());}
int main(void){
 chords_ui_init(0);
 atomic_store(&active,1); /* Sample is observed for native Chop actions. */
 assert(workflow_active()&&!workflow_blocks_sample_controls());
 assert(resample_ui_tick(0)&&!resample_ui_active()&&!resample_ui_shutdown_busy());
 assert(!(music_ui_chords_available()&&!workflow_active()&&!resample_ui_tick(0))); /* r24's false block reproduced. */
 opens();
 atomic_store(&button_routes,2);blocked(); /* Native browser is an actual modal. */
 atomic_store(&button_routes,1);blocked(); /* Native Copy is an actual modal. */
 atomic_store(&button_routes,0);atomic_store(&custom,1);blocked(); /* Batch/Mono. */
 atomic_store(&custom,0);batch_job=1;blocked();batch_job=0;batch_leaving=1;blocked();batch_leaving=0;
 atomic_store(&split_forwarded,1);blocked();atomic_store(&split_forwarded,0);
 render_dialog=1;blocked();render_dialog=0;render_busy=1;blocked();render_busy=0;
 sample_visible=0;blocked();sample_visible=1;
 chords_ui_poll(1);assert(!chords_ui_button_owns(0x4b)); /* Naming or Steps. */
 level_on=0;blocked();level_on=1;level_type=8;blocked();level_type=1;opens();
 assert(repaints);
 puts("PASS r24 entry regression: real Sample observer active + Resample entry ready allow Chords; browser/Copy/Batch/Mono/render/Steps/naming remain blocked");
 return 0;
}
