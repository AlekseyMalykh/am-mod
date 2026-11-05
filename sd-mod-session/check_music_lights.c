#include "src/music_settings.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t light(struct music_sample*s,unsigned pad,int reference){int replace=0;music_publish(s,0);uint64_t v=music_pad_light(music_published(),pad,reference,&replace);assert(replace);return (uint32_t)v;}
int main(void){
 music_reset();struct music_sample s;music_get_sample("Bass",&s);assert(s.root==-1&&s.original_colour==4);
 assert(MUSIC_SCALE==3&&MUSIC_DEGREES==4&&music_mode_ui_index(MUSIC_MONO)==0&&music_mode_ui_index(MUSIC_PIANO)==1&&music_mode_ui_index(MUSIC_SCALE)==2);
 /* r25f: Tonic is no longer offered and shows/lights as Scale. r27q: ID 4 is the Degrees layout. */
 assert(MUSIC_LAYOUT_CHOICES==4&&music_mode_ui_index(MUSIC_TONIC)==2&&music_mode_ui_index(MUSIC_DEGREES)==3&&music_mode_from_ui_index(3)==MUSIC_DEGREES&&music_mode_from_ui_index(9)==MUSIC_DEGREES&&music_mode_from_ui_index(2)==MUSIC_SCALE&&music_mode_from_ui_index(1)==MUSIC_PIANO&&music_mode_from_ui_index(0)==MUSIC_MONO);
 /* Main degrees I, IV and V by scale ordinal; outside the scale is -1. */
 {static const unsigned masks[8]={0xab5,0x5ad,0x6ad,0x5ab,0xad5,0x6b5,0x56b,0xfff};
  static const int fourth[8]={5,5,5,5,6,5,5,5},fifth[8]={7,7,7,7,7,7,6,7};
  for(unsigned k=0;k<8;++k)for(int d=0;d<12;++d){int r=music_degree_role(masks[k],d);
   if(!(masks[k]&(1u<<d)))assert(r==-1);else if(!d)assert(r==0);else assert(r==(d==fourth[k]?1:d==fifth[k]?3:2));}
  assert(music_degree_role(0xab5,-1)==-1&&music_degree_role(0xab5,12)==-1);}
 assert(music_set_sample("Bass",MUSIC_ROOT,0)&&music_get_sample("Bass",&s));
 assert(s.root==0&&s.mode==MUSIC_SCALE);
 assert(music_set_sample("Bass",MUSIC_MODE,MUSIC_DEGREES));music_get_sample("Bass",&s);
 for(int t=-24;t<=24;++t){s.transpose=t;music_publish(&s,0);assert(music_transposition()==t);}
 s.transpose=0;assert(light(&s,0,0)==music_colours[4]);
 {struct music_sample scale=s;scale.mode=MUSIC_SCALE;struct music_sample tonic=s;tonic.mode=MUSIC_TONIC;
  assert(music_light_snapshot(&s,0)!=music_light_snapshot(&scale,0)&&music_light_snapshot(&tonic,0)==music_light_snapshot(&scale,0));
  /* C Major, reference C on pad 0: Scale keeps every pad lit. */
  assert(light(&scale,1,0)==music_colours[10]&&light(&scale,2,0)==music_colours[1]&&light(&scale,12,0)==music_colours[0]);}
 /* Degrees: key notes only; I = Tonic slot, IV/V = second slot, other key notes = third slot. */
 {int r=0;music_publish(&s,0);uint64_t off=music_pad_light(music_published(),1,0,&r);assert(r&&off==0);}
 assert(light(&s,2,0)==music_colours[10]&&light(&s,4,0)==music_colours[10]&&light(&s,5,0)==music_colours[1]&&light(&s,7,0)==music_colours[MUSIC_DEFAULT_FIFTH_COLOUR]&&light(&s,9,0)==music_colours[10]&&light(&s,11,0)==music_colours[10]&&light(&s,12,0)==music_colours[0]);
 for(unsigned pad=1;pad<16;++pad){int r=0;music_publish(&s,0);uint64_t v=music_pad_light(music_published(),pad,0,&r);assert(r);
  if((0xab5u>>(pad%12))&1u)assert((uint32_t)v!=0);else assert(v==0);}
 {struct music_sample minor=s;minor.scale=1;minor.tonic=9; /* A minor local key */
  assert(light(&minor,9,0)==music_colours[0]&&light(&minor,2,0)==music_colours[1]&&light(&minor,4,0)==music_colours[MUSIC_DEFAULT_FIFTH_COLOUR]&&light(&minor,11,0)==music_colours[10]);
  int r=0;music_publish(&minor,0);assert(music_pad_light(music_published(),10,0,&r)==0&&r);}
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
 assert(!music_pad_colours_used()&&music_set_pad_colour(5,3)&&music_set_pad_colour(127,12)&&!music_set_pad_colour(128,1)&&!music_set_pad_colour(1,MUSIC_SEQUENCE_COLOURS));
 music_snapshot(&p);n=music_encode(&p,text,sizeof(text));assert(n>0&&!memcmp(text,"MPCS17",6)&&music_decode(text,(size_t)n,&q));
 assert(q.pad_colours[5]==3&&q.pad_colours[127]==12&&!q.pad_colours[0]&&q.sequence_colours[1]==1&&music_pad_colour(5)==3&&music_pad_colours_used());
 assert(!music_decode(text,(size_t)n-1,&q));text[n-2]='f';assert(!music_decode(text,(size_t)n,&q));
 assert(music_set_pad_colour(5,0)&&music_set_pad_colour(127,0)&&!music_pad_colours_used());
 const char old[]="MPCS12 0 0 1\n0 3 0 0 1 10 -1 -1\n42617373\n";assert(music_decode(old,sizeof(old)-1,&q)&&q.samples[0].original_colour==4&&q.samples[0].mode==MUSIC_SCALE);
 /* V colour: separate slot, version 18 only when it differs from the default. */
 music_reset();assert(music_set_sample("Keys",MUSIC_ROOT,0)&&music_set_sample("Keys",MUSIC_MODE,MUSIC_DEGREES));
 music_snapshot(&p);n=music_encode(&p,text,sizeof(text));assert(n>0&&!memcmp(text,"MPCS15",6));
 assert(!music_set_sample("Keys",MUSIC_FIFTH_COLOUR,12)&&music_set_sample("Keys",MUSIC_FIFTH_COLOUR,6));
 music_get_sample("Keys",&s);assert(s.fifth_colour==6&&light(&s,7,0)==music_colours[6]&&light(&s,5,0)==music_colours[1]&&light(&s,12,0)==music_colours[0]&&light(&s,0,0)==music_colours[4]);
 {struct music_sample scale=s;scale.mode=MUSIC_SCALE;assert(light(&scale,7,0)==music_colours[1]);} /* Scale ignores the V slot. */
 music_snapshot(&p);n=music_encode(&p,text,sizeof(text));assert(n>0&&!memcmp(text,"MPCS18",6)&&music_decode(text,(size_t)n,&q));
 assert(q.count==1&&q.samples[0].fifth_colour==6&&q.samples[0].original_colour==4&&q.samples[0].mode==MUSIC_DEGREES);
 assert(!music_decode(text,(size_t)n-1,&q));
 {static char bad[MUSIC_PAYLOAD_BYTES];memcpy(bad,text,(size_t)n);char*f=strstr(bad," 4 6\n");assert(f);f[3]='9';f[4]='9';assert(!music_decode(bad,(size_t)n,&q));}
 music_reset();for(unsigned i=0;i<MUSIC_MAX_SAMPLES;++i){char name[32];snprintf(name,sizeof(name),"sample%u",i);assert(music_set_sample(name,MUSIC_MODE,MUSIC_DEGREES));}
 assert(!music_set_sample("overflow",MUSIC_ROOT,0));music_snapshot(&p);n=music_encode(&p,text,sizeof(text));assert(n>0&&music_decode(text,(size_t)n,&q)&&q.count==MUSIC_MAX_SAMPLES);
 puts("PASS music Lights (Monochrome/Piano/Scale/Degrees) and128 sequence colours: twelve-colour roundtrip, legacy five-colour IDs retained, invalid/truncated rejection, MPCS12/14/15 migration, full-capacity persistence");
}
