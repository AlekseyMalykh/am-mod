#include "src/music_settings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t light(struct music_sample*s,unsigned pad,int reference){int replace=0;music_publish(s,0);uint64_t v=music_pad_light(music_published(),pad,reference,&replace);assert(replace);return (uint32_t)v;}
int main(void){
 music_reset();struct music_sample s;music_get_sample("Bass",&s);assert(s.root==-1&&s.original_colour==4);
 assert(MUSIC_SCALE==3&&MUSIC_DEGREES==4&&music_mode_ui_index(MUSIC_SCALE)==4&&music_mode_ui_index(MUSIC_DEGREES)==3);
 for(int mode=0;mode<=4;++mode)assert(music_mode_from_ui_index(music_mode_ui_index(mode))==mode);
 assert(music_set_sample("Bass",MUSIC_ROOT,0)&&music_set_sample("Bass",MUSIC_MODE,MUSIC_DEGREES));music_get_sample("Bass",&s);
 for(int t=-24;t<=24;++t){s.transpose=t;music_publish(&s,0);assert(music_transposition()==t);}
 s.transpose=0;assert(light(&s,0,0)==music_colours[4]);
 const unsigned pads[]={2,4,5,7,9,11,12};const unsigned colours[]={1,2,3,6,7,8,0};
 for(unsigned i=0;i<7;++i)assert(light(&s,pads[i],0)==music_colours[colours[i]]);
 assert(light(&s,1,0)==music_colours[10]);
 s.mode=MUSIC_PIANO;assert(light(&s,0,0)==music_colours[4]&&light(&s,12,0)==0xffffff&&light(&s,1,0)==0x2030ff);
 s.transpose=2;assert(light(&s,2,4)==music_colours[4]);
 for(int m=MUSIC_TONIC;m<=MUSIC_DEGREES;++m){s.mode=m;s.transpose=0;assert(light(&s,4,4)==music_colours[4]);}
 s.mode=MUSIC_MONO;music_publish(&s,0);int replace=1;music_pad_light(music_published(),0,0,&replace);assert(!replace);
 s.mode=MUSIC_SCALE;s.transpose=7;music_publish(&s,12);uint64_t ordinary=music_published();
 uint64_t independent=music_light_snapshot(&s,-2);assert(music_published()==ordinary);
 for(int pitch=-120;pitch<=120;++pitch){
  uint64_t actual=music_pitch_light(independent,pitch,&replace);assert(replace);
  struct music_sample untransposed=s;untransposed.transpose=0;
  uint64_t expected=music_pitch_light(music_light_snapshot(&untransposed,-2),pitch,&replace);
  assert(actual==expected);
 }
 music_pitch_light(independent,121,&replace);assert(!replace);
 music_pad_light(independent,0,-1,&replace);assert(!replace);
 s.mode=MUSIC_DEGREES;s.original_colour=15;music_publish(&s,0);assert(!music_published());
 for(unsigned i=0;i<128;++i)assert(music_sequence_colour(i)==0&&music_set_sequence_colour(i,i%5));
 assert(!music_set_sequence_colour(128,1)&&!music_set_sequence_colour(0,MUSIC_SEQUENCE_COLOURS));
 struct music_project p,q;music_snapshot(&p);static char text[MUSIC_PAYLOAD_BYTES];int n=music_encode(&p,text,sizeof(text));assert(n>0&&!memcmp(text,"MPCS15",6));assert(music_decode(text,(size_t)n,&q));assert(q.samples[0].mode==MUSIC_DEGREES&&q.samples[0].original_colour==4);
 for(unsigned i=0;i<128;++i)assert(q.sequence_colours[i]==i%5);
 assert(!music_decode(text,(size_t)n-1,&q));text[n-2]='5';assert(!music_decode(text,(size_t)n,&q));text[n-2]='2';
 text[5]='4';assert(music_decode(text,(size_t)n-136,&q));for(unsigned i=0;i<128;++i)assert(!q.sequence_colours[i]);text[5]='5';
 for(unsigned i=0;i<128;++i)assert(music_set_sequence_colour(i,i%MUSIC_SEQUENCE_COLOURS));
 music_snapshot(&p);n=music_encode(&p,text,sizeof(text));assert(n>0&&!memcmp(text,"MPCS16",6)&&music_decode(text,(size_t)n,&q));
 for(unsigned i=0;i<128;++i)assert(q.sequence_colours[i]==i%MUSIC_SEQUENCE_COLOURS);
 text[n-2]='f';assert(!music_decode(text,(size_t)n,&q));
 const char old[]="MPCS12 0 0 1\n0 3 0 0 1 10 -1 -1\n42617373\n";assert(music_decode(old,sizeof(old)-1,&q)&&q.samples[0].original_colour==4&&q.samples[0].mode==MUSIC_SCALE);
 music_reset();for(unsigned i=0;i<MUSIC_MAX_SAMPLES;++i){char name[32];snprintf(name,sizeof(name),"sample%u",i);assert(music_set_sample(name,MUSIC_MODE,MUSIC_DEGREES));}
 assert(!music_set_sample("overflow",MUSIC_ROOT,0));music_snapshot(&p);n=music_encode(&p,text,sizeof(text));assert(n>0&&music_decode(text,(size_t)n,&q)&&q.count==MUSIC_MAX_SAMPLES);
 puts("PASS music Lights and128 sequence colours: twelve-colour roundtrip, legacy five-colour IDs retained, invalid/truncated rejection, MPCS12/14/15 migration, full-capacity persistence");
}
