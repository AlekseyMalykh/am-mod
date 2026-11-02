#define _GNU_SOURCE
#include "src/music_settings.h"
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
 int replace=1;assert(!music_pitch_light(published,0,&replace)&&!replace);
 assert(music_published()==published&&music_transposition()==transpose);
 music_snapshot(&after);assert(!memcmp(&before,&after,sizeof(before))&&music_pad_colour(0)==13&&music_sequence_colour(3)==5);
 music_reset();assert(music_pad_lighting_mode()==MUSIC_LIGHTING_STANDARD);music_replace(&before);assert(!memcmp(&before,&after,sizeof(before)));
 assert(music_lighting_preferences_open(dir)&&music_pad_lighting_mode()==MUSIC_LIGHTING_STANDARD);
 assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_MOD)&&music_identity_rgb(13)==0xff70b8);
 assert(music_lighting_preferences_open(dir)&&music_pad_lighting_mode()==MUSIC_LIGHTING_MOD);
 assert(!music_set_pad_lighting_mode(2));
 char path[512],sentinel[512];snprintf(path,sizeof(path),"%s/.am-mod-pad-lighting",dir);snprintf(sentinel,sizeof(sentinel),"%s/sentinel",dir);
 FILE*f=fopen(path,"wb");assert(f);fputs("truncated",f);fclose(f);assert(music_lighting_preferences_open(dir)&&music_pad_lighting_mode()==MUSIC_LIGHTING_MOD);
 assert(!unlink(path));f=fopen(sentinel,"wb");assert(f);fputs("UNCHANGED",f);fclose(f);assert(!symlink(sentinel,path));
 assert(music_lighting_preferences_open(dir)&&music_pad_lighting_mode()==MUSIC_LIGHTING_MOD);
 assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_STANDARD));char text[32]={0};f=fopen(sentinel,"rb");assert(f&&fread(text,1,sizeof(text),f)==9&&!strcmp(text,"UNCHANGED"));fclose(f);
 assert(!unlink(sentinel)&&!unlink(path)&&!rmdir(dir));assert(!music_set_pad_lighting_mode(MUSIC_LIGHTING_MOD)&&music_pad_lighting_mode()==MUSIC_LIGHTING_STANDARD);
 puts("PASS global lighting persistence, reset/project independence, legacy colour preservation, unchanged pitch/transpose, malformed/symlink handling and failed-save rollback");return 0;
}
