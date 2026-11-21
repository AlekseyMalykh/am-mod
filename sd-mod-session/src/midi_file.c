#include "midi_file.h"
#include "midi_tools.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int error_text(char e[96],const char*s){if(e)snprintf(e,96,"%s",s);return 0;}
void am_smf_init(struct am_smf*d){memset(d,0,sizeof(*d));d->tracks=1;d->division=960;}
void am_smf_free(struct am_smf*d){if(d){free(d->notes);free(d->events);am_smf_init(d);}}
static int grow(void**p,unsigned*cap,unsigned need,unsigned limit,size_t size){
 if(need>limit)return 0;
 if(need<=*cap)return 1;
 unsigned n=*cap?*cap*2:64;if(n<need)n=need;if(n>limit)n=limit;
 void*q=realloc(*p,n*size);if(!q)return 0;*p=q;*cap=n;return 1;
}
int am_smf_note_add(struct am_smf*d,struct am_smf_note n){
 if(!d||n.track>=AM_SMF_MAX_TRACKS||n.channel>=16||n.note>127||!n.velocity||n.velocity>127||n.duration>UINT32_MAX-n.tick)return 0;
 if(!grow((void**)&d->notes,&d->capacity,d->count+1,AM_SMF_MAX_NOTES,sizeof(n)))return 0;
 d->notes[d->count++]=n;if(n.track>=d->tracks)d->tracks=n.track+1;
 if(n.tick+n.duration>d->duration)d->duration=n.tick+n.duration;
 return 1;
}
int am_smf_event_add(struct am_smf*d,struct am_smf_event e){
 if(!d||e.track>=AM_SMF_MAX_TRACKS||e.kind>AM_SMF_PORT)return 0;
 if(e.kind==AM_SMF_MESSAGE&&(e.size<2||e.size>3||e.data[0]<0x80||e.data[0]>=0xf0||e.data[1]>127||(e.size==3&&e.data[2]>127)))return 0;
 if(e.kind==AM_SMF_TEMPO&&(!e.value||e.value>0xffffff))return 0;
 if(e.kind==AM_SMF_SIGNATURE&&(!(e.value>>24)||((e.value>>16)&255)>7))return 0;
 if(e.kind==AM_SMF_PORT&&e.value>127)return 0;
 if(!grow((void**)&d->events,&d->event_capacity,d->event_count+1,AM_SMF_MAX_EVENTS,sizeof(e)))return 0;
 d->events[d->event_count++]=e;if(e.track>=d->tracks)d->tracks=e.track+1;if(e.tick>d->duration)d->duration=e.tick;return 1;
}
struct reader {const unsigned char *p,*end;};
static int take(struct reader*r,size_t n,const unsigned char**p){if(n>(size_t)(r->end-r->p))return 0;*p=r->p;r->p+=n;return 1;}
static uint32_t be32(const unsigned char*p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static unsigned be16(const unsigned char*p){return (unsigned)p[0]<<8|p[1];}
static int vlq(struct reader*r,uint32_t*out){uint32_t n=0;for(unsigned i=0;i<4;++i){if(r->p==r->end)return 0;unsigned b=*r->p++;n=(n<<7)|(b&127);if(!(b&128)){*out=n;return 1;}}return 0;}
int am_smf_read(const void*bytes,size_t size,struct am_smf*out,char e[96]){
 if(!bytes||!out||size>AM_SMF_MAX_BYTES||size<14)return error_text(e,"Invalid MIDI file size");
 struct reader r={(const unsigned char*)bytes,(const unsigned char*)bytes+size};const unsigned char*p;
 if(!take(&r,8,&p)||memcmp(p,"MThd",4))return error_text(e,"Missing MIDI header");
 uint32_t h=be32(p+4);if(h<6||!take(&r,h,&p))return error_text(e,"Truncated MIDI header");
 unsigned format=be16(p),tracks=be16(p+2),division=be16(p+4);
 if(format>1)return error_text(e,"SMF format 2 is not supported");
 if(!tracks||tracks>AM_SMF_MAX_TRACKS||(format==0&&tracks!=1)||!division||(division&0x8000))return error_text(e,"Use MIDI format 0/1 with PPQN timing");
 struct am_smf d;am_smf_init(&d);d.tracks=tracks;d.division=division;
 int *next=malloc(AM_SMF_MAX_NOTES*sizeof(int));const char*failure="Malformed or truncated MIDI track";
 if(!next){failure="MIDI allocation failed";goto fail;}
 for(unsigned track=0;track<tracks;++track){
  if(!take(&r,8,&p)||memcmp(p,"MTrk",4))goto fail;
  uint32_t len=be32(p+4);if(!take(&r,len,&p))goto fail;
  struct reader t={p,p+len};int heads[16][128],tails[16][128];
  memset(heads,0xff,sizeof(heads));memset(tails,0xff,sizeof(tails));
  unsigned status=0,ended=0;uint32_t tick=0;
  while(t.p<t.end){
   uint32_t delta;if(!vlq(&t,&delta)||delta>UINT32_MAX-tick)goto fail;tick+=delta;
   if(t.p==t.end)goto fail;
   unsigned s=*t.p;if(s&128){++t.p;if(s<0xf0)status=s;else status=0;}else {if(!status)goto fail;s=status;}
   if(s==0xff){
    if(t.p==t.end)goto fail;
    unsigned type=*t.p++;uint32_t n;if(!vlq(&t,&n)||!take(&t,n,&p))goto fail;
    struct am_smf_event meta={.tick=tick,.track=(uint16_t)track};
    if(type==0x2f){if(n)goto fail;ended=1;break;}
    if(type==0x51){if(n!=3)goto fail;meta.kind=AM_SMF_TEMPO;meta.value=(uint32_t)p[0]<<16|(uint32_t)p[1]<<8|p[2];if(!am_smf_event_add(&d,meta))goto limit;}
    else if(type==0x58){if(n!=4)goto fail;meta.kind=AM_SMF_SIGNATURE;meta.value=be32(p);if(!am_smf_event_add(&d,meta))goto limit;}
    else if(type==0x21){if(n!=1)goto fail;meta.kind=AM_SMF_PORT;meta.value=p[0];if(!am_smf_event_add(&d,meta))goto limit;}
   }else if(s==0xf0||s==0xf7){uint32_t n;if(!vlq(&t,&n)||!take(&t,n,&p))goto fail;}
   else if(s>=0x80&&s<0xf0){
    unsigned kind=s&0xf0,ch=s&15,n=kind==0xc0||kind==0xd0?1:2;
    if(!take(&t,n,&p)||p[0]>127||(n==2&&p[1]>127))goto fail;
    if(kind==0x90&&p[1]){
     unsigned index=d.count;struct am_smf_note note={tick,0,(uint16_t)track,(uint8_t)ch,p[0],p[1]};
     if(!am_smf_note_add(&d,note))goto limit;
     next[index]=-1;
     if(tails[ch][p[0]]>=0)next[tails[ch][p[0]]]=(int)index;else heads[ch][p[0]]=(int)index;
     tails[ch][p[0]]=(int)index;
    }else if(kind==0x80||(kind==0x90&&!p[1])){
     int at=heads[ch][p[0]];
     if(at>=0){d.notes[at].duration=tick-d.notes[at].tick;heads[ch][p[0]]=next[at];if(heads[ch][p[0]]<0)tails[ch][p[0]]=-1;}
     else ++d.unmatched_off;
    }else {struct am_smf_event message={.tick=tick,.track=(uint16_t)track,.kind=AM_SMF_MESSAGE,.size=(uint8_t)(n+1),.data={(uint8_t)s,p[0],n==2?p[1]:0}};if(!am_smf_event_add(&d,message))goto limit;}
   }else goto fail;
  }
  if(!ended)goto fail;
  for(unsigned ch=0;ch<16;++ch)for(unsigned note=0;note<128;++note)for(int at=heads[ch][note];at>=0;at=next[at])d.notes[at].duration=tick-d.notes[at].tick;
  if(tick>d.duration)d.duration=tick;
 }
 if(r.p!=r.end){failure="Unexpected data after MIDI tracks";goto fail;}
 free(next);am_smf_free(out);*out=d;if(e)*e=0;return 1;
limit:failure="Invalid MIDI event or file exceeds capacity";
fail:free(next);am_smf_free(&d);return error_text(e,failure);
}
struct buffer {unsigned char *p;size_t size,cap;};
static int append(struct buffer*b,const void*p,size_t n){
 if(n>AM_SMF_MAX_BYTES-b->size)return 0;
 if(b->size+n>b->cap){size_t cap=b->cap?b->cap*2:4096;if(cap<b->size+n)cap=b->size+n;if(cap>AM_SMF_MAX_BYTES)cap=AM_SMF_MAX_BYTES;void*q=realloc(b->p,cap);if(!q)return 0;b->p=q;b->cap=cap;}
 memcpy(b->p+b->size,p,n);b->size+=n;return 1;
}
static int put32(struct buffer*b,uint32_t n){unsigned char p[4]={(unsigned char)(n>>24),(unsigned char)(n>>16),(unsigned char)(n>>8),(unsigned char)n};return append(b,p,4);}
static int putvlq(struct buffer*b,uint32_t n){unsigned char p[4];unsigned at=4;p[--at]=n&127;while((n>>=7)!=0)p[--at]=(n&127)|128;return append(b,p+at,4-at);}
struct wire_event {struct am_smf_event e;unsigned priority,ordinal;};
static int wire_less(const void*a,const void*b){const struct wire_event*x=a,*y=b;
 if(x->e.track!=y->e.track)return x->e.track<y->e.track?-1:1;
 if(x->e.tick!=y->e.tick)return x->e.tick<y->e.tick?-1:1;
 if(x->priority!=y->priority)return x->priority<y->priority?-1:1;
 return x->ordinal==y->ordinal?0:x->ordinal<y->ordinal?-1:1;
}
static int delta_write(struct buffer*b,uint32_t n){
 while(n>0x0fffffffu){const unsigned char spacer[]={0xff,0x7f,0};if(!putvlq(b,0x0fffffffu)||!append(b,spacer,3))return 0;n-=0x0fffffffu;}
 return putvlq(b,n);
}
int am_smf_write(const struct am_smf*d,unsigned char**out,size_t*out_size,char error[96]){
 if(!d||!out||!out_size||!d->division||d->division>32767||!d->tracks||d->tracks>AM_SMF_MAX_TRACKS||d->count>AM_SMF_MAX_NOTES||d->event_count>AM_SMF_MAX_EVENTS)return error_text(error,"Invalid MIDI export document");
 size_t count=(size_t)d->count*2+d->event_count;
 struct wire_event *events=malloc((count?count:1)*sizeof(*events));struct buffer b={0};
 if(!events)return error_text(error,"MIDI export allocation failed");
 size_t used=0;uint32_t end=d->duration;
 for(unsigned i=0;i<d->event_count;++i){if(d->events[i].track>=d->tracks)goto fail;events[used]=(struct wire_event){d->events[i],1,(unsigned)used};++used;}
 for(unsigned i=0;i<d->count;++i){const struct am_smf_note*n=d->notes+i;
  if(n->track>=d->tracks||n->channel>15||n->note>127||!n->velocity||n->velocity>127||n->duration>UINT32_MAX-n->tick)goto fail;
  struct am_smf_event on={.tick=n->tick,.track=n->track,.kind=AM_SMF_MESSAGE,.size=3,.data={(uint8_t)(0x90|n->channel),n->note,n->velocity}};
  struct am_smf_event off=on;off.tick+=n->duration;off.data[0]=(uint8_t)(0x80|n->channel);off.data[2]=0;
  if(!n->duration&&off.tick<UINT32_MAX)++off.tick;
  if(off.tick>end)end=off.tick;
  events[used]=(struct wire_event){off,0,(unsigned)used};++used;events[used]=(struct wire_event){on,2,(unsigned)used};++used;
 }
 qsort(events,used,sizeof(*events),wire_less);
 unsigned char header[6]={0,(uint8_t)(d->tracks>1), (uint8_t)(d->tracks>>8),(uint8_t)d->tracks,(uint8_t)(d->division>>8),(uint8_t)d->division};
 if(!append(&b,"MThd",4)||!put32(&b,6)||!append(&b,header,6))goto fail;
 size_t at=0;
 for(unsigned track=0;track<d->tracks;++track){
  if(!append(&b,"MTrk",4))goto fail;
  size_t length_at=b.size;if(!put32(&b,0))goto fail;size_t start=b.size;uint32_t previous=0;
  while(at<used&&events[at].e.track==track){struct am_smf_event e=events[at++].e;
   if(e.tick<previous||!delta_write(&b,e.tick-previous))goto fail;
   previous=e.tick;
   if(e.kind==AM_SMF_MESSAGE){if(e.size<2||e.size>3||!append(&b,e.data,e.size))goto fail;}
   else if(e.kind==AM_SMF_TEMPO){unsigned char p[]={0xff,0x51,3,(uint8_t)(e.value>>16),(uint8_t)(e.value>>8),(uint8_t)e.value};if(!e.value||e.value>0xffffff||!append(&b,p,sizeof(p)))goto fail;}
   else if(e.kind==AM_SMF_SIGNATURE){unsigned char p[]={0xff,0x58,4,(uint8_t)(e.value>>24),(uint8_t)(e.value>>16),(uint8_t)(e.value>>8),(uint8_t)e.value};if(!append(&b,p,sizeof(p)))goto fail;}
   else if(e.kind==AM_SMF_PORT){unsigned char p[]={0xff,0x21,1,(uint8_t)e.value};if(!append(&b,p,sizeof(p)))goto fail;}
   else goto fail;
  }
  uint32_t last=end>previous?end:previous;const unsigned char eot[]={0xff,0x2f,0};if(!delta_write(&b,last-previous)||!append(&b,eot,3))goto fail;
  uint32_t size=(uint32_t)(b.size-start);b.p[length_at]=(unsigned char)(size>>24);b.p[length_at+1]=(unsigned char)(size>>16);b.p[length_at+2]=(unsigned char)(size>>8);b.p[length_at+3]=(unsigned char)size;
 }
 free(events);*out=b.p;*out_size=b.size;if(error)*error=0;return 1;
fail:free(events);free(b.p);return error_text(error,"MIDI export failed or exceeds capacity");
}
uint64_t am_smf_native_tick(uint32_t tick,unsigned division){return division&&division<32768?((uint64_t)tick*960+division/2)/division:UINT64_MAX;}
int am_smf_import(const struct am_smf*d,const struct am_smf_import_options*o,const int native[128],struct am_smf_pad_note*out,unsigned capacity,unsigned*out_count,struct am_smf_import_info*info,char error[96]){
 if(!d||!o||!native||!out||!out_count||!info||o->mode>AM_SMF_CHOP||o->sound>127||o->port>2||!o->length||!d->division||d->division>32767)return error_text(error,"Invalid MIDI import options");
 if(o->mode==AM_SMF_TUNE&&o->original_note<0)return error_text(error,"Set Orig Note before importing Tune");
 if(o->mode==AM_SMF_CHOP&&o->chops<2)return error_text(error,"Split sample before importing Chop");
 struct am_midi_map maps[128];for(unsigned p=0;p<128;++p)if(!am_midi_map_get(p,maps+p))return error_text(error,"MIDI map unavailable");
 struct am_smf_pad_note *plan=malloc((d->count?d->count:1)*sizeof(*plan));if(!plan)return error_text(error,"MIDI import allocation failed");
 struct am_smf_import_info stats={0};unsigned count=0;
 for(unsigned i=0;i<d->event_count;++i)if(d->events[i].kind==AM_SMF_MESSAGE)++stats.controllers;
 for(unsigned i=0;i<d->count;++i){const struct am_smf_note*n=d->notes+i;
  if((o->track>=0&&(unsigned)o->track!=n->track)||(o->channel>=0&&(unsigned)o->channel!=n->channel)){++stats.filtered;continue;}
  int sound=-1,tune=0,slice=0;
  if(o->mode==AM_SMF_PAD_MAP){
   for(unsigned p=0;p<128;++p)if(maps[p].in_kind==AM_MIDI_INPUT_NOTE&&maps[p].in_number==n->note&&(!maps[p].in_channel||maps[p].in_channel==n->channel+1)&&(!o->port||!maps[p].in_port||maps[p].in_port==o->port)){
    if(sound>=0){sound=-2;break;}sound=(int)p;
   }
   if(sound==-1)for(unsigned p=0;p<128;++p)if(native[p]==n->note){if(sound>=0){sound=-2;break;}sound=(int)p;}
  }else {sound=(int)o->sound;const struct am_midi_map*m=maps+sound;
   int base=m->in_kind==AM_MIDI_INPUT_NOTE?m->in_number:native[sound];
   if(base<0||base>127){++stats.unmapped;continue;}
   if((m->in_channel&&m->in_channel!=n->channel+1)||(o->port&&m->in_port&&m->in_port!=o->port)){++stats.filtered;continue;}
   if(o->mode==AM_SMF_TUNE)tune=(int)n->note-base;else slice=(int)n->note-base+1;
  }
  if(sound==-2){++stats.ambiguous;continue;}if(sound<0){++stats.unmapped;continue;}
  uint64_t tick=am_smf_native_tick(n->tick,d->division),end=am_smf_native_tick(n->tick+n->duration,d->division);
  if(o->fit&&d->duration){tick=(uint64_t)n->tick*o->length/d->duration;end=(uint64_t)(n->tick+n->duration)*o->length/d->duration;if(tick>=o->length)tick=o->length-1;}
  if(end<=tick)end=tick+1;
  if(end>stats.end)stats.end=end;
  if(tick>=o->length||end>o->length||tune< -120||tune>120||(o->mode==AM_SMF_CHOP&&(slice<1||(unsigned)slice>o->chops))){++stats.out_of_range;continue;}
  plan[count++]=(struct am_smf_pad_note){(uint32_t)tick,(uint32_t)(end-tick),(int16_t)tune,(uint16_t)slice,(uint8_t)sound,n->channel,n->velocity};
 }
 stats.accepted=count;*info=stats;
 const char*failure=stats.ambiguous?"Ambiguous MIDI map: select source port":!o->skip_unmapped&&(stats.unmapped||stats.out_of_range)?"Unmapped/outside sequence: adjust map or Fit":count>capacity?"Imported notes exceed editor capacity":!count?"No matching MIDI notes":NULL;
 if(failure){free(plan);return error_text(error,failure);}
 memcpy(out,plan,count*sizeof(*plan));*out_count=count;free(plan);if(error)*error=0;return 1;
}
