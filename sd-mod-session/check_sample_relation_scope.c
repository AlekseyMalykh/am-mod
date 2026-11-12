#define _GNU_SOURCE
#define PHONE_RGB_TEST 1
#define MPC_MIXER_LIVE 1
#define MPC_COMPRESSOR 1
#define MPC_CHORDS 1
#define MPC_RESAMPLE_TRIAL 1
#define MPC_REAR_ROUTING 1
#include "src/voice_guard.h"
#include "src/phone_native.c"
#include <assert.h>
enum {HIDDEN,PROJECT,SEQ,KNOB,SETTINGS,BROWSER,NATIVE_WORKFLOW,STEPS,COMPRESSOR,CHORDS,RESAMPLE,RENDER,REAR,INPUT_CONFIG,LEVELS,COUNT};
static unsigned blocked;
/* Lighting mode is the only music_settings state the scope predicate reads. */
static int lighting_mode=MUSIC_LIGHTING_MOD;
int music_pad_lighting_mode(void){return lighting_mode;}
int music_set_pad_lighting_mode(unsigned mode){if(mode>MUSIC_LIGHTING_MOD)return 0;lighting_mode=(int)mode;return 1;}
int note_native_sample_visible(void){return !(blocked&(1u<<HIDDEN));}
int music_ui_chords_available(void){return !(blocked&(1u<<PROJECT));}
int music_ui_sequence_visible(void){return !!(blocked&(1u<<SEQ));}
int music_ui_knob_fx_active(void){return !!(blocked&(1u<<KNOB));}
int music_ui_pad_settings_visible(void){return !!(blocked&(1u<<SETTINGS));}
int workflow_blocks_sample_controls(void){return !!(blocked&(1u<<BROWSER));}
int workflow_native_pad_colours(void){return !!(blocked&(1u<<NATIVE_WORKFLOW));}
int step_native_active(void){return !!(blocked&(1u<<STEPS));}
int compressor_ui_active(void){return !!(blocked&(1u<<COMPRESSOR));}
int chords_active(void){return !!(blocked&(1u<<CHORDS));}
int resample_ui_active(void){return !!(blocked&((1u<<RESAMPLE)|(1u<<RENDER)));}
int rear_ui_pad_owns(void){return !!(blocked&(1u<<REAR));}
int rear_ui_input_config_visible(void){return !!(blocked&(1u<<INPUT_CONFIG));}
int music_lighting_levels_state(unsigned*on,unsigned*mode,unsigned*origin){*on=!!(blocked&(1u<<LEVELS));*mode=8;*origin=0;return 1;}
int main(void){
 assert(sample_relations_visible());
 for(unsigned i=0;i<COUNT;++i){blocked=1u<<i;assert(!sample_relations_visible());}
 blocked=0;assert(sample_relations_visible());
 atomic_store(&naming_active,1);assert(!sample_relations_visible());atomic_store(&naming_active,0);
 atomic_store(&native_mute_mode,1);assert(!sample_relations_visible());atomic_store(&native_mute_mode,0);
 atomic_store(&native_buttons_down[0x09],1);assert(!sample_relations_visible());atomic_store(&native_buttons_down[0x09],0);
 for(unsigned page=MIXER_OVERVIEW;page<=MIXER_INSPECT;++page){
  (void)page;atomic_store(&mixer_route,MIXER_OPEN);assert(!sample_relations_visible());
 }
 atomic_store(&mixer_route,0);assert(sample_relations_visible());
 for(blocked=1;blocked<(1u<<COUNT);++blocked)assert(!sample_relations_visible());
 blocked=0;assert(sample_relations_visible());
 /* Pad Link / Mute Group hints exist only in Am MOD lighting mode. */
 assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_STANDARD)&&!sample_relations_visible());
 assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_MOD)&&sample_relations_visible());
 puts("PASS production Sample relation scope: hidden/Project/Seq/Knob FX/settings/browser/Copy/Steps/Compressor/Chords/Resample/Rear/Input Config/Chop/16 Levels; Mixer, naming, Mute, Erase and all32767 overlap combinations; return to Sample");
 return 0;
}
