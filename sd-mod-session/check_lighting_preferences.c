#define _GNU_SOURCE
#include "src/music_settings.h"
#include "src/pad_workflow_colour.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void){
 char dir[]="/tmp/am-pad-lighting-XXXXXX";assert(mkdtemp(dir));
 assert(music_lighting_preferences_open(dir)&&music_pad_lighting_mode()==MUSIC_LIGHTING_MOD);
 music_reset();assert(music_set_pad_colour(0,13)&&music_set_sequence_colour(3,5));
 struct music_sample sample;music_get_sample("Proof",&sample);sample.root=0;sample.mode=MUSIC_PIANO;sample.transpose=7;music_publish(&sample,0);
 struct music_project before,after;music_snapshot(&before);uint64_t published=music_published();int transpose=music_transposition();
 assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_STANDARD));
 for(unsigned i=0;i<MUSIC_SEQUENCE_COLOURS;++i)assert(music_identity_rgb(i)==0x2030ff);
 int replace=0;uint64_t piano=music_pitch_light(published,0,&replace);assert(piano&&replace);
 for(int mode=MUSIC_PIANO;mode<=MUSIC_SCALE;++mode){
  sample.mode=mode;uint64_t snapshot=music_light_snapshot(&sample,0);
  uint64_t standard[16];
  for(unsigned pad=0;pad<16;++pad){standard[pad]=music_pad_light(snapshot,pad,3,&replace);assert(replace);}
  assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_MOD));
  for(unsigned pad=0;pad<16;++pad){assert(music_pad_light(snapshot,pad,3,&replace)==standard[pad]&&replace);}
  assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_STANDARD));
 }
 sample.mode=MUSIC_MONO;assert(!music_pitch_light(music_light_snapshot(&sample,0),0,&replace)&&!replace);
 sample.mode=MUSIC_PIANO;sample.root=-1;assert(!music_pitch_light(music_light_snapshot(&sample,0),0,&replace)&&!replace);
 assert(music_published()==published&&music_transposition()==transpose);
 music_snapshot(&after);assert(!memcmp(&before,&after,sizeof(before))&&music_pad_colour(0)==13&&music_sequence_colour(3)==5);
 music_reset();assert(music_pad_lighting_mode()==MUSIC_LIGHTING_STANDARD);music_replace(&before);assert(!memcmp(&before,&after,sizeof(before)));
 assert(music_lighting_preferences_open(dir)&&music_pad_lighting_mode()==MUSIC_LIGHTING_STANDARD);
 assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_MOD)&&music_identity_rgb(13)==music_palette_default_rgb(4));
 assert(music_lighting_preferences_open(dir)&&music_pad_lighting_mode()==MUSIC_LIGHTING_MOD);
 assert(!music_set_pad_lighting_mode(2));
 uint64_t defaults=music_mod_settings_snapshot();
 assert(music_tail_ms((unsigned)music_mod_setting(MUSIC_SETTING_TAIL))==1000);
 for(unsigned i=0;i<MUSIC_TAIL_CHOICES;++i){
  assert(music_set_mod_setting(MUSIC_SETTING_TAIL,i));
  assert(music_lighting_preferences_open(dir)&&music_mod_setting(MUSIC_SETTING_TAIL)==(int)i);
 }
 assert(!music_set_mod_setting(MUSIC_SETTING_TAIL,MUSIC_TAIL_CHOICES));
 assert(music_mod_restore_recommended(1)&&music_mod_settings_snapshot()==defaults);
 for(unsigned role=0;role<MUSIC_SERVICE_COUNT;++role)for(unsigned c=0;c<MUSIC_PALETTE_SIZE;++c){
  assert(music_set_service_colour(role,c)&&music_service_colour(role)==c);
  uint64_t snapshot=music_mod_settings_snapshot();assert(music_mod_settings_valid(snapshot));
  for(unsigned user=1;user<MUSIC_IDENTITY_CHOICES;++user)for(unsigned system=0;system<MUSIC_SERVICE_COUNT;++system)
   assert(music_identity_rgb(music_colour_choice(user))!=music_service_rgb(system));
  assert(music_lighting_preferences_open(dir)&&music_mod_settings_snapshot()==snapshot);
 }
 assert(music_mod_restore_recommended(2)&&music_mod_settings_snapshot()==defaults);
 assert(!music_set_service_colour(6,0)&&!music_set_service_colour(0,MUSIC_PALETTE_SIZE)&&!music_mod_settings_restore(defaults&31));
 assert(MUSIC_USER_COLOURS==8&&MUSIC_IDENTITY_CHOICES==9);
 for(unsigned a=1;a<MUSIC_IDENTITY_CHOICES;++a)for(unsigned b=a+1;b<MUSIC_IDENTITY_CHOICES;++b)
  assert(music_identity_rgb(music_colour_choice(a))!=music_identity_rgb(music_colour_choice(b)));
 assert(music_service_rgb(MUSIC_SERVICE_FREE)==0x00ff00&&music_service_rgb(MUSIC_SERVICE_DANGER)==0xff0000&&music_service_rgb(MUSIC_SERVICE_PLAYHEAD)==0x00ffff);
 uint64_t old_default=(UINT64_C(0xa9876543210)<<5)|17;
 assert(music_mod_settings_valid(old_default)&&music_mod_settings_restore(old_default)&&music_mod_settings_snapshot()==defaults);
 uint64_t old_swap=old_default;
 old_swap&=~((UINT64_C(15)<<(5+4*0))|(UINT64_C(15)<<(5+4*5)));
 old_swap|=UINT64_C(5)<<(5+4*0); /* old cursor White <-> user Blue */
 assert(music_mod_settings_restore(old_swap)&&music_identity_rgb(8)==0xffffff&&music_service_rgb(MUSIC_SERVICE_SELECT)==music_palette_default_rgb(0));
 assert(music_mod_settings_restore(defaults));
 char legacy_path[512];snprintf(legacy_path,sizeof(legacy_path),"%s/.am-mod-settings",dir);
 FILE*legacy=fopen(legacy_path,"wb");assert(legacy);fprintf(legacy,"AM_SETTINGS1 %013llx\n",(unsigned long long)old_default);assert(!fclose(legacy));
 assert(music_lighting_preferences_open(dir)&&music_mod_settings_snapshot()==defaults);
 assert(music_set_mod_setting(MUSIC_SETTING_WAVE,1)&&music_set_mod_setting(MUSIC_SETTING_SPECTRUM,1));
 assert(music_set_mod_setting(MUSIC_SETTING_BLINK,0)&&music_cursor_on(9999,150));
 assert(music_set_mod_setting(MUSIC_SETTING_BLINK,3)&&music_cursor_on(100,500)&&!music_cursor_on(250,500));
 for(unsigned mode=0;mode<4;++mode){
  assert(music_set_mod_setting(MUSIC_SETTING_BLINK,mode));
  uint64_t saved=music_mod_settings_snapshot();
  assert(music_lighting_preferences_open(dir)&&music_mod_settings_snapshot()==saved);
  unsigned period=mode==1?1200:mode==2?800:400;
  for(unsigned ms=0;ms<2400;ms+=20){
   int white=!mode||ms%period<period/2;
   assert((uint32_t)pad_source_light(0xff20b0,ms)==(white?PAD_SELECT:0xff20b0));
   assert((uint32_t)pad_operation_colour(0,0,1,0,0,ms)==(white?PAD_SELECT:0));
   assert((uint32_t)pad_operation_colour(0,1,1,1,1,ms)==(mode&&white?PAD_SELECT:PAD_DANGER));
   assert((uint32_t)pad_operation_colour(0,0,1,1,1,ms)==(mode&&white?PAD_SELECT:PAD_FREE));
   assert((uint32_t)pad_workflow_colour(0,1,1,0,0,ms)==(white?PAD_SELECT:music_identity_rgb(music_pad_colour(0))));
  }
 }
 puts("PASS selection/source/library cursor Steady/Slow/Normal/Fast, preserved identity and empty off phases, red/green overlap priority, durable setting");
 uint64_t project_display=music_mod_settings_snapshot();
 assert(music_mod_restore_recommended(1)&&!music_mod_setting(MUSIC_SETTING_WAVE)&&!music_mod_setting(MUSIC_SETTING_SPECTRUM));
 assert(music_mod_restore_recommended(3)&&music_mod_setting(MUSIC_SETTING_BLINK)==2);
 assert(music_mod_settings_restore(project_display)); /* load must not replace card defaults */
 assert(music_lighting_preferences_open(dir)&&music_mod_settings_snapshot()==defaults);
 char path[512],sentinel[512];snprintf(path,sizeof(path),"%s/.am-mod-settings",dir);snprintf(sentinel,sizeof(sentinel),"%s/sentinel",dir);
 FILE*f=fopen(path,"wb");assert(f);fputs("truncated",f);fclose(f);assert(music_lighting_preferences_open(dir)&&music_pad_lighting_mode()==MUSIC_LIGHTING_MOD);
 assert(!unlink(path));f=fopen(sentinel,"wb");assert(f);fputs("UNCHANGED",f);fclose(f);assert(!symlink(sentinel,path));
 assert(music_lighting_preferences_open(dir)&&music_pad_lighting_mode()==MUSIC_LIGHTING_MOD);
 assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_STANDARD));char text[32]={0};f=fopen(sentinel,"rb");assert(f&&fread(text,1,sizeof(text),f)==9&&!strcmp(text,"UNCHANGED"));fclose(f);
 char palette_path[512];snprintf(palette_path,sizeof(palette_path),"%s/.am-mod-palette",dir);
 assert(music_palette_set_rgb(0,0)&&music_palette_rgb(0)==0);
 assert(music_palette_set_rgb(8,0x778899)&&music_palette_rgb(8)==0x778899);
 assert(!music_palette_set_rgb(MUSIC_PALETTE_SIZE,0)&&!music_palette_set_rgb(0,0x1000000));
 assert(music_lighting_preferences_open(dir)&&music_palette_rgb(0)==0&&music_palette_rgb(8)==0x778899);
 assert(!music_palette_present(14)&&!music_palette_present(15));
 struct music_palette_edit edits[MUSIC_PALETTE_SLOTS]={0};
 edits[14]=(struct music_palette_edit){0x115577,"Ocean",0};
 edits[15]=(struct music_palette_edit){0,"Black",0};
 assert(music_palette_apply(edits,(1u<<14)|(1u<<15)));

 assert(music_lighting_preferences_open(dir)&&music_palette_present(15));
 assert(!strcmp(music_palette_name(14),"Ocean")&&music_identity_choice(music_identity_count()-2)==14);
 assert(!strcmp(music_identity_name(15),"Black")&&music_user_rgb(15)==0);
 assert(music_palette_reset(14)&&music_lighting_preferences_open(dir)&&!music_palette_present(14));
 assert(music_identity_choice(music_identity_count()-1)==15&&music_identity_index(15)==music_identity_count()-1);
 assert(music_service_rgb(MUSIC_SERVICE_SELECT)==0xffffff); /* Standard palette remains stock. */
 assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_MOD)&&music_user_rgb(0)==0&&music_service_rgb(MUSIC_SERVICE_SELECT)==0x778899);
 assert(music_palette_set_rgb(8,music_palette_default_rgb(8))&&music_lighting_preferences_open(dir)&&music_palette_rgb(8)==0xffffff);
 assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_STANDARD));
 assert(!unlink(palette_path));f=fopen(palette_path,"wb");assert(f);fputs("AMP1",f);fclose(f);
 assert(music_lighting_preferences_open(dir)&&music_palette_rgb(0)==music_palette_default_rgb(0));
 assert(!unlink(palette_path)&&!unlink(sentinel)&&!unlink(path)&&!rmdir(dir));
 assert(!music_palette_set_rgb(0,0)&&music_palette_rgb(0)==music_palette_default_rgb(0));
 assert(!music_set_pad_lighting_mode(MUSIC_LIGHTING_MOD)&&music_pad_lighting_mode()==MUSIC_LIGHTING_STANDARD);
 puts("PASS global lighting persistence, reset/project independence, legacy colour preservation, unchanged pitch/transpose, malformed/symlink handling and failed-save rollback");return 0;
}
