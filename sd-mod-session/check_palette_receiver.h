/* Default-only settings receiver for isolated sender/backend fixtures.
 * Full settings permutations, persistence and archive commits are exercised
 * against music_settings.c in check_lighting_preferences/project_recovery. */
#include "src/music_settings.h"
uint32_t music_service_rgb(unsigned role){
 static const uint32_t rgb[]={0xffffff,0x00ff00,0xff0000,0xffb000,0x00ffff,0x707070};
 return rgb[role<MUSIC_SERVICE_COUNT?role:0];
}
uint32_t music_user_rgb(unsigned raw){return music_sequence_colours[music_colour_effective(raw)];}
int music_cursor_on(uint64_t phase,unsigned duty){return phase%800u<800u*duty/1000u;}
int music_mod_setting(unsigned field){return field==MUSIC_SETTING_BLINK?2:0;}
uint32_t music_led_correct(uint32_t rgb){return rgb;}
