/* Real native worker queue; sampler/recorder handoff captured explicitly. */
#define BATCH_DISPATCH_MAIN piano_queue_setup_main
#include "check_batch_dispatch.c"
static unsigned char piano_track[0xc00],piano_slots[16][0xc0];
static unsigned piano_on,piano_off,piano_last;static struct step_event piano_notes[16];
static unsigned piano_choices[16];
void poly_record_release_event(void *engine,void *track,unsigned source,unsigned choice,int last){
 assert(worker_active&&engine==preview_engine&&track==piano_track&&source==2&&piano_off<16);
 piano_choices[piano_off++]=choice;piano_last+=(unsigned)last;
}
static void piano_receive(void*engine,int64_t source,const struct step_event*note,unsigned mask){
 assert(worker_active&&engine==preview_engine&&!source&&mask==0x7ffffffa&&note->data[32]==2&&piano_on<16);
 piano_notes[piano_on++]=*note;
}
static void piano_drain(void*q,uint64_t*reader){worker_active=1;for(unsigned i=0;i<100&&FIELD(context,0x50,unsigned);++i)FN(0x2b59200,void(*)(void*,uint64_t*))(q,reader);worker_active=0;assert(!FIELD(context,0x50,unsigned));}
int main(int argc,char**argv){
 assert(!piano_queue_setup_main(argc,argv));phase="Piano Roll live native input";
 stub(0x211b090,piano_receive);__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 FIELD(context,0x78,void*)=things;FIELD(things,0xb8,void*)=program;
 FIELD(preview_engine,0x3c0,void*)=things;FIELD(things,0x140,void*)=piano_track;
 FIELD(program,0x12a8+2*4,int)=38;FIELD(program,0x14a8+38*4,int)=2;
 void*q=queues+0x1d8;uint64_t reader=0;memset(q,0,0x48);FIELD(q,0,uint64_t)=16;FIELD(q,0x30,void*)=piano_slots;
 for(unsigned i=0;i<16;++i){FIELD(piano_slots[i],0,uintptr_t)=base+0xb57a7c;FIELD(piano_slots[i],8,uintptr_t)=base+0x15265b0;}
 FIELD(preview_record,0x9e8,int)=1; /* Armed/recording must accept musical input. */
 struct step_event low=make_note(38,0,.3f),high=make_note(38,0,.9f);event_choice_set(&low,0,-12);event_choice_set(&high,0,7);
 assert(batch_piano_input(base,context,program,&low,0)&&batch_piano_input(base,context,program,&high,7));
 assert(!piano_on);piano_drain(q,&reader);assert(piano_on==2&&event_choice_value(&piano_notes[0],0)==-12&&event_choice_value(&piano_notes[1],0)==7);
 assert(FIELD(&piano_notes[0],0x10,float)==.3f&&FIELD(&piano_notes[1],0x10,float)==.9f);
 assert(batch_piano_input(base,context,NULL,NULL,7));piano_drain(q,&reader);assert(piano_off==1&&!piano_last&&piano_choices[0]==(0x1000u|high.data[6]));
 assert(batch_piano_input(base,context,NULL,NULL,0));piano_drain(q,&reader);assert(piano_off==2&&piano_last==1&&piano_choices[1]==(0x1000u|low.data[6]));
 assert(batch_piano_input(base,context,NULL,NULL,0));piano_drain(q,&reader);assert(piano_off==2);
 struct step_event a=make_note(38,0,.7f),b=a;event_choice_set(&a,15,1);event_choice_set(&b,15,5);
 assert(batch_piano_input(base,context,program,&a,1)&&batch_piano_input(base,context,program,&b,5));piano_drain(q,&reader);
 assert(event_choice_value(&piano_notes[2],15)==1&&event_choice_value(&piano_notes[3],15)==5);
 assert(batch_piano_input(base,context,NULL,NULL,5)&&batch_piano_input(base,context,NULL,NULL,1));piano_drain(q,&reader);
 assert(piano_choices[2]==(0x1000u|(15u<<8)|b.data[21])&&piano_choices[3]==(0x1000u|(15u<<8)|a.data[21])&&piano_last==2);
 assert(batch_piano_input(base,context,program,&low,0)&&batch_piano_input_stop(base,context));piano_drain(q,&reader);assert(piano_on==4);
 assert(batch_piano_input(base,context,program,&a,2));piano_drain(q,&reader);assert(piano_on==5);
 assert(batch_piano_input_stop(base,context));piano_drain(q,&reader);assert(piano_off==5&&piano_last==3);
 assert(batch_piano_input(base,context,program,&a,2));FIELD(things,0xb8,void*)=NULL;piano_drain(q,&reader);assert(piano_on==5);
 FIELD(things,0xb8,void*)=program;
 struct step_event plain=make_note(38,0,.8f),first_chop=plain;event_choice_set(&first_chop,15,1);
 assert(batch_piano_input(base,context,program,&plain,1)&&batch_piano_input(base,context,program,&first_chop,2));piano_drain(q,&reader);
 assert(batch_piano_input(base,context,NULL,NULL,2)&&batch_piano_input(base,context,NULL,NULL,1));piano_drain(q,&reader);
 assert(piano_choices[5]==(0x1f00u|first_chop.data[21])&&piano_choices[6]==plain.data[6]&&piano_last==4);
 puts("PASS Piano Roll musical queue: actual native worker/lifetime, armed input, copied velocity and L/C choices, polyphonic reverse releases, shared source audio release once, Stop cancels queued/held keys, project-change guard; sampler/recorder receivers are fixtures");return 0;
}
