/* Real ExtractSliceCommand -> SampleEditFunctions -> TrimProcessingStream.
 * Sample allocation/storage and model notifications are explicit adapters. */
#define CHOP_PCM_MAIN crop_proof_main
#include "check_chop_pcm.c"
#undef CHOP_PCM_MAIN
static unsigned char source_sample[0x200],output_sample[0x200],edit_functions[0x90],queue_owner[0x200];
static unsigned char settings[0x420];static uintptr_t queue_vtable[5],sample_ref[4];
static unsigned creation_calls,pool_adds,pool_removes,metadata_calls;
static int creation_fail;static unsigned expected_start=4,expected_end=12;
void pipeline_source(void*sample,uintptr_t*out){
 assert(sample==source_sample);unsigned char*reader=calloc(1,0x28);assert(reader);uintptr_t none[2]={0};
 FN(0x20c74f0,void(*)(void*,const uintptr_t*))(reader,none);FIELD(reader,0,void*)=reader_vtable;
 void*d=FIELD(reader,8,void*);FN(0x20c56d0,void(*)(void*,int))(d,16);FN(0x20c56b0,void(*)(void*,int))(d,2);FN(0x20c56f0,void(*)(void*,int))(d,48000);
 out[0]=base+0x20c58d0;out[1]=(uintptr_t)reader;
}
void pipeline_create(void*functions,const uintptr_t*sample,const uintptr_t*generator,int add,int fresh,uintptr_t*out){
 assert(functions==edit_functions&&sample[0]==(uintptr_t)source_sample&&add==1&&fresh==1);
 void*trim=(void*)generator[1];assert(FIELD(trim,0,uintptr_t)==base+0x6e7de10);
 void*d=FIELD(trim,8,void*);assert(FIELD(d,4,unsigned)==expected_end-expected_start&&FIELD(d,0,uint16_t)==2&&FIELD(d,8,unsigned)==48000);
 for(unsigned i=expected_start;i<expected_end;++i){struct pad_native_block frame={0};check_read_frame(trim,1,&frame,base+0x20c5f60);
  assert(frame.frames==1&&frame.pcm[0]==pcm[2*i]&&frame.pcm[1]==pcm[2*i+1]);pad_native_block_finish(&frame);}
 assert(FN(0x20c58f0,int(*)(void*))(trim));++creation_calls;
 out[0]=creation_fail?0:(uintptr_t)output_sample;out[1]=creation_fail?0:(uintptr_t)sample_ref;
 if(!creation_fail)++FIELD(sample_ref,8,unsigned);
}
static uintptr_t no_thread(unsigned key){(void)key;return 0;}
static int execute_inline(void*q,void*action,uintptr_t tag){assert(q==queue_owner+0x30&&tag==(uintptr_t)edit_functions);
 ((void(*)(void*))FIELD(action,0,uintptr_t))((char*)action+16);return 1;}
static void drain(void*q,int wait){assert(q==queue_owner+0x30&&!wait);}
static void set_sample_name(void*loader,const uintptr_t*sample,const uint64_t*name){
 assert(loader==(void*)0x1111&&sample[0]==(uintptr_t)output_sample&&!strcmp(*(const char**)name,"cropped-probe"));}
static void copy_settings(void*sample,const void*data){assert(sample==output_sample&&data);++metadata_calls;}
static void*sample_settings(void*sample){assert(sample==output_sample);return settings;}
static void apply_slice(void*data,void*slice){assert(data==settings&&FIELD(slice,0x78,int64_t)==0&&FIELD(slice,0xd0,int64_t)==(int64_t)(expected_end-expected_start));}
static void bit_depth(void*sample,int depth){assert(sample==output_sample&&depth==24);}
static void dirty(void*sample,uintptr_t a,int b){(void)a;assert(sample==output_sample&&!(b&255));}
void pool_insert(void*pool,const uintptr_t*sample,uintptr_t*out){assert(pool==(void*)0x2222&&sample[0]==(uintptr_t)output_sample);++pool_adds;out[0]=out[1]=0;}
static void pool_remove(void*pool,const uintptr_t*sample){assert(pool==(void*)0x2222&&sample[0]==(uintptr_t)output_sample);++pool_removes;}
extern void check_pipeline_source(void),check_pipeline_create(void),check_pool_insert(void);
int main(int argc,char**argv){assert(crop_proof_main(argc,argv)==0);phase="native successful extract crop pipeline";
 stub(0x2943250,check_pipeline_source);stub(0x209b1f0,check_pipeline_create);stub(0xb53508,no_thread);
 stub(0x1df69f0,set_sample_name);stub(0x2942010,copy_settings);stub(0x2941ff0,sample_settings);
 stub(0x294d670,apply_slice);stub(0x2942020,bit_depth);stub(0x29457e0,dirty);
 stub(0x2931440,check_pool_insert);stub(0x2930010,pool_remove);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 queue_vtable[2]=(uintptr_t)execute_inline;queue_vtable[4]=(uintptr_t)drain;FIELD(queue_owner,0x30,void*)=queue_vtable;
 FIELD(edit_functions,0,void*)=queue_owner;FIELD(edit_functions,0x78,void*)=(void*)0x1111;FIELD(edit_functions,0x80,void*)=(void*)0x2222;
 FIELD(sample_ref,8,unsigned)=1000;
 for(unsigned trial=0;trial<2;++trial){unsigned char inner[0xc8]={0};creation_fail=(int)trial;
  FIELD(inner,0x50,void*)=source_sample;FIELD(inner,0x60,int64_t)=expected_start;FIELD(inner,0x68,int64_t)=expected_end;
  FIELD(inner,0xa0,void*)=(void*)0x2222;FIELD(inner,0xa8,int)=2;FIELD(inner,0xac,int)=24;FIELD(inner,0xb0,void*)=edit_functions;
  FN(0xf535c0,void(*)(void*,const char*))(inner+0x98,"cropped-probe");
  unsigned old_released=released;int result=FN(0x19aaae0,int(*)(void*))(inner);
  assert(result==(trial?3:0)&&released==old_released+1&&!FIELD(edit_functions,8,unsigned));
  assert(creation_calls==trial+1&&pool_adds==1&&metadata_calls==1);
  if(!trial){assert(FIELD(inner,0xb8,void*)==output_sample);assert(!FN(0x19a9820,int(*)(void*))(inner)&&pool_removes==1);
   FN(0x123bca0,void(*)(void*))((void*)FIELD(inner,0xc0,uintptr_t));}
  else assert(!FIELD(inner,0xb8,void*));
  FN(0xf0a290,void(*)(void*))(inner+0x98);
 }
 puts("PASS real ExtractSliceCommand19aaae0 -> SampleEditFunctions209c5b0 -> TrimProcessingStream stereo PCM; successful crop result, native post-processing queue closure and undo removal, empty creation failure; Sample allocation/pool/model receivers are adapters, device persistence pending");
}
