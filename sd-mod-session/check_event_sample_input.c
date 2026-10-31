/* Native queue AND native1e27360/1e270d0 execute. L uses an engine adapter;
 * C additionally runs211b090/211aa30/2115070. Track routing policy, MIDI
 * service and final sampler remain adapters: no physical audio claim. */
#define BATCH_DISPATCH_MAIN batch_dispatch_proof_main
#include "check_batch_dispatch.c"
static unsigned char midi_buffer[64],midi_adapter[8];static uintptr_t adapter_vtable[3];
static unsigned char sample_track[0xc00];
static unsigned char event_queue_slots[16][0xc0];
static unsigned new_notes,releases,midi_calls,override_calls;static unsigned expected_midi;static struct step_event played_note;
static void process_choice(void*p,int key,void*event){assert(worker_active&&key>=0&&key<16&&batch_event_preview_override(p,event));++override_calls;}
static void receive_midi(void*p,void*msg,unsigned mask){
 assert(worker_active&&p==midi_adapter&&mask==0x7ffffffa);
 ++midi_calls;
 assert(FN(0xddffb0,int(*)(void*))(msg)==(int)expected_midi);
 FN(0xddf080,void(*)(void*,const void*))(midi_buffer,msg);
 FIELD(midi_buffer,24,unsigned)=mask;
 FIELD(preview_input_data,0x168,uint64_t)=2;FIELD(preview_input_data,0x170,void*)=midi_buffer;
 FIELD(preview_input_data,0x188,void*)=midi_buffer;FIELD(preview_input_data,0x190,void*)=midi_buffer+32;
}
static void receive_off(void*p,void*msg,int source,void*properties,unsigned mask){
 assert(worker_active&&p==preview_input_data&&(source==0||source==1)&&!properties&&mask==0x7ffffffa);
 assert(FN(0xddff20,int(*)(void*,int))(msg,1));
 assert(FN(0xddffb0,int(*)(void*))(msg)==(int)expected_midi);++releases;
 FIELD(preview_input_data,0x188,void*)=NULL;
}
static void receive_note(void*p,int64_t source,const struct step_event*event,unsigned mask){
 assert(worker_active&&p==preview_engine&&!source&&mask==0x7ffffffa&&musical(event));
 assert(event->data[0x20]==expected_midi);
 /* Execute the real engine normalizer, including its CurrentThings+140
  * and Program pad->MIDI map. This is why staged MIDI38 must enter as pad2. */
 played_note=FN(0x2115070,struct step_event(*)(void*,const struct step_event*,unsigned,void*))(p,event,mask,NULL);++new_notes;
}
/* Run the actual211b090 ->211aa30 ->2115070 chain for Chop. Only the
 * registered-track list, record/MIDI policy and final sampler are adapters.
 * The native receiver must not remap Slice15 or lose its source bank. */
