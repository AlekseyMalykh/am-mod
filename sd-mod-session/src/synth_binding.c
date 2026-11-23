#include "synth_binding.h"
#include <string.h>
static size_t key_size(const char*s){
 if(!s)return 0;
 for(size_t i=0;i<AM_SYNTH_PROGRAM_KEY_BYTES;++i){unsigned c=(unsigned char)s[i];if(!c)return i;if(c<32||c==127)return 0;}
 return 0;
}
int am_synth_binding_valid(const struct am_synth_binding*b){
 if(!b||!key_size(b->program_key)||b->carrier>=128)return 0;
 for(unsigned i=0;i<AM_SYNTH_PARAMS;++i)if(!am_synth_param_value_valid(i,b->parameters[i]))return 0;
 return 1;
}
int am_synth_binding_capture(const char*key,unsigned carrier,struct am_synth_binding*out){
 size_t n=key_size(key);if(!n||carrier>=128||!out)return 0;
 struct am_synth_binding b={0};memcpy(b.program_key,key,n);b.carrier=(unsigned char)carrier;
 for(unsigned i=0;i<AM_SYNTH_PARAMS;++i)b.parameters[i]=(unsigned char)am_synth_param_get(i);
 if(!am_synth_binding_valid(&b))return 0;
 *out=b;return 1;
}
int am_synth_binding_matches(const struct am_synth_binding*b,const char*key,unsigned carrier){
 size_t n=key_size(key);return b&&n&&carrier<128&&carrier==b->carrier&&n==key_size(b->program_key)&&!memcmp(key,b->program_key,n);
}
static const char hex[]="0123456789abcdef";
static void byte_write(char*out,unsigned x){out[0]=hex[x>>4];out[1]=hex[x&15];}
static int byte_read(const char*s){
 int result=0;for(unsigned i=0;i<2;++i){unsigned c=(unsigned char)s[i];if(c>='0'&&c<='9')c-='0';else if(c>='a'&&c<='f')c=c-'a'+10;else return -1;result=result*16+(int)c;}return result;
}
enum am_synth_binding_result am_synth_binding_encode(const struct am_synth_binding*b,char*out,size_t capacity,size_t*size){
 if(!am_synth_binding_valid(b))return AM_SYNTH_BINDING_INVALID;
 size_t n=key_size(b->program_key),length=83+2*n;if(size)*size=length;
 if(!out)return AM_SYNTH_BINDING_OK;
 if(capacity<=length)return AM_SYNTH_BINDING_BUFFER;
 char data[AM_SYNTH_BINDING_PAYLOAD_BYTES];memcpy(data,"AMSY1 ",6);byte_write(data+6,(unsigned)n);size_t at=8;
 for(size_t i=0;i<n;++i,at+=2)byte_write(data+at,(unsigned char)b->program_key[i]);
 byte_write(data+at,b->carrier);at+=2;
 for(unsigned i=0;i<AM_SYNTH_PARAMS;++i,at+=2)byte_write(data+at,b->parameters[i]);
 data[at++]='\n';data[at]=0;memcpy(out,data,at+1);return AM_SYNTH_BINDING_OK;
}
enum am_synth_binding_result am_synth_binding_decode(const char*s,size_t size,struct am_synth_binding*out){
 if(!s||!out)return AM_SYNTH_BINDING_INVALID;
 if(size<6||memcmp(s,"AMSY1 ",6))return AM_SYNTH_BINDING_UNSUPPORTED;
 if(size<8)return AM_SYNTH_BINDING_INVALID;
 int n=byte_read(s+6);if(n<=0||n>=128||size!=(size_t)(83+2*n)||s[size-1]!='\n')return AM_SYNTH_BINDING_INVALID;
 struct am_synth_binding b={0};size_t at=8;
 for(int i=0;i<n;++i,at+=2){int c=byte_read(s+at);if(c<=0)return AM_SYNTH_BINDING_INVALID;b.program_key[i]=(char)c;}
 int pad=byte_read(s+at);if(pad<0)return AM_SYNTH_BINDING_INVALID;b.carrier=(unsigned char)pad;at+=2;
 for(unsigned i=0;i<AM_SYNTH_PARAMS;++i,at+=2){int v=byte_read(s+at);if(v<0)return AM_SYNTH_BINDING_INVALID;b.parameters[i]=(unsigned char)v;}
 if(!am_synth_binding_valid(&b))return AM_SYNTH_BINDING_INVALID;
 *out=b;return AM_SYNTH_BINDING_OK;
}
