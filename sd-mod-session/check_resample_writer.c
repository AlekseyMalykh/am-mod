/* Real pinned accumulator/flush and master-tap registration. No device I/O.
 * Only the final file stream is replaced with two in-memory PCM sinks. */
#define main unused_fx_main
#include "check_fx_native.c"
#undef main
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
static unsigned char writer[0x108],main_audio[0x130],source_audio[0x130],node[56];
static uintptr_t sink_vtable[4],main_sink[1],side_sink[1],sinks[1];
static float main_l[128],main_r[128],source_l[128],source_r[128];
static float *main_channels[]={main_l,main_r},*source_channels[]={source_l,source_r};
static float main_peak,side_peak;
static unsigned main_frames,side_frames;
static int sink(void*self,const float*l,const float*r,unsigned n){
 assert(n==128);float*peak=self==main_sink?&main_peak:&side_peak;
 unsigned*frames=self==main_sink?&main_frames:&side_frames;assert(self==main_sink||self==side_sink);*frames+=n;
 for(unsigned k=0;k<n;++k){float p=fmaxf(fabsf(l[k]),fabsf(r[k]));if(p>*peak)*peak=p;}return 1;
}
static void setup(void){
 memset(writer,0,sizeof(writer));memset(main_audio,0,sizeof(main_audio));memset(node,0,sizeof(node));memset(main_l,0,sizeof(main_l));memset(main_r,0,sizeof(main_r));
 F(main_audio,0,int)=2;F(main_audio,4,int)=128;F(main_audio,16,void*)=main_channels;F(main_audio,0x128,unsigned char)=1;
 F(source_audio,0,int)=2;F(source_audio,4,int)=128;F(source_audio,16,void*)=source_channels;F(source_audio,0x128,unsigned char)=0;
 for(unsigned k=0;k<128;++k){source_l[k]=.25f;source_r[k]=-.125f;}
 sink_vtable[3]=(uintptr_t)sink;main_sink[0]=side_sink[0]=(uintptr_t)sink_vtable;sinks[0]=(uintptr_t)main_sink;
 F(writer,0x18,void*)=sinks;F(writer,0x20,void*)=sinks+1;F(writer,0x28,void*)=sinks+1;
 F(writer,0x30,void*)=main_audio;F(writer,0x38,void*)=main_audio+sizeof(main_audio);F(writer,0x40,void*)=main_audio+sizeof(main_audio);
 /* Native std::map<AudioBuffer*, FileWriter>: one existing master stem. */
 F(writer,0x58,void*)=node;F(writer,0x60,void*)=node;F(writer,0x68,void*)=node;F(writer,0x70,unsigned long)=1;
 F(node,8,void*)=writer+0x50;F(node,32,void*)=source_audio;F(node,40,void*)=side_sink;F(node,48,float)=1.f;
 F(writer,0x80,void*)=writer+0x80;F(writer,0x90,void*)=writer+0x80;
 F(writer,0xb0,void*)=writer+0xb0;F(writer,0xc0,void*)=writer+0xb0;
 main_peak=side_peak=0;main_frames=side_frames=0;
}
int main(int argc,char**argv){
 assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);sigaction(SIGILL,&sa,NULL);load(argv[1]);
 phase="r20d stem registration";setup();
 FN(0x18b94f0,void(*)(void*,void*,void*,float))(writer,source_audio,NULL,1.f);
 assert(FN(0x18b3b10,int(*)(void*,unsigned))(writer,128));
 printf("r20d native master registration: primary=%.6g stem=%.6g frames=%u/%u\n",(double)main_peak,(double)side_peak,main_frames,side_frames);
 assert(main_frames==128&&side_frames==128&&main_peak==0&&side_peak==.25f);
 phase="primary output accumulation";setup();float gain[]={1,1,0};
 FN(0x18b8dd0,void(*)(void*,void*,void*,unsigned))(writer,source_audio,gain,128);
 assert(FN(0x18b3b10,int(*)(void*,unsigned))(writer,128));
 printf("native primary accumulation: primary=%.6g stem=%.6g frames=%u/%u\n",(double)main_peak,(double)side_peak,main_frames,side_frames);
 assert(main_frames==128&&main_peak==.25f&&source_l[0]==.25f&&source_r[0]==-.125f);

 phase="real native WAV encoder";setup();
 const char*path="/tmp/mpc-writer-native-proof.wav";unlink(path);void*file=NULL;
 FN(0xf535c0,void(*)(void*,const char*))(&file,path);
 F(writer,0,unsigned)=0;F(writer,4,unsigned)=3;F(writer,8,unsigned)=3;F(writer,12,unsigned)=44100;F(writer,16,unsigned)=2;
 void*stream=FN(0x18b8a10,void*(*)(void*,void*))(writer,&file);assert(stream);sinks[0]=(uintptr_t)stream;
 FN(0x18b8dd0,void(*)(void*,void*,void*,unsigned))(writer,source_audio,gain,128);
 assert(FN(0x18b3b10,int(*)(void*,unsigned))(writer,128));
 uintptr_t*v=F(stream,0,uintptr_t*);((void(*)(void*))v[5])(stream);((void(*)(void*))v[1])(stream);
 FILE*f=fopen(path,"rb");assert(f);unsigned char b[12];assert(fread(b,1,12,f)==12&&!memcmp(b,"RIFF",4)&&!memcmp(b+8,"WAVE",4));unsigned nonzero=0,data_bytes=0;
 for(unsigned k=0;k<16;++k){assert(fread(b,1,8,f)==8);unsigned n=(unsigned)b[4]|(unsigned)b[5]<<8|(unsigned)b[6]<<16|(unsigned)b[7]<<24;
  if(!memcmp(b,"data",4)){data_bytes=n;for(unsigned j=0;j<n;++j){int ch=fgetc(f);assert(ch!=EOF);nonzero+=ch!=0;}break;}assert(!fseek(f,(long)n+(n&1u),SEEK_CUR));}
 fclose(f);unlink(path);assert(data_bytes==128*2*3&&nonzero);printf("NATIVE_WAV frames=128 bits=24 bytes=%u nonzero_bytes=%u\n",data_bytes,nonzero);
 puts("PASS real native accumulator/flush and WAV encoder produce nonzero destination audio; master-stem-only path leaves primary silent. DSP is not executed.");return 0;
}
