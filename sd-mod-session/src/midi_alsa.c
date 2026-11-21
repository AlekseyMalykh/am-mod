#define _GNU_SOURCE
#include "midi_tools.h"
#include "midi_wire.h"
extern int am_midi_wire_current(struct am_midi_wire*) __attribute__((weak));
extern int am_midi_wire_send(struct am_midi_voices*,const struct am_midi_wire*,unsigned,const struct am_midi_message*,am_midi_queue,void*) __attribute__((weak));
extern int am_midi_voices_panic(struct am_midi_voices*,am_midi_queue,void*) __attribute__((weak));
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sound/asequencer.h>
#include <stdio.h>
/* Observe the application's existing ALSA owners, never open a competing
 * reader. The control surface is excluded by identity before decoding. */
typedef void raw_t;
static int(*open_real)(raw_t**,raw_t**,const char*,int),(*close_real)(raw_t*);
static long(*read_real)(raw_t*,void*,size_t),(*tread_real)(raw_t*,struct timespec*,void*,size_t),(*write_real)(raw_t*,const void*,size_t);
static size_t(*info_size)(void);
static int(*info_real)(raw_t*,void*);
static const char*(*info_id)(const void*),*(*info_name)(const void*);
static int(*seq_input)(void*,struct snd_seq_event**),(*seq_output)(void*,struct snd_seq_event*);
static int(*connect_from)(void*,int,int,int),(*connect_to)(void*,int,int,int),(*seq_close)(void*);
static size_t(*client_size)(void);
static int(*client_info)(void*,int,void*);
static const char*(*client_name)(const void*);
static pthread_once_t once=PTHREAD_ONCE_INIT;
static void resolve(void){
#define R(name,member) *(void**)(&member)=dlsym(RTLD_NEXT,name)
 R("snd_rawmidi_open",open_real);R("snd_rawmidi_close",close_real);R("snd_rawmidi_read",read_real);R("snd_rawmidi_tread",tread_real);R("snd_rawmidi_write",write_real);
 R("snd_rawmidi_info_sizeof",info_size);R("snd_rawmidi_info",info_real);R("snd_rawmidi_info_get_id",info_id);R("snd_rawmidi_info_get_name",info_name);
 R("snd_seq_event_input",seq_input);R("snd_seq_event_output_direct",seq_output);R("snd_seq_connect_from",connect_from);R("snd_seq_connect_to",connect_to);R("snd_seq_close",seq_close);
 R("snd_seq_client_info_sizeof",client_size);R("snd_seq_get_any_client_info",client_info);R("snd_seq_client_info_get_name",client_name);
#undef R
}
void am_midi_io_ready(void){pthread_once(&once,resolve);}
static unsigned identity(const char*id,const char*name){
 if(id&&!strcmp(id,"SerialMIDI"))return name&&!strcmp(name,"Control Surface")?AM_MIDI_SURFACE:name&&!strcmp(name,"External MIDI")?AM_MIDI_EXTERNAL:AM_MIDI_OTHER;
 if(name&&(!strcmp(name,"Control Surface")))return AM_MIDI_SURFACE;
 if(name&&!strcmp(name,"External MIDI"))return AM_MIDI_EXTERNAL;
 if((id&&(!strcmp(id,"MIDI Gadget")||!strcmp(id,"f_midi")||!strcmp(id,"MPCSample")))||
    (name&&(!strcmp(name,"MIDI Gadget")||!strcmp(name,"MPC Sample")||!strcmp(name,"MPC Sample MIDI")||!strcmp(name,"f_midi"))))return AM_MIDI_USB;
 return AM_MIDI_OTHER;
}
#define ENDPOINTS 16u
#define QUEUE_BYTES 24576u
#define INPUT_BYTES 2048u
struct stream {struct am_midi_parser parser,wire;struct am_midi_router router;
 struct am_midi_voices voices;struct am_midi_wire provenance;
 unsigned used,at,source_size;uint8_t queue[QUEUE_BYTES],source[INPUT_BYTES];unsigned port,direction;struct timespec timestamp;};
