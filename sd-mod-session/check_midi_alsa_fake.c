#include <stddef.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sound/asequencer.h>
struct raw{int port,dir;};static struct raw handles[12];static unsigned used;
unsigned char fake_input[8192],fake_output[8192];unsigned fake_in_size,fake_in_at,fake_out_size;long fake_limit=8192,fake_write_error;
int snd_rawmidi_open(void**in,void**out,const char*name,int mode){(void)mode;int port=!strcmp(name,"surface")?3:!strcmp(name,"external")?2:1;if(in){handles[used]=(struct raw){port,0};*in=handles+used++;}if(out){handles[used]=(struct raw){port,1};*out=handles+used++;}return 0;}
int snd_rawmidi_close(void*h){(void)h;return 0;}
size_t snd_rawmidi_info_sizeof(void){return sizeof(struct raw);}
int snd_rawmidi_info(void*h,void*out){memcpy(out,h,sizeof(struct raw));return 0;}
const char*snd_rawmidi_info_get_id(const void*p){return ((const struct raw*)p)->port==1?"MIDI Gadget":"SerialMIDI";}
const char*snd_rawmidi_info_get_name(const void*p){return ((const struct raw*)p)->port==1?"MPC Sample":((const struct raw*)p)->port==2?"External MIDI":"Control Surface";}
long snd_rawmidi_read(void*h,void*out,size_t size){(void)h;if(fake_in_at==fake_in_size)return -EAGAIN;size_t n=fake_in_size-fake_in_at;if(n>size)n=size;memcpy(out,fake_input+fake_in_at,n);fake_in_at+=(unsigned)n;return (long)n;}
long snd_rawmidi_tread(void*h,struct timespec*t,void*out,size_t n){t->tv_sec=123;t->tv_nsec=0;return snd_rawmidi_read(h,out,n);}
long snd_rawmidi_write(void*h,const void*data,size_t n){(void)h;if(fake_write_error)return fake_write_error;if(n>(size_t)fake_limit)n=(size_t)fake_limit;if(fake_out_size+n>sizeof fake_output)return -ENOSPC;memcpy(fake_output+fake_out_size,data,n);fake_out_size+=(unsigned)n;return (long)n;}
int snd_seq_connect_from(void*h,int p,int c,int s){(void)h;(void)p;(void)c;(void)s;return 0;}
int snd_seq_connect_to(void*h,int p,int c,int s){(void)h;(void)p;(void)c;(void)s;return 0;}
int snd_seq_close(void*h){(void)h;return 0;}
size_t snd_seq_client_info_sizeof(void){return 4;}
int snd_seq_get_any_client_info(void*h,int c,void*p){(void)h;memcpy(p,&c,4);return 0;}
const char*snd_seq_client_info_get_name(const void*p){return *(const int*)p==20?"MIDI Gadget":"External MIDI";}
struct snd_seq_event fake_seq_in,fake_seq_out;int fake_seq_result;
int snd_seq_event_input(void*h,struct snd_seq_event**out){(void)h;*out=&fake_seq_in;return fake_seq_result;}
int snd_seq_event_output_direct(void*h,struct snd_seq_event*e){(void)h;if(fake_seq_result>=0)fake_seq_out=*e;return fake_seq_result;}
