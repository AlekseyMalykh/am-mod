/* Stand-in for libasound: verifies the production interposer against a
 * separate dynamic library, including actual converted PCM and short writes. */
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
struct pcm {unsigned channels;int stream,format;};
struct params {unsigned channels;};
static struct pcm handles[8];static unsigned used;
unsigned fake_channels,fake_frames,fake_device_channels=2;long fake_result=1000000;unsigned char fake_data[18*64*8];
int snd_pcm_open(struct pcm**out,const char*name,int stream,int mode){(void)mode;*out=&handles[used++%8];(*out)->channels=stream==0&&!strcmp(name,"hw:UAC2Gadget")?fake_device_channels:2;(*out)->stream=stream;return 0;}
int snd_pcm_close(struct pcm*p){(void)p;return 0;}
int snd_pcm_drop(struct pcm*p){(void)p;return 0;}
int snd_pcm_prepare(struct pcm*p){(void)p;return 0;}
int snd_pcm_hw_params_any(struct pcm*p,struct params*a){a->channels=p->channels;return 0;}
int snd_pcm_hw_params_set_channels(struct pcm*p,struct params*a,unsigned n){(void)p;a->channels=n;return 0;}
int snd_pcm_hw_params_set_channels_near(struct pcm*p,struct params*a,unsigned*n){(void)p;a->channels=*n;return 0;}
int snd_pcm_hw_params_get_channels(const struct params*a,unsigned*n){*n=a->channels;return 0;}
int snd_pcm_hw_params_get_channels_min(const struct params*a,unsigned*n){*n=a->channels;return 0;}
int snd_pcm_hw_params_get_channels_max(const struct params*a,unsigned*n){*n=a->channels;return 0;}
int snd_pcm_hw_params_set_format(struct pcm*p,struct params*a,int f){(void)a;p->format=f;return 0;}
int snd_pcm_hw_params(struct pcm*p,struct params*a){p->channels=a->channels;return 0;}
static unsigned size(int f){return f==2?2:f==32?3:f==16?8:4;}
long snd_pcm_writei(struct pcm*p,const void*data,unsigned long n){fake_channels=p->channels;fake_frames=(unsigned)n;memcpy(fake_data,data,n*p->channels*size(p->format));return fake_result<0?fake_result:(long)n<fake_result?(long)n:fake_result;}
long snd_pcm_writen(struct pcm*p,void**data,unsigned long n){fake_channels=p->channels;fake_frames=(unsigned)n;unsigned sz=size(p->format);for(unsigned f=0;f<n;++f)for(unsigned c=0;c<p->channels;++c)memcpy(fake_data+(f*p->channels+c)*sz,(char*)data[c]+f*sz,sz);return fake_result<0?fake_result:(long)n<fake_result?(long)n:fake_result;}