struct endpoint { _Atomic(uintptr_t) handle;_Atomic unsigned port;atomic_flag busy;struct stream stream;};
static struct endpoint endpoints[ENDPOINTS];
static void observe(void*u,const struct am_midi_message*m){struct stream*s=u;am_midi_observe(s->direction,s->port,m);}
static void append(struct stream*s,const void*p,unsigned n){if(n<=QUEUE_BYTES-s->used){memcpy(s->queue+s->used,p,n);s->used+=n;}}
static void panic_bytes(void*u,const struct am_midi_message*m){struct stream*s=u;append(s,m->data,m->size);}
static int wire_bytes(void*u,const struct am_midi_packet*p){struct stream*s=u;if(p->message.size>QUEUE_BYTES-s->used)return 0;append(s,p->message.data,p->message.size);return 1;}
static void translated(void*u,const struct am_midi_message*m){
 struct stream*s=u;struct am_midi_message out;
 if(s->direction==AM_MIDI_IN)am_midi_observe(s->direction,s->port,m);
 if(m->data[0]==0xf0)return; /* SysEx is passed byte-for-byte by feed(). */
 if(s->direction==AM_MIDI_OUT&&s->provenance.id&&am_midi_wire_send&&am_midi_wire_send(&s->voices,&s->provenance,s->port,m,wire_bytes,s))return;
 if(s->port>0&&s->port<3&&(m->data[0]&0xf0)==0xb0&&m->size==3&&(m->data[1]==120||m->data[1]==123)){
    if(am_midi_voices_panic)am_midi_voices_panic(&s->voices,wire_bytes,s);
    am_midi_router_panic(&s->router,m->data[0]&15,panic_bytes,s);
 }
 if(am_midi_translate(&s->router,s->direction,s->port,m,&out))append(s,out.data,out.size);
}
static void feed(struct stream*s,unsigned byte){
 int sysex=s->parser.sysex;
 if(byte==0xf0||(sysex&&byte<0xf8)){uint8_t b=(uint8_t)byte;append(s,&b,1);}
 am_midi_parse(&s->parser,byte,translated,s);
}
static struct endpoint*find(raw_t*h){for(unsigned i=0;i<ENDPOINTS;++i)if(atomic_load_explicit(&endpoints[i].handle,memory_order_acquire)==(uintptr_t)h)return endpoints+i;return NULL;}
static void register_raw(raw_t*h,unsigned direction){
 if(!h)return;
 unsigned port=AM_MIDI_OTHER;_Alignas(8) unsigned char info[1024]={0};const char*id="",*name="";
 if(info_size&&info_real&&info_id&&info_name&&info_size()<=sizeof info&&!info_real(h,info)){id=info_id(info);name=info_name(info);port=identity(id,name);}
 am_midi_io_identity(port,id,name);
 for(unsigned i=0;i<ENDPOINTS;++i){uintptr_t empty=0;if(atomic_compare_exchange_strong(&endpoints[i].handle,&empty,UINTPTR_MAX)){
    struct endpoint*e=endpoints+i;memset(&e->stream,0,sizeof e->stream);e->stream.port=port;e->stream.direction=direction;atomic_flag_clear(&e->busy);atomic_store(&e->port,port);atomic_store_explicit(&e->handle,(uintptr_t)h,memory_order_release);return;}}
}
int snd_rawmidi_open(raw_t**in,raw_t**out,const char*name,int mode){am_midi_io_ready();if(!open_real)return -ENOSYS;int r=open_real(in,out,name,mode);if(!r){if(in)register_raw(*in,AM_MIDI_IN);if(out)register_raw(*out,AM_MIDI_OUT);}return r;}
int snd_rawmidi_close(raw_t*h){am_midi_io_ready();struct endpoint*e=find(h);if(e)atomic_store_explicit(&e->handle,0,memory_order_release);return close_real?close_real(h):-ENOSYS;}
static long receive(raw_t*h,struct timespec*time,void*out,size_t size,int timestamped){
 am_midi_io_ready();if(!size)return 0;struct endpoint*e=find(h);
 if(!e||atomic_load(&e->port)==AM_MIDI_SURFACE)return timestamped?(tread_real?tread_real(h,time,out,size):-ENOSYS):(read_real?read_real(h,out,size):-ENOSYS);
 if(atomic_flag_test_and_set_explicit(&e->busy,memory_order_acquire))return -EAGAIN;
 struct stream*s=&e->stream;long result=-EAGAIN;
 if(s->used==s->at){s->used=s->at=0;uint8_t input[INPUT_BYTES];size_t want=size<sizeof input?size:sizeof input;
    long n=timestamped?(tread_real?tread_real(h,time,input,want):-ENOSYS):(read_real?read_real(h,input,want):-ENOSYS);
    result=n;if(n>0){if(timestamped&&time)s->timestamp=*time;for(long i=0;i<n;++i)feed(s,input[i]);}
 }
 if(s->used>s->at){size_t n=s->used-s->at;if(n>size)n=size;memcpy(out,s->queue+s->at,n);s->at+=(unsigned)n;result=(long)n;if(timestamped&&time)*time=s->timestamp;}
 else if(result>0)result=-EAGAIN; /* partial MIDI frame retained */
 atomic_flag_clear_explicit(&e->busy,memory_order_release);return result;
}
long snd_rawmidi_read(raw_t*h,void*out,size_t n){return receive(h,NULL,out,n,0);}
long snd_rawmidi_tread(raw_t*h,struct timespec*t,void*out,size_t n){return receive(h,t,out,n,1);}
long snd_rawmidi_write(raw_t*h,const void*input,size_t size){
 am_midi_io_ready();if(!write_real)return -ENOSYS;struct endpoint*e=find(h);
 if(!e||atomic_load(&e->port)==AM_MIDI_SURFACE)return write_real(h,input,size);
 if(!size)return 0;
 if(atomic_flag_test_and_set_explicit(&e->busy,memory_order_acquire))return -EAGAIN;
 struct stream*s=&e->stream;long result=-EAGAIN;
 if(s->source_size){if(size<s->source_size||memcmp(input,s->source,s->source_size)){result=-EBUSY;goto done;}}
 else{
    /* Return a short write at an input boundary for large SysEx buffers.
     * At most 3x expansion from running status fits the fixed staging area. */
    if(size>INPUT_BYTES)size=INPUT_BYTES;
    s->used=s->at=0;s->source_size=(unsigned)size;memcpy(s->source,input,size);
    memset(&s->provenance,0,sizeof s->provenance);
    if(am_midi_wire_current)am_midi_wire_current(&s->provenance);
    for(size_t i=0;i<size;++i)feed(s,((const uint8_t*)input)[i]);
 }
 if(s->at<s->used){long n=write_real(h,s->queue+s->at,s->used-s->at);
    if(n<0){result=n;goto done;}if(!n)goto done;
    for(long i=0;i<n;++i)am_midi_parse(&s->wire,s->queue[s->at+(unsigned)i],observe,s);
    s->at+=(unsigned)n;if(s->at<s->used)goto done;
 }
 result=s->source_size;s->source_size=s->used=s->at=0;
done: atomic_flag_clear_explicit(&e->busy,memory_order_release);return result;
}
/* ALSA sequencer ABI is Linux UAPI. Raw SerialMIDI and USB sequencer
 * callbacks share the same router/log without opening either device twice. */
