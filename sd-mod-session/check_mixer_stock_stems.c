#define _GNU_SOURCE
#include <assert.h>
#include "src/mixer_stems.c"
static unsigned fx_mode;
unsigned music_fx_mode_active(void){return fx_mode;}
int main(void){
 image=1;fx_mode=0;assert(!mixer_stems_available());mixer_stems_arm(1);assert(!mixer_stems_armed());
 fx_mode=1;assert(mixer_stems_available());mixer_stems_arm(1);assert(mixer_stems_armed());mixer_stems_disarm();
 fx_mode=0;assert(!mixer_stems_available());mixer_stems_arm(0);assert(!mixer_stems_armed());
 puts("PASS Stock cannot arm an offline Mixer stems job; Am MOD still can");return 0;
}
