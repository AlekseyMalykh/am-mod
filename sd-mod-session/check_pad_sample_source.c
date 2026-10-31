/* Native Sample2943250 getter, metadata methods, stream read/EOF/deleter
 * execute. Sample provider/PCM decoder are deterministic test adapters. */
#define CHOP_PCM_MAIN trim_stream_proof_main
#include "check_chop_pcm.c"
#include "src/pad_sample_source.h"
static unsigned char native_sample[0x200],sample_metadata[0x400],provider[8];
static unsigned char selected_instrument[0x2320],selected_layer[0x1660];
static uintptr_t provider_vtable[3],provider_reference[4],sample_reference[4],owned_reader_vtable[8];
static unsigned channels=2,sample_rate=48000,frames_returned=16,short_frame,fail_factory,fail_seek,frame_calls,stream_deletes;
static unsigned block_lease,block_completed,block_destroyed;
static void source_block_complete(void*p){(void)p;assert(block_lease);block_lease=0;++block_completed;}
static int source_block_manage(unsigned op,const void*from,void*to){
 if(op==2)return 1;
 if(op==1){++block_destroyed;return 0;}
 assert(op==0&&from&&to);memcpy(to,from,sizeof(uintptr_t)*4);return 0;
}
extern void check_pad_source_factory(void),check_pad_source_frame(void);
static int source_seek(void*p,int64_t frame){return fail_seek?0:reader_seek(p,frame);}
static void source_delete(void*p){reader_delete(p);++stream_deletes;}
void native_reader_frame(void*p,int64_t requested,struct pad_native_block*out){
 int64_t n=FIELD(p,0x20,int64_t);assert(n>=0&&n<16);++frame_calls;
 assert(requested>0&&!block_lease);block_lease=1;memset(out,0,sizeof(*out));out->pcm=pcm+n*2;out->frames=short_frame?0:1;out->invoke=(uintptr_t)source_block_complete;out->manage=(uintptr_t)source_block_manage;FIELD(p,0x20,int64_t)=n+1;
}
void native_reader_factory(void*p,uintptr_t*out){
 assert(p==provider);
 if(fail_factory){out[0]=out[1]=0;return;}
 unsigned char*r=calloc(1,0x28);assert(r);uintptr_t none[2]={0};
 FN(0x20c74f0,void(*)(void*,const uintptr_t*))(r,none);FIELD(r,0,void*)=owned_reader_vtable;
 void*d=FIELD(r,8,void*);FN(0x20c56d0,void(*)(void*,int))(d,(int)frames_returned);
 FN(0x20c56b0,void(*)(void*,int))(d,(int)channels);FN(0x20c56f0,void(*)(void*,int))(d,(int)sample_rate);
 out[0]=base+0x20c58d0;out[1]=(uintptr_t)r;
}
static void finish_source(struct pad_render_job*j,size_t budget){
 enum pad_render_status state;unsigned loops=0;
 do{state=pad_render_step(j,budget);assert(++loops<1000);}while(state==PAD_RENDER_WORKING);
 assert(state==PAD_RENDER_DONE);
}
int main(int argc,char**argv){
 assert(!trim_stream_proof_main(argc,argv));phase="native Sample -> owned bounded renderer capture";
 for(unsigned i=0;i<8;++i)owned_reader_vtable[i]=reader_vtable[i];
 owned_reader_vtable[1]=(uintptr_t)source_delete;owned_reader_vtable[5]=(uintptr_t)check_pad_source_frame;owned_reader_vtable[6]=(uintptr_t)source_seek;
 provider_vtable[2]=(uintptr_t)check_pad_source_factory;FIELD(provider,0,void*)=provider_vtable;
 FIELD(provider_reference,8,unsigned)=1000;FIELD(sample_reference,8,unsigned)=1000;
 FIELD(native_sample,0x170,void*)=provider;FIELD(native_sample,0x178,void*)=provider_reference;FIELD(native_sample,0x130,void*)=sample_metadata;
 FN(0x20c56d0,void(*)(void*,int))(sample_metadata,16);FN(0x20c56b0,void(*)(void*,int))(sample_metadata,2);FN(0x20c56f0,void(*)(void*,int))(sample_metadata,48000);
 FN(0x20c5740,void(*)(void*,float))(sample_metadata,91.f); /* BPM is not sample rate. */
 uintptr_t shared[2]={(uintptr_t)native_sample,(uintptr_t)sample_reference};
 for(unsigned mono=0;mono<2;++mono){
  channels=mono?1:2;FN(0x20c56b0,void(*)(void*,int))(sample_metadata,(int)channels);
  sample_rate=mono?44100:48000;FN(0x20c56f0,void(*)(void*,int))(sample_metadata,(int)sample_rate);
  struct pad_render_stream stream;unsigned old_reads=frame_calls,old_deletes=stream_deletes;
  enum pad_render_status opened=pad_sample_stream_open(base,shared,&stream);
  assert(opened==PAD_RENDER_WORKING);
  assert(stream.frames==16&&stream.rate==sample_rate&&FIELD(sample_reference,8,unsigned)==1001);
  struct pad_render_request r={.first=4,.end=12,.repeats=2,.tail_frames=3,.pcm_budget=216};
  enum pad_render_status error;struct pad_render_job*j=pad_render_create_stream(&stream,&r,NULL,&error);
  assert(j&&error==PAD_RENDER_WORKING&&frame_calls==old_reads);
  struct pad_render_progress progress;pad_render_progress(j,&progress);assert(!progress.source_done&&progress.source_total==8);
  assert(pad_render_step(j,3)==PAD_RENDER_WORKING);pad_render_progress(j,&progress);
  assert(progress.source_done==3&&!progress.done&&frame_calls==old_reads+3);
  assert(!pad_render_output(j,NULL,NULL));finish_source(j,2);
  const float*out=pad_render_output(j,NULL,NULL);assert(out);
  for(unsigned f=0;f<19;++f)for(unsigned c=0;c<2;++c)assert(out[f*2+c]==(f<16?pcm[(4+f%8)*2+(mono?0:c)]:0.f));
  assert(stream_deletes==old_deletes+1&&FIELD(sample_reference,8,unsigned)==1000&&FIELD(provider_reference,8,unsigned)==1000);
  pad_render_destroy(j);assert(stream_deletes==old_deletes+1);
 }
 channels=2;FN(0x20c56b0,void(*)(void*,int))(sample_metadata,2);sample_rate=48000;FN(0x20c56f0,void(*)(void*,int))(sample_metadata,48000);
 struct pad_render_request r={.first=4,.end=12,.repeats=1};enum pad_render_status error;struct pad_render_stream stream;
 assert(pad_sample_stream_open(base,shared,&stream)==PAD_RENDER_WORKING);
 struct pad_render_job*j=pad_render_create_stream(&stream,&r,NULL,&error);assert(j);pad_render_cancel(j);
 unsigned old_reads=frame_calls,old_deletes=stream_deletes;
 assert(pad_render_step(j,256)==PAD_RENDER_CANCELLED&&frame_calls==old_reads&&stream_deletes==old_deletes+1);pad_render_destroy(j);
 for(unsigned failure=0;failure<2;++failure){
  assert(pad_sample_stream_open(base,shared,&stream)==PAD_RENDER_WORKING);j=pad_render_create_stream(&stream,&r,NULL,&error);assert(j);
  if(failure)fail_seek=1;else short_frame=1;
  assert(pad_render_step(j,256)==PAD_RENDER_AUDIO_ERROR&&!pad_render_output(j,NULL,NULL));pad_render_destroy(j);short_frame=fail_seek=0;
  assert(FIELD(sample_reference,8,unsigned)==1000);
 }
 frames_returned=15;old_deletes=stream_deletes;
 assert(pad_sample_stream_open(base,shared,&stream)==PAD_RENDER_INVALID&&!stream.read&&stream_deletes==old_deletes+1);frames_returned=16;
 fail_factory=1;assert(pad_sample_stream_open(base,shared,&stream)==PAD_RENDER_RESOURCE&&!stream.user);fail_factory=0;
 assert(FIELD(sample_reference,8,unsigned)==1000&&FIELD(provider_reference,8,unsigned)==1000);
 FN(0x20c56b0,void(*)(void*,int))(sample_metadata,8);assert(pad_sample_stream_open(base,shared,&stream)==PAD_RENDER_INVALID);FN(0x20c56b0,void(*)(void*,int))(sample_metadata,2);
 assert(pad_sample_stream_open(base,shared,&stream)==PAD_RENDER_WORKING);r.pcm_budget=1;
 assert(!pad_render_create_stream(&stream,&r,NULL,&error)&&error==PAD_RENDER_LIMIT);
 assert(FIELD(sample_reference,8,unsigned)==1001);stream.close(stream.user);assert(FIELD(sample_reference,8,unsigned)==1000);
 FIELD(program,0x2808,void*)=instruments;FIELD(program,0x2810,void*)=instruments+128;instruments[65]=selected_instrument;
 FIELD(selected_instrument,0x22e8,void*)=selected_layer;FIELD(selected_instrument,0x22f0,void*)=selected_layer+sizeof(selected_layer);FIELD(selected_instrument,0x2310,void*)=program;
 FIELD(selected_layer,0x4a8,void*)=native_sample;FIELD(selected_layer,0x4b0,void*)=sample_reference;
 FIELD(selected_layer,0x740+0x78,int64_t)=4;FIELD(selected_layer,0x740+0xd0,int64_t)=12;
 struct pad_sample_selection selection;assert(pad_sample_stream_open_pad(base,program,65,&selection)==PAD_RENDER_WORKING);
 assert(selection.pad==65&&selection.first==4&&selection.end==12&&selection.stream.rate==48000&&FIELD(sample_reference,8,unsigned)==1001);
 /* Selection and stream retain Sample/Trim, never borrowed layer fields. */
 FIELD(selected_layer,0x740+0x78,int64_t)=0;FIELD(selected_layer,0x4a8,void*)=NULL;
 r=(struct pad_render_request){.first=selection.first,.end=selection.end,.repeats=1};
 j=pad_render_create_stream(&selection.stream,&r,NULL,&error);assert(j);finish_source(j,3);
 const float*out=pad_render_output(j,NULL,NULL);for(unsigned f=0;f<8;++f)assert(out[f*2]==pcm[(4+f)*2]);
 pad_render_destroy(j);assert(FIELD(sample_reference,8,unsigned)==1000&&FIELD(sample_metadata,0x3d0,float)==91.f);
 FIELD(selected_layer,0x4a8,void*)=native_sample;FIELD(selected_layer,0x740+0x78,int64_t)=4;
 assert(pad_sample_stream_open_pad(base,program,65,&selection)==PAD_RENDER_WORKING);
 struct fx_stage eq={.kind=FX_STAGE_EQ};fx_eq_default(&eq.eq);eq.eq.band[1].gain_db=9;
 assert(fx_chain_add(&r.chain,&eq,48000)==FX_OK);
 j=pad_render_create_stream(&selection.stream,&r,NULL,&error);assert(j);finish_source(j,1);
 struct pad_render_source reference_source={pcm,16,2,48000};
 struct pad_render_job*k=pad_render_create(&reference_source,&r,NULL,&error);assert(k);finish_source(k,3);
 assert(!memcmp(pad_render_output(j,NULL,NULL),pad_render_output(k,NULL,NULL),8*2*sizeof(float)));
 pad_render_destroy(j);pad_render_destroy(k);
 assert(FIELD(sample_reference,8,unsigned)==1000);FIELD(selected_layer,0x4a8,void*)=NULL;
 assert(pad_sample_stream_open_pad(base,program,128,&selection)==PAD_RENDER_INVALID);
 assert(pad_sample_stream_open_pad(base,program,64,&selection)==PAD_RENDER_INVALID);
 assert(pad_sample_stream_open_pad(base,program,65,&selection)==PAD_RENDER_INVALID);
 FIELD(selected_layer,0x4a8,void*)=native_sample;FIELD(selected_layer,0x740+0xd0,int64_t)=17;
 assert(pad_sample_stream_open_pad(base,program,65,&selection)==PAD_RENDER_INVALID&&!selection.stream.read&&FIELD(sample_reference,8,unsigned)==1000);
 FIELD(selected_instrument,0x2310,void*)=NULL;
 assert(pad_sample_stream_open_pad(base,program,65,&selection)==PAD_RENDER_INVALID);
 assert(!block_lease&&block_completed==frame_calls&&block_destroyed==frame_calls);
 puts("PASS real Program286bbe0 and layer Sample2803b90 -> Sample2943250 provider snapshot, metadata/owned read-block/EOF/stream deletion and reference release -> bounded renderer; complete+destroy each native block exactly once before reuse, including invalid short block; stereo/mono Trim/repeats/tail, other-bank selection independent of layer edits, BPM91 distinct from PCM48000, progress/cancel/error cleanup; source decoder/provider are adapters, no live pad DSP/assignment/Save integration");
}
