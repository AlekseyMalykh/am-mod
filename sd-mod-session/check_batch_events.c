#define BATCH_PROOF_MAIN batch_base_proof
#include "check_batch_native.c"
#undef BATCH_PROOF_MAIN
#include "src/batch_events.h"
static unsigned char sequences[3][0x800],clips[3][0x900],nodes[3][0x40],track[0xc00];
static void*sequence_array[3];
static void quiet(void*p){(void)p;}
static unsigned event_performs;
static unsigned char real_event_perform[16];
static int fail_second_event_command(void*p){
 if(++event_performs==2)return 1;
 memcpy((void*)(base+0x19d8f00),real_event_perform,sizeof(real_event_perform));
 __builtin___clear_cache((char*)(base+0x19d8f00),(char*)(base+0x19d8f10));
 int result=FN(0x19d8f00,int(*)(void*))(p);
 stub(0x19d8f00,fail_second_event_command);
 __builtin___clear_cache((char*)(base+0x19d8f00),(char*)(base+0x19d8f10));return result;
}
static unsigned size_at(unsigned s){return (unsigned)FIELD(clips[s]+0x688,0x88,uint64_t);}
static int contains(unsigned s,const struct step_event*event){
 void*list=clips[s]+0x688,*end=(char*)list+0x68;
 for(void*n=FIELD(list,0x78,void*);n!=end;n=FN(0xb51738,void*(*)(void*))(n)){
  const struct step_event*actual=(void*)((char*)n+0x20);
  /* Bytes12..15 are native alignment padding, not an event property. */
  if(!memcmp(actual,event,12)&&!memcmp(actual->data,event->data,40))return 1;
 }
 return 0;
}
static void put(unsigned s,const struct step_event*e){FN(0x27ee5d0,void*(*)(void*,const void*))(clips[s]+0x688,e);}
int main(int argc,char**argv){
 assert(!batch_base_proof(argc,argv));
 phase="actual all-sequence Copy Events";
 stub(0x27ee100,quiet);stub(0x14c43c0,quiet);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 FIELD(properties,0,uintptr_t)=base+0x6ecc648;
 FIELD(properties,0x1250,void*)=sequence_array;FIELD(properties,0x125c,int)=3;
 FIELD(copy_current,0x368,void*)=track;
 for(unsigned i=0;i<3;++i){
  sequence_array[i]=sequences[i];FIELD(sequences[i],0,uintptr_t)=base+0x6ecdff0;
  /* A native track-key map with one Clip shared_ptr. The control block is
   * null for fixture lifetime;2957ae0/29579e0 themselves remain real. */
  FIELD(sequences[i],0x638,void*)=nodes[i];
  FIELD(nodes[i],0x20,void*)=track;FIELD(nodes[i],0x28,void*)=clips[i];
  FN(0x27eb120,void(*)(void*))(clips[i]+0x688);
 }
 for(unsigned i=0;i<128;++i)FIELD(copy_program,0x12a8+i*4,int)=(int)i;
 FIELD(copy_program,0x12a8,int)=36;FIELD(copy_program,0x12ac,int)=40;
 FIELD(copy_program,0x12a8+16*4,int)=52;FIELD(copy_program,0x12a8+17*4,int)=53;
 struct step_event a=make_note(36,240,.8f),b=make_note(40,720,.5f),c=make_note(36,960,.6f),existing=make_note(52,0,.7f);
 a.type|=0x200;FIELD(a.data,4,uint16_t)=0x8001;FIELD(a.data,6,unsigned char)=4;
 FIELD(b.data,4,uint16_t)=1;FIELD(b.data,6,unsigned char)=17;
 put(0,&a);put(0,&a);put(0,&existing);put(1,&b);put(2,&c);
 int before16=FIELD(pads[16],0x100,int),before17=FIELD(pads[17],0x100,int);
 unsigned src[2]={0,1},dst[2]={16,17};
 assert(batch_copy_submit_events(base,context,src,dst,2,1));
 assert(!native_command_result);
 struct step_event aa=a,bb=b,cc=c;aa.data[0x20]=52;bb.data[0x20]=53;cc.data[0x20]=52;
 assert(size_at(0)==5&&size_at(1)==2&&size_at(2)==2);
 assert(contains(0,&a)&&contains(0,&aa)&&contains(0,&existing)&&!contains(0,&bb));
 assert(contains(1,&b)&&contains(1,&bb)&&!contains(1,&aa));
 assert(contains(2,&c)&&contains(2,&cc)&&!contains(2,&aa));
 void*cmd=(void*)kept[0];
 assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x48,uintptr_t))(cmd)==0);
 assert(size_at(0)==3&&size_at(1)==1&&size_at(2)==1&&contains(0,&existing)&&!contains(0,&aa));
 assert(FIELD(pads[16],0x100,int)==before16&&FIELD(pads[17],0x100,int)==before17);
 assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x40,uintptr_t))(cmd)==0);
 assert(size_at(0)==5&&size_at(1)==2&&size_at(2)==2&&contains(0,&aa)&&contains(1,&bb));
 assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x48,uintptr_t))(cmd)==0);
 /* Sound-only does not touch any sequence. */
 assert(batch_copy_submit_events(base,context,src,dst,2,0));
 assert(size_at(0)==3&&size_at(1)==1&&size_at(2)==1);
 cmd=(void*)kept[0];assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x48,uintptr_t))(cmd)==0);
 /* Overlap copies from the original snapshots, never copies its own output. */
 unsigned cycle[2]={1,0};assert(batch_copy_submit_events(base,context,src,cycle,2,1));
 struct step_event ab=a,ba=b;ab.data[0x20]=40;ba.data[0x20]=36;
 assert(size_at(0)==5&&size_at(1)==2&&contains(0,&ab)&&contains(1,&ba));
 cmd=(void*)kept[0];assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x48,uintptr_t))(cmd)==0);
 /* A bad later sequence rejects before changing sounds or earlier lists. */
 int sound_before=FIELD(pads[16],0x100,int);FIELD(sequences[2],0,uintptr_t)=0;
 assert(!batch_copy_submit_events(base,context,src,dst,2,1));
 assert(size_at(0)==3&&size_at(1)==1&&size_at(2)==1&&FIELD(pads[16],0x100,int)==sound_before);
 FIELD(sequences[2],0,uintptr_t)=base+0x6ecdff0;
 memcpy(real_event_perform,(void*)(base+0x19d8f00),sizeof(real_event_perform));
 stub(0x19d8f00,fail_second_event_command);__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 assert(!batch_copy_submit_events(base,context,src,dst,2,1));
 assert(event_performs>=2&&size_at(0)==3&&size_at(1)==1&&size_at(2)==1);
 assert(FIELD(pads[16],0x100,int)==sound_before&&contains(0,&existing)&&!contains(0,&aa));
 memcpy((void*)(base+0x19d8f00),real_event_perform,sizeof(real_event_perform));
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 puts("PASS actual native sequence getter and per-track Clip list getter; three independent sequence patterns, full C/L/mute payload and duplicate notes, existing target notes, sound-only untouched, overlapping copy snapshots, single group Undo/Redo; native bulk event command ctor/perform/undo, audio/list notification adapters remain fixtures");
 return 0;
}