struct seq_route { _Atomic(uintptr_t) handle;int remote,local;unsigned direction,port;atomic_flag busy;struct am_midi_router router;struct am_midi_voices voices;};
static struct seq_route seq_routes[32];
static void register_seq(void*h,int local,int remote,unsigned direction){
 _Alignas(8) unsigned char info[1024]={0};const char*name="";
 if(client_size&&client_info&&client_name&&client_size()<=sizeof info&&!client_info(h,remote,info))name=client_name(info);
 unsigned port=identity(NULL,name);am_midi_io_identity(port,"ALSA sequencer",name);
 fprintf(stderr,"AM_MIDI SEQ_ROUTE direction=%s local=%d remote_client=%d port=%s\n",direction==AM_MIDI_IN?"IN":"OUT",local,remote,am_midi_port_name(port));
 for(unsigned i=0;i<32;++i){uintptr_t empty=0;if(atomic_compare_exchange_strong(&seq_routes[i].handle,&empty,UINTPTR_MAX)){
    struct seq_route*r=seq_routes+i;r->local=local;r->remote=remote;r->direction=direction;r->port=port;memset(&r->router,0,sizeof r->router);memset(&r->voices,0,sizeof r->voices);atomic_flag_clear(&r->busy);atomic_store_explicit(&r->handle,(uintptr_t)h,memory_order_release);return;}}
}
int snd_seq_connect_from(void*h,int mine,int client,int port){am_midi_io_ready();int r=connect_from?connect_from(h,mine,client,port):-ENOSYS;if(r>=0)register_seq(h,mine,client,AM_MIDI_IN);return r;}
int snd_seq_connect_to(void*h,int mine,int client,int port){am_midi_io_ready();int r=connect_to?connect_to(h,mine,client,port):-ENOSYS;if(r>=0)register_seq(h,mine,client,AM_MIDI_OUT);return r;}
int snd_seq_close(void*h){am_midi_io_ready();for(unsigned i=0;i<32;++i)if(atomic_load(&seq_routes[i].handle)==(uintptr_t)h)atomic_store(&seq_routes[i].handle,0);return seq_close?seq_close(h):-ENOSYS;}
static struct seq_route*seq_find(void*h,const struct snd_seq_event*e,unsigned dir){
 for(unsigned i=0;i<32;++i){struct seq_route*r=seq_routes+i;if(atomic_load_explicit(&r->handle,memory_order_acquire)!=(uintptr_t)h||r->direction!=dir)continue;
    if(dir==AM_MIDI_IN?r->remote==e->source.client:r->local==e->source.port)return r;
 }return NULL;
}
static int from_seq(const struct snd_seq_event*e,struct am_midi_message*m){
 memset(m,0,sizeof(*m));m->size=3;
 switch(e->type){
 case SNDRV_SEQ_EVENT_NOTEON:case SNDRV_SEQ_EVENT_NOTEOFF:case SNDRV_SEQ_EVENT_KEYPRESS:
    m->data[0]=(uint8_t)((e->type==SNDRV_SEQ_EVENT_NOTEON?0x90:e->type==SNDRV_SEQ_EVENT_NOTEOFF?0x80:0xa0)|e->data.note.channel);m->data[1]=e->data.note.note;m->data[2]=e->data.note.velocity;break;
 case SNDRV_SEQ_EVENT_CONTROLLER:m->data[0]=(uint8_t)(0xb0|e->data.control.channel);m->data[1]=(uint8_t)e->data.control.param;m->data[2]=(uint8_t)e->data.control.value;break;
 case SNDRV_SEQ_EVENT_PGMCHANGE:case SNDRV_SEQ_EVENT_CHANPRESS:m->size=2;m->data[0]=(uint8_t)((e->type==SNDRV_SEQ_EVENT_PGMCHANGE?0xc0:0xd0)|e->data.control.channel);m->data[1]=(uint8_t)e->data.control.value;break;
 case SNDRV_SEQ_EVENT_PITCHBEND:{unsigned v=(unsigned)(e->data.control.value+8192);m->data[0]=(uint8_t)(0xe0|e->data.control.channel);m->data[1]=(uint8_t)(v&127);m->data[2]=(uint8_t)((v>>7)&127);break;}
 case SNDRV_SEQ_EVENT_CLOCK:m->size=1;m->data[0]=0xf8;break;
 case SNDRV_SEQ_EVENT_START:m->size=1;m->data[0]=0xfa;break;
 case SNDRV_SEQ_EVENT_CONTINUE:m->size=1;m->data[0]=0xfb;break;
 case SNDRV_SEQ_EVENT_STOP:m->size=1;m->data[0]=0xfc;break;
 case SNDRV_SEQ_EVENT_SENSING:m->size=1;m->data[0]=0xfe;break;
 case SNDRV_SEQ_EVENT_RESET:m->size=1;m->data[0]=0xff;break;
 case SNDRV_SEQ_EVENT_SYSEX:m->size=1;m->data[0]=0xf0;break;
 default:return 0;
 }
 return 1;
}
static void to_seq(struct snd_seq_event*e,const struct am_midi_message*m){
 unsigned kind=m->data[0]&0xf0,ch=m->data[0]&15;
 if(kind==0x80||kind==0x90||kind==0xa0){e->type=kind==0x80?SNDRV_SEQ_EVENT_NOTEOFF:kind==0xa0?SNDRV_SEQ_EVENT_KEYPRESS:SNDRV_SEQ_EVENT_NOTEON;memset(&e->data,0,sizeof e->data);e->data.note.channel=(uint8_t)ch;e->data.note.note=m->data[1];e->data.note.velocity=m->data[2];}
 else if(kind==0xb0||kind==0xc0){e->type=kind==0xb0?SNDRV_SEQ_EVENT_CONTROLLER:SNDRV_SEQ_EVENT_PGMCHANGE;memset(&e->data,0,sizeof e->data);e->data.control.channel=(uint8_t)ch;e->data.control.param=kind==0xb0?m->data[1]:0;e->data.control.value=kind==0xb0?m->data[2]:m->data[1];}
}
struct seq_panic {void*handle;struct snd_seq_event event;unsigned port;int error;};
static void panic_event(void*u,const struct am_midi_message*m){struct seq_panic*p=u;if(p->error)return;struct snd_seq_event event=p->event;to_seq(&event,m);int result=seq_output(p->handle,&event);if(result<0)p->error=result;else am_midi_observe(AM_MIDI_OUT,p->port,m);}
static int wire_seq(void*u,const struct am_midi_packet*m){struct seq_panic*p=u;panic_event(p,&m->message);return !p->error;}
int snd_seq_event_input(void*h,struct snd_seq_event**out){
 am_midi_io_ready();if(!seq_input)return -ENOSYS;
 for(unsigned attempt=0;attempt<64;++attempt){int result=seq_input(h,out);if(result<0||!out||!*out)return result;struct seq_route*r=seq_find(h,*out,AM_MIDI_IN);unsigned port=r?r->port:AM_MIDI_OTHER;struct am_midi_message in,m;
    if(!from_seq(*out,&in))return result;
    am_midi_observe(AM_MIDI_IN,port,&in);
    if(!r||port==AM_MIDI_SURFACE)return result;
    if(atomic_flag_test_and_set(&r->busy))return result;
    int emit=am_midi_translate(&r->router,AM_MIDI_IN,port,&in,&m);atomic_flag_clear(&r->busy);
    if(emit){if(memcmp(&in,&m,sizeof m))to_seq(*out,&m);return result;}
 }return -EAGAIN;
}
int snd_seq_event_output_direct(void*h,struct snd_seq_event*event){
 am_midi_io_ready();if(!seq_output)return -ENOSYS;struct seq_route*r=seq_find(h,event,AM_MIDI_OUT);unsigned port=r?r->port:AM_MIDI_OTHER;struct am_midi_message in,m;
 if(!from_seq(event,&in))return seq_output(h,event);
 struct am_midi_wire provenance;
 if(r&&port>=AM_MIDI_USB&&port<=AM_MIDI_EXTERNAL&&am_midi_wire_current&&am_midi_wire_send&&am_midi_wire_current(&provenance)){
    if(atomic_flag_test_and_set(&r->busy))return -EAGAIN;
    struct seq_panic p={h,*event,port,0};int owned=am_midi_wire_send(&r->voices,&provenance,port,&in,wire_seq,&p);
    atomic_flag_clear(&r->busy);
    if(owned)return p.error?p.error:owned<0?-EAGAIN:0;
 }
 m=in;struct snd_seq_event copy=*event;
 struct am_midi_router saved;int locked=0;
 if(r&&port!=AM_MIDI_SURFACE){if(atomic_flag_test_and_set(&r->busy))return -EAGAIN;locked=1;saved=r->router;
    if((in.data[0]&0xf0)==0xb0&&in.size==3&&(in.data[1]==120||in.data[1]==123)){
        struct seq_panic p={h,*event,port,0};am_midi_router_panic(&r->router,in.data[0]&15,panic_event,&p);
        if(!p.error&&am_midi_voices_panic)am_midi_voices_panic(&r->voices,wire_seq,&p);
        if(p.error){r->router=saved;atomic_flag_clear(&r->busy);return p.error;}
    }
    if(!am_midi_translate(&r->router,AM_MIDI_OUT,port,&in,&m)){atomic_flag_clear(&r->busy);return 0;}
    if(memcmp(&in,&m,sizeof m))to_seq(&copy,&m);
 }
 int result=seq_output(h,&copy);if(result>=0)am_midi_observe(AM_MIDI_OUT,port,&m);else if(locked)r->router=saved;
 if(locked)atomic_flag_clear(&r->busy);
 return result;
}
