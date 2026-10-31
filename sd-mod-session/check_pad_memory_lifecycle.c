#define MEMORY_PROOF_MAIN basic_memory_main
#include "check_pad_memory_sample.c"
extern void pad_source_result(void*,uintptr_t*,uintptr_t);
static struct pad_memory_job*prepare_memory(const struct pad_memory_services*s,const struct pad_output_reference*a){
 enum pad_render_status status;struct pad_memory_job*j=pad_memory_begin(base,s,a,"Rendered-Lifecycle",0,&status);assert(j&&status==PAD_RENDER_WORKING);return j;
}
static void complete_memory(struct pad_memory_job*j,uintptr_t result[2]){
 unsigned loops=0;while(pad_memory_step(j,7)==PAD_RENDER_WORKING)assert(++loops<10000);
 assert(pad_memory_take(j,result));pad_memory_destroy(j);
}
int main(int argc,char**argv){
 assert(!basic_memory_main(argc,argv));phase="renderer -> native RAM Sample lifecycle";
 struct pad_memory_services services={memory_queues,memory_storage,memory_gate};enum pad_render_status status;
 struct pad_render_source source={pcm,16,2,48000};struct pad_render_request request={.first=0,.end=16,.repeats=2,.tail_frames=3};
 struct fx_stage eq={.kind=FX_STAGE_EQ};fx_eq_default(&eq.eq);eq.eq.band[1].gain_db=6;assert(fx_chain_add(&request.chain,&eq,48000)==FX_OK);
 struct pad_render_job*render=pad_render_create(&source,&request,NULL,&status);assert(render);while(pad_render_step(render,7)==PAD_RENDER_WORKING){}
 struct pad_output_reference audio={.owner=pcm,.retain=memory_retain,.release=memory_release};audio.pcm=pad_render_output(render,&audio.frames,&audio.rate);assert(audio.frames==35);
 float expected[70];memcpy(expected,audio.pcm,sizeof(expected));source_refs=1;
 unsigned dropped=deletes,gates_before=gates;assert(!pad_memory_begin(base,&services,&audio,"budget",559,&status)&&status==PAD_RENDER_LIMIT&&source_refs==1&&gates==gates_before);
 deny_gate=1;assert(!pad_memory_begin(base,&services,&audio,"resource",0,&status)&&status==PAD_RENDER_RESOURCE&&source_refs==1);deny_gate=0;
 struct pad_memory_job*j=prepare_memory(&services,&audio);assert(pad_memory_step(j,7)==PAD_RENDER_WORKING);pad_memory_cancel(j);
 assert(pad_memory_step(j,7)==PAD_RENDER_CANCELLED&&source_refs==1);uintptr_t result[2]={};assert(!pad_memory_take(j,result));pad_memory_destroy(j);assert(deletes>dropped);
 j=prepare_memory(&services,&audio);pad_memory_destroy(j);assert(source_refs==1);
 float invalid[2]={NAN,0};struct pad_output_reference corrupt=audio;corrupt.pcm=invalid;corrupt.frames=1;j=prepare_memory(&services,&corrupt);
 assert(pad_memory_step(j,1)==PAD_RENDER_AUDIO_ERROR&&source_refs==1&&!pad_memory_take(j,result));pad_memory_destroy(j);
 j=prepare_memory(&services,&audio);complete_memory(j,result);uintptr_t second[2]={};j=prepare_memory(&services,&audio);complete_memory(j,second);assert(source_refs==1);
 phase="actual pool unique names and rollback";FN(0x292e310,void(*)(void*))(real_pool);
 assert(pad_sample_pool_register(base,real_pool,result));assert(pad_sample_pool_register(base,real_pool,second));
 assert(strcmp(FIELD((void*)result[0],0xa8,char*),FIELD((void*)second[0],0xa8,char*))!=0);
 assert(pad_sample_pool_contains(base,real_pool,result)&&pad_sample_pool_contains(base,real_pool,second));
 assert(pad_sample_pool_register(base,real_pool,second)); /* Idempotent, no duplicate ref or rename. */
 assert(pad_sample_pool_remove(base,real_pool,second)&&pad_sample_pool_remove(base,real_pool,second));
 assert(pad_sample_pool_contains(base,real_pool,result)&&!pad_sample_pool_contains(base,real_pool,second));
 read_memory_sample(result,expected,35);read_memory_sample(second,expected,35);
 assert(pad_sample_pool_remove(base,real_pool,result));FN(0x29348b0,void(*)(void*))(real_pool);
 memory_release(pcm);assert(!source_refs);pad_render_destroy(render); /* All producer PCM now gone. */
 read_memory_sample(result,expected,35);FN(0x123bca0,void(*)(void*))((void*)second[1]);
 /* Run the stock sampler's buffer acquisition helper, not just the export
  * reader. Its voice prefix takes Sample at f0 and retains the vector at598.
  * This covers buffer ownership, not complete device voice initialization. */
 unsigned char voice[0x600]={0};FIELD(voice,0xf0,uintptr_t)=result[0];
 FN(0x21fdd40,void(*)(void*))(voice);
 uintptr_t buffer[2]={FIELD(voice,0x598,uintptr_t),FIELD(voice,0x5a0,uintptr_t)};
 assert(buffer[0]&&buffer[1]);
 float*buffer_first=FIELD((void*)buffer[0],0,float*),*buffer_end=FIELD((void*)buffer[0],8,float*);
 assert(buffer_end-buffer_first==70&&!memcmp(buffer_first,expected,sizeof(expected)));
 /* A real reader retains metadata/vector after Sample and provider die.
  * Guard the complete native return object against another short-ABI error. */
 uintptr_t reader[2]={};pad_source_result((void*)result[0],reader,base+0x2943250);assert(reader[0]&&reader[1]);
 FN(0x123bca0,void(*)(void*))((void*)result[1]);
 assert(!memcmp(buffer_first,expected,sizeof(expected))); /* Sample/provider now released. */
 FN(0x123bca0,void(*)(void*))((void*)buffer[1]);
 struct {uint64_t before;struct pad_native_block block;uint64_t after;} guarded={.before=UINT64_C(0x123456789abc),.after=UINT64_C(0xabcdef123456)};
 check_read_frame((void*)reader[1],7,&guarded.block,base+0x20c5f60);
 assert(guarded.before==UINT64_C(0x123456789abc)&&guarded.after==UINT64_C(0xabcdef123456)&&guarded.block.frames==7);
 for(unsigned i=0;i<14;++i)assert(guarded.block.pcm[i]==expected[i]);
 pad_native_block_finish(&guarded.block);pad_native_block_finish(&guarded.block);
 float copied[14];assert(FN(0x20c5ff0,int64_t(*)(void*,int64_t,float*))((void*)reader[1],7,copied)==7);
 for(unsigned i=0;i<14;++i)assert(copied[i]==expected[14+i]);
 ((void(*)(void*))reader[0])((void*)reader[1]);
 float*large=calloc(5000*2,sizeof(float));assert(large);source_refs=1;
 struct pad_output_reference long_audio={pcm,large,5000,48000,memory_retain,memory_release};j=prepare_memory(&services,&long_audio);
 assert(pad_memory_step(j,5000)==PAD_RENDER_WORKING);uint64_t done,total;pad_memory_progress(j,&done,&total);assert(done==4096&&total==5000);
 pad_memory_cancel(j);assert(pad_memory_step(j,5000)==PAD_RENDER_CANCELLED&&source_refs==1);pad_memory_destroy(j);memory_release(pcm);free(large);
 puts("PASS real PCM/EQ render -> real RAM Sample -> actual pool registration/unique naming/rollback -> native read after producer destruction; gate/budget/cancel/partial teardown/nonfinite rejection,4096-frame copy bound; stock sampler buffer acquisition21fdd40 retains complete PCM after producer/Sample/provider destruction; retained reader,64-byte block canaries and owned completion cleanup; no full device voice/playback/Apply");
 return 0;
}
