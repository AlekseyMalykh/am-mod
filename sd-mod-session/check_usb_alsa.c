#include "src/usb_audio.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
struct pcm;struct params{unsigned channels;};
extern int snd_pcm_open(struct pcm**,const char*,int,int),snd_pcm_close(struct pcm*),snd_pcm_drop(struct pcm*);
extern int snd_pcm_hw_params_any(struct pcm*,struct params*),snd_pcm_hw_params_set_channels(struct pcm*,struct params*,unsigned);
extern int snd_pcm_hw_params_get_channels(const struct params*,unsigned*),snd_pcm_hw_params_set_format(struct pcm*,struct params*,int),snd_pcm_hw_params(struct pcm*,struct params*);
extern int snd_pcm_hw_params_get_channels_max(const struct params*,unsigned*),snd_pcm_hw_params_set_channels_near(struct pcm*,struct params*,unsigned*);
extern long snd_pcm_writei(struct pcm*,const void*,unsigned long),snd_pcm_writen(struct pcm*,void**,unsigned long);
extern unsigned fake_channels,fake_frames,fake_device_channels;extern long fake_result;extern unsigned char fake_data[];
static float source[18][16],peek[18*16];static const float*tap[18];
/* Actual pinned native path3f3ee10: any -> format -> channels_max ->
 * channels_near -> apply; after both directions open it reads channels. */
static void setup(struct pcm*p,struct params*a,int f){unsigned channels=0;assert(!snd_pcm_hw_params_any(p,a));assert(!snd_pcm_hw_params_set_format(p,a,f));assert(!snd_pcm_hw_params_get_channels_max(a,&channels));assert(channels==2);assert(!snd_pcm_hw_params_set_channels_near(p,a,&channels));assert(channels==2);assert(!snd_pcm_hw_params(p,a));assert(!snd_pcm_hw_params_get_channels(a,&channels));assert(channels==2);}
static void fresh(void){usb_audio_capture(tap,16);usb_audio_peek(peek,16);usb_audio_capture(tap,16);}
int main(int argc,char**argv){assert(argc==2);unlink(argv[1]);
 for(unsigned c=0;c<18;++c){tap[c]=source[c];for(unsigned f=0;f<16;++f)source[c][f]=(float)(c+1)/32.f+f/1024.f;}
 struct pcm*p;struct params a;int32_t stock[32]={0};
 assert(!snd_pcm_open(&p,"hw:UAC2Gadget",0,0));setup(p,&a,10);assert(a.channels==2);assert(snd_pcm_writei(p,stock,16)==16);assert(fake_channels==2);snd_pcm_close(p);
 FILE*flag=fopen(argv[1],"w");assert(flag);fclose(flag);
 assert(!snd_pcm_open(&p,"hw:UAC2Gadget",1,0));setup(p,&a,10);assert(a.channels==2);snd_pcm_close(p);
 assert(!snd_pcm_open(&p,"hw:Other",0,0));setup(p,&a,10);assert(a.channels==2);snd_pcm_close(p);
 fake_device_channels=18;
 assert(!snd_pcm_open(&p,"hw:UAC2Gadget",0,0));setup(p,&a,10);assert(a.channels==18);fresh();
 fake_result=-11;assert(snd_pcm_writei(p,stock,16)==-11);assert(usb_audio_peek(peek,16)==16);
 fake_result=7;assert(snd_pcm_writei(p,stock,16)==7);assert(fake_channels==18);assert(usb_audio_peek(peek,16)==9);
 for(unsigned f=0;f<16;++f)for(unsigned c=0;c<18;++c){int32_t v;memcpy(&v,fake_data+(f*18+c)*4,4);assert(v==(int32_t)(source[c][f]*2147483648.));}
 fake_result=1000000;assert(snd_pcm_writei(p,stock,9)==9);assert(!usb_audio_peek(peek,16));
 assert(!snd_pcm_drop(p));assert(snd_pcm_writei(p,stock,16)==16);for(unsigned i=0;i<16*18*4;++i)assert(fake_data[i]==0);
 setup(p,&a,14);fresh();void*planes[2]={source[0],source[1]};assert(snd_pcm_writen(p,planes,16)==16);
 for(unsigned f=0;f<16;++f)for(unsigned c=0;c<18;++c){float v;memcpy(&v,fake_data+(f*18+c)*4,4);assert(v==source[c][f]);}
 snd_pcm_close(p);unlink(argv[1]);puts("PASS actual ALSA interposition: stereo bypass, capture bypass, 18 real / 2 native channels, S32/float, planar/interleaved, EAGAIN, short writes and reset");
}
