/* Actual native consumer of rendered PCM, not SamplePool/assignment proof. */
#define CHOP_PCM_MAIN trim_proof_main
#include "check_chop_pcm.c"
#include "src/pad_output_stream.h"
struct owner {unsigned refs;struct pad_render_job*job;};
static unsigned dropped,denied;
static int retain_output(void*p){struct owner*o=p;if(denied)return 0;assert(o->refs&&o->job);++o->refs;return 1;}
static void release_output(void*p){struct owner*o=p;assert(o->refs);if(!--o->refs){pad_render_destroy(o->job);o->job=NULL;++dropped;}}
int main(int argc,char**argv){
 assert(!trim_proof_main(argc,argv));phase="native ProcessingStream consumes rendered PCM";
 struct pad_render_source source={pcm,16,2,48000};struct pad_render_request request={.first=0,.end=16,.repeats=2,.tail_frames=3};
 struct fx_stage eq={.kind=FX_STAGE_EQ};fx_eq_default(&eq.eq);eq.eq.band[1].gain_db=6;assert(fx_chain_add(&request.chain,&eq,48000)==FX_OK);
 enum pad_render_status error;struct owner owner={.refs=1,.job=pad_render_create(&source,&request,NULL,&error)};assert(owner.job);
 while(pad_render_step(owner.job,7)==PAD_RENDER_WORKING){}
 struct pad_output_reference audio={.owner=&owner,.retain=retain_output,.release=release_output};
 audio.pcm=pad_render_output(owner.job,&audio.frames,&audio.rate);assert(audio.pcm&&audio.frames==35);
 uintptr_t stream[2];assert(pad_output_stream_open(base,&audio,stream)==PAD_RENDER_WORKING&&owner.refs==2);
 void*reader=(void*)stream[1];void*metadata=FIELD(reader,8,void*);
 assert(FIELD(metadata,0,uint16_t)==2&&FIELD(metadata,4,int)==35&&FIELD(metadata,8,int)==48000);
 release_output(&owner);assert(owner.refs==1&&owner.job); /* UI owner gone; native stream pins audio. */
 for(unsigned i=0;i<35;++i){
  assert(!FN(0x20c58f0,int(*)(void*))(reader));struct pad_native_block frame={0};check_read_frame(reader,1,&frame,base+0x20c5f60);
  assert(frame.pcm==audio.pcm+i*2&&frame.frames==1);pad_native_block_finish(&frame); /* No duplicate PCM. */
 }
 assert(FN(0x20c58f0,int(*)(void*))(reader));struct pad_native_block empty={0};check_read_frame(reader,1,&empty,base+0x20c5f60);assert(!empty.frames&&!empty.pcm);pad_native_block_finish(&empty);
 assert(FN(0x20c5f30,int(*)(void*,int64_t))(reader,0)&&!FN(0x20c58f0,int(*)(void*))(reader));
 float copied[14];assert(FN(0x20c5ff0,int64_t(*)(void*,int64_t,float*))(reader,7,copied)==7);
 for(unsigned i=0;i<14;++i)assert(copied[i]==audio.pcm[i]);
 assert(!FN(0x20c5f30,int(*)(void*,int64_t))(reader,-1)&&!FN(0x20c5f30,int(*)(void*,int64_t))(reader,36));
 assert(FN(0x20c5f30,int(*)(void*,int64_t))(reader,35));
 unsigned char*trim=calloc(1,0x38);assert(trim);FN(0x20d9fd0,void(*)(void*,void*,int64_t,int64_t))(trim,stream,4,28);assert(!stream[1]);
 for(unsigned i=4;i<28;++i){struct pad_native_block frame={0};check_read_frame(trim,1,&frame,base+0x20c5f60);assert(frame.frames==1&&frame.pcm[0]==audio.pcm[i*2]&&frame.pcm[1]==audio.pcm[i*2+1]);pad_native_block_finish(&frame);}
 assert(FN(0x20c58f0,int(*)(void*))(trim));FN(0x20da200,void(*)(void*))(trim);assert(!owner.refs&&!owner.job&&dropped==1);
 owner.refs=1;owner.job=pad_render_create(&source,&request,NULL,&error);assert(owner.job);while(pad_render_step(owner.job,7)==PAD_RENDER_WORKING){}
 audio.pcm=pad_render_output(owner.job,&audio.frames,&audio.rate);denied=1;assert(pad_output_stream_open(base,&audio,stream)==PAD_RENDER_RESOURCE&&!stream[1]&&owner.refs==1);denied=0;
 audio.rate=48000.5;assert(pad_output_stream_open(base,&audio,stream)==PAD_RENDER_INVALID&&owner.refs==1);audio.rate=48000;
 audio.frames=UINT64_MAX;assert(pad_output_stream_open(base,&audio,stream)==PAD_RENDER_INVALID&&owner.refs==1);release_output(&owner);assert(dropped==2);
 puts("PASS rendered PCM/EQ -> owned native ProcessingStream interface -> requested-frame owned read block, real native20c5ff0 bulk-copy consumer/EOF/seek and TrimProcessingStream consumer; zero-copy block pointers, correct metadata and completion, retained output after caller release, one final destruction, refused retain and invalid metadata cleanup; native RAM Sample/Pool covered separately, mix/complete Apply/Save unconnected");
}