static unsigned char native_routes[24],native_route[16],native_policy[0x210];
static unsigned native_delivery;static int expected_slice;
static void* route_list(void*p,unsigned mask){assert(p==native_routes&&mask==0x7ffffffa);return native_routes;}
static int accepts_route(void*p,void*r,int channel){assert(p==native_routes&&r&&channel==0);return 1;}
static int policy_disabled(void*p,int channel,int flag){(void)p;(void)channel;(void)flag;return 0;}
static void receiver_policy(void*p,unsigned mask,void*unused,const struct step_event*e){assert(p==preview_engine&&mask==0x7ffffffa&&!unused&&musical(e));}
static void velocity_property(void*p,int unused,float velocity){assert(p==sample_track+0x228&&!unused&&velocity>0);}
static void final_sampler(void*p,int64_t source,void*track,const struct step_event*e,int channel,unsigned mask){
 assert(worker_active&&p==preview_engine&&!source&&track==sample_track&&!channel&&mask==0x7ffffffa);
 assert(e->data[0x20]==FIELD(program,0x12a8+expected_midi*4,int)&&!(e->type&0x300));
 assert(event_choice_value(e,15)==expected_slice);played_note=*e;++native_delivery;
}
static void native_chop_receiver(const char*stock){
 restore_entry(stock,0x211b090);
 stub(0x282a920,route_list);stub(0x282a9c0,accepts_route);stub(0x21152d0,receiver_policy);
 stub(0x282fe30,policy_disabled);stub(0x282fec0,policy_disabled);stub(0x2768e10,velocity_property);stub(0x2119d00,final_sampler);
 FIELD(native_routes,0,void*)=native_route;FIELD(native_routes,8,void*)=native_route+16;
 FIELD(native_route,0,void*)=sample_track;FIELD(native_route,8,int)=0;
 FIELD(preview_engine,0x7a8,void*)=native_routes;FIELD(preview_engine,0x5d8,void*)=native_policy;
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
}
static void drain(void*q,uint64_t*reader){worker_active=1;for(unsigned n=0;n<128&&batch_event_preview_pending();++n)FN(0x2b59200,void(*)(void*,uint64_t*))(q,reader);worker_active=0;assert(!batch_event_preview_pending()&&!FIELD(context,0x50,unsigned));}
static void timed_out(int sig){(void)sig;fprintf(stderr,"TEST_TIMEOUT phase=%s refs=%u\n",phase,FIELD(context,0x50,unsigned));_exit(2);}
int main(int argc,char**argv){
 signal(SIGALRM,timed_out);alarm(30);
 assert(batch_dispatch_proof_main(argc,argv)==0);phase="ordinary native Sample16Levels event audition";
 alarm(30);
 fprintf(stderr,"EVENT_INPUT_TEST native setup\n");
 restore_entry(argv[1],0x1e27360);restore_entry(argv[1],0x1e270d0);
 stub(0x1e26dd0,receive_off);stub(0x211b090,receive_note);
 FILE*bridges=fopen("build/phone_bridges.bin","rb");assert(bridges);assert(fread((void*)(base+0x6d3d000),1,PHONE_TABLE_VA-0x6d3d000,bridges)>4096);fclose(bridges);
 FILE*patch=fopen("autorun/phone-sixteen-process.bin","rb");assert(patch);assert(fread((void*)(base+0x21472f4),1,4,patch)==4);fclose(patch);
 FIELD((void*)base,PHONE_TABLE_VA+64+H_SIXTEEN_PROCESS*16+8,uintptr_t)=(uintptr_t)process_choice;
 step_native_levels_processor(base+phone_hooks[H_SIXTEEN_PROCESS].trampoline);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 adapter_vtable[2]=(uintptr_t)receive_midi;FIELD(midi_adapter,0,void*)=adapter_vtable;
 FIELD(preview_input_data,0x198,void*)=midi_adapter;
 FN(0xddeb70,void(*)(void*))(midi_buffer);
 FIELD(context,0x78,void*)=things;FIELD(things,0xb8,void*)=program;FIELD(preview_record,0x9e8,int)=0;
 void*q=queues+0x1d8;uint64_t reader=0;expected_midi=2;
 FIELD(program,0x12a8+2*4,int)=38;FIELD(program,0x14a8+38*4,int)=2;
 FIELD(preview_engine,0x3c0,void*)=things;FIELD(things,0x140,void*)=sample_track;
 FIELD(sample_track,0xbb8,void*)=program;
 unsigned dispatch_before=dispatch_entered;
 /* The earlier serial fixture has only one writable slot. Real UI/audio
  * workers run concurrently; give this manually drained fixture16 slots
  * so Cancel/latest-only can enqueue more than one action before drain. */
 memset(q,0,0x48);FIELD(q,0,uint64_t)=16;FIELD(q,0x30,void*)=event_queue_slots;
 for(unsigned i=0;i<16;++i){FIELD(event_queue_slots[i],0,uintptr_t)=base+0xb57a7c;FIELD(event_queue_slots[i],8,uintptr_t)=base+0x15265b0;}
 struct step_event original=make_note(38,240,.8f);event_choice_set(&original,0,5);event_choice_set(&original,15,3);original.type|=0x200;
 for(unsigned mode=0;mode<10;++mode){
  fprintf(stderr,"EVENT_INPUT_TEST mode%u candidate\n",mode);
  struct step_event candidate;level_candidate(&original,&candidate,(int)mode,(int)mode,mode==1?-3:mode==8?1:0);
  unsigned before=new_notes,off=releases,midi_before=midi_calls,override_before=override_calls;
  assert(batch_event_preview(base,context,program,&candidate,mode,mode)==1&&batch_event_preview_pending()&&new_notes==before);
  fprintf(stderr,"EVENT_INPUT_TEST mode%u drain\n",mode);
  drain(q,&reader);assert(new_notes==before+1&&batch_event_preview_status()==BATCH_PREVIEW_PLAYING);
  assert(played_note.time==0&&!(played_note.type&0x300)&&!memcmp(played_note.data,candidate.data,37));
  assert(midi_calls==midi_before+(mode!=8)&&override_calls==override_before+(mode!=8));
  assert(batch_event_preview_stop(base,context,0));drain(q,&reader);assert(releases==off+2); /* Native initial retrigger-off + release. */
 }
 FIELD(program,0x12a8+19*4,int)=65;FIELD(program,0x14a8+65*4,int)=19;expected_midi=19;
 struct step_event bank_b=original;bank_b.data[0x20]=65;
 assert(batch_event_preview(base,context,program,&bank_b,0,1)==1);drain(q,&reader);assert(played_note.data[0x20]==65);
 assert(batch_event_preview_stop(base,context,0));drain(q,&reader);expected_midi=2;
 unsigned before=new_notes;
 assert(batch_event_preview(base,context,program,&original,2,1)==1);
 assert(batch_event_preview_stop(base,context,1));drain(q,&reader);assert(new_notes==before); /* Cancel before worker. */
 assert(batch_event_preview(base,context,program,&original,2,1)==1);
 assert(batch_event_preview(base,context,program,&original,3,1)==1);drain(q,&reader);assert(new_notes==before+1);
 assert(batch_event_preview_stop(base,context,0));drain(q,&reader);
 before=new_notes;assert(batch_event_preview(base,context,program,&original,2,1)==1);
 FIELD(program,0x14a8+38*4,int)=-1;drain(q,&reader);assert(new_notes==before&&batch_event_preview_status()==BATCH_PREVIEW_FAILED);
 FIELD(program,0x14a8+38*4,int)=2;
 FIELD(preview_record,0x9e8,int)=1;assert(batch_event_preview(base,context,program,&original,2,1)==2&&!batch_event_preview_pending());
 FIELD(preview_record,0x9e8,int)=0;assert(batch_event_preview(base,context,program,&original,2,1)==1);FIELD(preview_record,0x9e8,int)=1;
 drain(q,&reader);assert(new_notes==before&&batch_event_preview_status()==BATCH_PREVIEW_RECORDING);FIELD(preview_record,0x9e8,int)=0;
 FIELD(q,0x2d,unsigned char)=1;assert(!batch_event_preview(base,context,program,&original,2,1)&&!batch_event_preview_pending()&&!FIELD(context,0x50,unsigned));
 FIELD(q,0x2d,unsigned char)=0;
 assert(!batch_event_preview(base,context,(void*)1,&original,2,1));
 /* Every staged slice, with another-bank source, reaches the real native
  * receiver and normalizer without any live MIDI dependency. */
 native_chop_receiver(argv[1]);expected_midi=19;
 for(unsigned slice=1;slice<=16;++slice){
  struct step_event chop=bank_b;event_choice_set(&chop,15,(int)slice);expected_slice=(int)slice;
  unsigned delivered=native_delivery,midi_before=midi_calls,override_before=override_calls;
  assert(batch_event_preview(base,context,program,&chop,(slice-1)%16,8)==1);drain(q,&reader);
  assert(native_delivery==delivered+1&&midi_calls==midi_before&&override_calls==override_before);
  assert(!memcmp(played_note.data,chop.data,37));
  assert(batch_event_preview_stop(base,context,0));drain(q,&reader);
 }
 unsigned delivered=native_delivery;event_choice_set(&bank_b,15,1);
 assert(batch_event_preview(base,context,program,&bank_b,0,8));
 assert(batch_event_preview_stop(base,context,1));drain(q,&reader);assert(native_delivery==delivered);
 assert(batch_event_preview(base,context,program,&bank_b,0,8));event_choice_set(&bank_b,15,3);expected_slice=3;
 assert(batch_event_preview(base,context,program,&bank_b,2,8));drain(q,&reader);assert(native_delivery==delivered+1);
 assert(batch_event_preview_stop(base,context,0));drain(q,&reader);
 delivered=native_delivery;assert(batch_event_preview(base,context,program,&bank_b,0,8));FIELD(preview_record,0x9e8,int)=1;
 drain(q,&reader);assert(native_delivery==delivered&&batch_event_preview_status()==BATCH_PREVIEW_RECORDING);FIELD(preview_record,0x9e8,int)=0;
 FIELD(things,0xb8,void*)=NULL;assert(!batch_event_preview(base,context,program,&bank_b,0,8));FIELD(things,0xb8,void*)=program;
 unsigned char wrong_engine_things[0xc0]={0};FIELD(preview_engine,0x3c0,void*)=wrong_engine_things;
 assert(batch_event_preview(base,context,program,&bank_b,0,8));drain(q,&reader);assert(batch_event_preview_status()==BATCH_PREVIEW_FAILED);
 FIELD(preview_engine,0x3c0,void*)=things;
 unsigned char other_properties[0x230]={0};assert(!batch_event_preview_override(other_properties,&original));
 assert(original.type&0x200);FN(0xddf190,void(*)(void*))(midi_buffer);
 assert(dispatch_entered==dispatch_before);alarm(0);
 puts("PASS actual native Sample1e27360 MIDI construction/Levels hook and native1e270d0 release; Chop direct native211b090/211aa30/2115070 dispatch with16 exact slices and other-bank mapping; real worker queue, full staged C/L payload, all10 types, latest choice, cancel, no live-record input, stopped queue/refcounts; MIDI/track routing and final sampler are adapters");
}
