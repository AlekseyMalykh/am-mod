/* Included after the existing composed callbacks, before the hook table. */
#ifdef MPC_PATTERN_SOURCE_TRIAL
#include "pattern_lifecycle.h"
static void pattern_music_load(void*p,void*a){am_pattern_lifecycle_load(p,a,music_load);}
static int pattern_music_read(void*p,void*j){return am_pattern_lifecycle_read(p,j,music_read);}
static void pattern_new_project(void*p,int*choice){am_pattern_lifecycle_new(p,choice,new_project);}
#define PHONE_PATTERN_LOAD pattern_music_load
#define PHONE_PATTERN_READ pattern_music_read
#define PHONE_PATTERN_NEW pattern_new_project
#else
#define PHONE_PATTERN_LOAD music_load
#define PHONE_PATTERN_READ music_read
#define PHONE_PATTERN_NEW new_project
#endif
