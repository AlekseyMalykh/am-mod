#define _GNU_SOURCE
#include "src/music_settings.h"
#include "src/mixer_model.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
static struct music_project before,after;
static char payload[MUSIC_PAYLOAD_BYTES];
static uint32_t pitch(struct music_sample *s,int semitones){
 int replace=0;uint64_t v=music_pitch_light(music_light_snapshot(s,0),semitones,&replace);
 assert(replace);return (uint32_t)v;
}
int main(void){
 char dir[]="/tmp/am-global-colours-XXXXXX",path[256];assert(mkdtemp(dir));
 assert(music_lighting_preferences_open(dir));
 assert(MUSIC_PALETTE_SLOTS==32&&music_led_vivid()==1&&!music_palette_reserved());
 assert(music_palette_choice_count()==14&&!music_palette_present(31));
 assert(music_palette_save(31,0x12f340,"Acid green"));
 assert(music_palette_save(16,0xfa0040,"Neon red"));
 assert(music_palette_save(15,0,"Black"));
 assert(music_palette_present(15)&&!music_palette_rgb(15));
 assert(music_set_service_colour(MUSIC_SERVICE_SELECT,15));
 assert(!music_service_rgb(MUSIC_SERVICE_SELECT)&&!music_service_colours_fallback());
 assert(music_set_service_colour(MUSIC_SERVICE_SELECT,8));
 assert(music_identity_choice(music_identity_index(47))==47);
 assert(music_user_rgb(47)==0x12f340&&!strcmp(music_identity_name(47),"Acid green"));
 int found=0;for(unsigned i=0;i<mixer_colour_count();++i)if(mixer_colour_at(i)==0x12f340){
  assert(!strcmp(mixer_colour_name(mixer_colour_at(i)),"Acid green"));found=1;
 }assert(found);
 assert(music_set_service_colour(MUSIC_SERVICE_SELECT,31));
 assert(music_service_rgb(MUSIC_SERVICE_SELECT)==0x12f340);
 assert(music_set_relation_colour(MUSIC_RELATION_LINK,31));
 assert(music_relation_rgb(MUSIC_RELATION_LINK)==0x12f340);
 assert(music_set_palette_reserved(1)&&!music_palette_available(31));
 for(unsigned i=0;i<mixer_colour_count();++i)assert(mixer_colour_at(i)!=0x12f340);
 assert(!music_set_relation_colour(MUSIC_RELATION_GROUP,31));
 assert(music_set_service_colour(MUSIC_SERVICE_SELECT,8)&&music_palette_available(31));
 assert(music_set_palette_reserved(0));
 assert(music_set_pad_colour(127,47)&&music_set_sequence_colour(127,32));
 assert(music_set_sample("Keys",MUSIC_ROOT,0)&&music_set_sample("Keys",MUSIC_MODE,MUSIC_DEGREES));
 const int fields[]={MUSIC_COLOUR0,MUSIC_COLOUR1,MUSIC_COLOUR2,MUSIC_ORIGINAL_COLOUR,MUSIC_FIFTH_COLOUR,MUSIC_FOURTH_COLOUR};
 struct music_sample sample;
 for(unsigned i=0;i<6;++i)assert(music_set_sample("Keys",fields[i],43));
 assert(music_get_sample("Keys",&sample));
 assert(pitch(&sample,0)==0x12f340&&pitch(&sample,12)==0x12f340);
 assert(pitch(&sample,2)==0x12f340&&pitch(&sample,5)==0x12f340&&pitch(&sample,7)==0x12f340);
 sample.mode=MUSIC_SCALE;assert(pitch(&sample,1)==0x12f340); /* Other role upper bits */
 const unsigned slots[]={16,17,18,19,30,31};
 uint32_t rgb[6];
 for(unsigned i=0;i<6;++i){rgb[i]=0x102030u+i*0x10101u;
  assert(music_palette_save(slots[i],rgb[i],"Independent"));
  assert(music_set_sample("Keys",fields[i],(int)(12+slots[i])));
 }
 assert(music_get_sample("Keys",&sample));
 assert(pitch(&sample,0)==rgb[3]&&pitch(&sample,12)==rgb[0]&&pitch(&sample,2)==rgb[1]);
 assert(pitch(&sample,5)==rgb[5]&&pitch(&sample,7)==rgb[4]);
 sample.mode=MUSIC_SCALE;assert(pitch(&sample,1)==rgb[2]);
 assert(music_palette_save(31,0x12f340,"Acid green"));
 music_snapshot(&before);int n=music_encode(&before,payload,sizeof(payload));
 assert(n>0&&!memcmp(payload,"MPCS20 ",7));
 assert(music_decode(payload,(size_t)n,&after)&&!memcmp(&before,&after,sizeof(before)));
 assert(!music_decode(payload,(size_t)n-1,&after));
 char *last=strstr(payload,"PAD128\n");assert(last);last[7]='f';assert(!music_decode(payload,(size_t)n,&after));
 assert(music_lighting_preferences_open(dir)&&music_palette_rgb(31)==0x12f340);
 assert(!strcmp(music_palette_name(31),"Acid green")&&music_relation_colour(MUSIC_RELATION_LINK)==31);
 /* A failed disk write must leave both banks unchanged. */
 snprintf(path,sizeof(path),"%s/.am-mod-palette",dir);assert(!unlink(path)&&!mkdir(path,0700));
 assert(!music_palette_save(31,0xffffff,"Lost")&&music_palette_rgb(31)==0x12f340);
 assert(!rmdir(path));
 /* Read the previously released 16-slot palette with names intact. */
 unsigned char old[4+16*(4+MUSIC_COLOUR_NAME_BYTES)]={'A','M','P','2'};
 old[4+14*28]=1;old[4+14*28+1]=0x11;old[4+14*28+2]=0x22;old[4+14*28+3]=0x33;
 memcpy(old+4+14*28+4,"Legacy name",12);
 FILE *f=fopen(path,"wb");assert(f&&fwrite(old,1,sizeof(old),f)==sizeof(old)&&!fclose(f));
 assert(music_lighting_preferences_open(dir)&&music_palette_rgb(14)==0x112233);
 assert(!strcmp(music_palette_name(14),"Legacy name")&&!music_palette_present(31));
 assert(music_palette_save(31,0x00ffaa,"Bank B"));
 assert(music_lighting_preferences_open(dir)&&music_palette_rgb(14)==0x112233&&music_palette_rgb(31)==0x00ffaa);
 assert(music_set_service_colour(MUSIC_SERVICE_SELECT,31));
 assert(music_set_service_colour(MUSIC_SERVICE_FREE,31));
 assert(music_service_colour(MUSIC_SERVICE_FREE)==31&&music_service_colour(MUSIC_SERVICE_SELECT)==9);
 assert(music_set_palette_reserved(1)&&music_mod_restore_recommended(2));
 assert(!music_palette_reserved()&&music_service_colour(MUSIC_SERVICE_SELECT)==8&&music_led_vivid()==1);
 const char legacy[]="MPCS12 0 0 1\n0 3 0 0 1 10 -1 -1\n42617373\n";
 assert(music_decode(legacy,sizeof(legacy)-1,&after)&&after.samples[0].original_colour==4);
 puts("PASS global 32-colour palette: names, RGB/black, Mixer, all six Lights roles, cursor, relation colours, reservation/release, project roundtrip, AMP2 migration and failed-write rollback");
 return 0;
}
