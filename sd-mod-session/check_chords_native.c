#define MPC_CHORDS 1
#define MPC_POLY_TEST_MAIN baseline_poly_main
#include "check_poly_record.c"
static struct step_event chord_played[32];
static unsigned chord_count,chord_off[32],chord_off_count;
static unsigned expected_source;
static void capture_chord(void*engine,int source,void*event,int flags){
 assert(engine==fake_engine&&!source&&flags==0x7ffffffa&&chord_count<32);
 chord_played[chord_count++]=*(struct step_event*)event;
 assert(F(event,0x30,unsigned char)==expected_source);
 assert(F(event,0x14,uint16_t)&1); /* Persisted L category even at zero pitch. */
}
static int chord_midi(void*engine,int pad,int flags,void*track){assert(engine==fake_engine&&(unsigned)pad==expected_source&&flags==0x7ffffffa&&track==(void*)0x4560);return 36;}
static void chord_release(void*engine,int source,int note,int flags,int extra){assert(engine==fake_engine&&source==1&&(unsigned)note==expected_source&&flags==0x7ffffffa&&!extra&&releasing);++release_count;}
static void capture_chord_off(void*recorder,int source,int destination,unsigned note){
 assert(recorder==(void*)0x2340&&source==1&&destination==7&&chord_off_count<32);
 chord_off[chord_off_count++]=note;
}
static int pitch(unsigned i){return FN(0x27e4230,int(*)(const void*,int,int))((char*)&chord_played[i]+0x14,0,0);}
static void value_only(void*property,float value){F(property,0x1c,float)=value;}
static void physical(void*global_controls,void*input,unsigned pad,unsigned status,unsigned velocity){
 _Alignas(8) unsigned char event[40]={0};F(event,0,void*)=input;F(input,0x10,void*)=global_controls;
 event[8]=(unsigned char)pad;event[16]=(unsigned char)status;event[17]=36;event[18]=(unsigned char)velocity;F(event,32,int)=3;
 FN(0x159ed30,void(*)(void*))(event);
}
int main(int argc,char**argv){
 assert(!baseline_poly_main(argc,argv));
 redirect(0x211b090,capture_chord);redirect(0x20e4da0,capture_chord_off);
 redirect(0x2114fc0,chord_midi);redirect(0x2118930,chord_release);release_count=0;
 unsigned char mode[0x80]={0},props[0x340]={0},gate[0x60]={0},things[0x150]={0},input[0xe0]={0};
 F(mode,0x48,void*)=gate;F(mode,0x50,void*)=props;F(mode,0x58,void*)=things;F(mode,0x68,void*)=input;
 F(things,0x140,void*)=(void*)0x4560;F(input,0xd8,void*)=fake_engine;
 F(props,0x88,int)=1;F(props,0x228,int)=3;atomic_store(&context,0x30001);
 chords_open();chords_set(CHORD_ARP,ARP_OFF);chords_context(0,0,0,0,1);
 input_pad(mode,0,.8f);assert(chord_count==3&&pitch(0)==0&&pitch(1)==4&&pitch(2)==7);
 for(unsigned i=0;i<3;++i)assert(F(&chord_played[i],0x10,float)==.8f);
 input_pad(mode,0,.6f);assert(chord_count==3);
 input_pad(mode,4,.5f);assert(chord_count==5&&pitch(3)==11&&pitch(4)==14);
 input_pad(mode,0,0);assert(chord_off_count==2&&!release_count);
 input_pad(mode,4,0);assert(chord_off_count==5&&release_count==1);
 for(unsigned i=0;i<5;++i){unsigned expected=CHOICE_MARK|((unsigned)F(&chord_played[i],0x16,unsigned char)<<8)|36;
  assert(chord_off[i]==expected);}
 for(unsigned pad=8;pad<12;++pad){chords_parameter(pad,1);chords_parameter(pad,0);}
 input_pad(mode,0,.7f);assert(chord_count==12);
 const int wanted[]={0,4,7,9,10,11,14};for(unsigned i=0;i<7;++i)assert(pitch(5+i)==wanted[i]);
 chords_close();input_pad(mode,0,0);assert(chord_off_count==12&&release_count==2);
 input_pad(mode,0,0);assert(chord_off_count==12);
 input_pad(mode,0,.6f);assert(chord_count==13); /* Native Poly input survives exit. */
 input_pad(mode,0,0);
 expected_source=12;F(props,0x158,unsigned)=12;atomic_store(&context,0x40c01);
 chords_open();chords_set(CHORD_ARP,ARP_OFF);chords_context(0,0,0,0,1);input_pad(mode,0,.9f);assert(chord_count==16);
 input_pad(mode,0,0);assert(chord_off_count==16);
 atomic_store(&context,0x50c00);input_pad(mode,0,.9f);assert(chord_count==16); /* Mono does not play a wrong note. */
 chords_close();
 atomic_store(&context,0x60c01);input_pad(mode,2,.6f);unsigned before=chord_count,off_before=chord_off_count;
 chords_open();chords_set(CHORD_ARP,ARP_OFF);chords_context(0,0,0,0,1);input_pad(mode,2,.4f);assert(chord_count==before);
 input_pad(mode,2,0);assert(chord_off_count==off_before+1); /* Pre-entry key still releases native Poly. */
 input_pad(mode,0,.5f);assert(chord_count==before+3);input_pad(mode,0,0);chords_close();
 /* Real physical MIDI -> PadControls -> distinct virtual down/up slots.
  * Only the last Engine calls and irrelevant UI float notifications are
  * captured. This reproduces the device failure missed by direct-input tests. */
 _Alignas(8) unsigned char global_controls[0x3200]={0},physical_input[0x18]={0},pad_values[16*0x128]={0},subscriber[32]={0};
 void*controls=global_controls+0x3180,*header=(char*)controls+8;
 F(controls,8,void*)=F(controls,16,void*)=subscriber;F(subscriber,0,void*)=F(subscriber,8,void*)=header;
 F(subscriber,24,void*)=mode;F(controls,0x48,unsigned)=16;F(controls,0x50,void*)=pad_values;
 F(mode,0,uintptr_t)=base+0x6d8dc28;redirect(0x14a7580,value_only);
 uintptr_t*slot=(uintptr_t*)(base+0x6d8dc48),patched=*slot;assert(patched==(uintptr_t)input_release);
 assert(!mprotect((void*)((uintptr_t)slot&~(uintptr_t)4095),4096,PROT_READ|PROT_WRITE));
 *slot=base+0x1545920;chord_count=chord_off_count=release_count=0;atomic_store(&context,0x70c01);
 chords_open();chords_set(CHORD_ARP,ARP_OFF);chords_context(0,0,0,0,1);physical(global_controls,physical_input,0,0x99,100);
 assert(chord_count==3);physical(global_controls,physical_input,0,0x89,0);
 struct chord_config c;chords_get(&c);assert(c.held_roots==1&&!chord_off_count); /* r24a release is empty. */
 physical(global_controls,physical_input,0,0x99,100);assert(chord_count==3); /* Retrigger blocked. */
 *slot=patched;physical(global_controls,physical_input,0,0x89,0);chords_get(&c);
 assert(!c.held_roots&&chord_off_count==3&&release_count==1);
 for(unsigned pass=0;pass<3;++pass){physical(global_controls,physical_input,0,0x99,100);physical(global_controls,physical_input,0,0x99,0);}
 assert(chord_count==12&&chord_off_count==12&&release_count==4);chords_get(&c);assert(!c.held_roots);
 physical(global_controls,physical_input,0,0x99,90);chords_close();physical(global_controls,physical_input,0,0x89,0);
 assert(chord_off_count==15&&release_count==5); /* Release still completes after Back. */
 unsigned old_count=chord_count;physical(global_controls,physical_input,2,0x99,100);physical(global_controls,physical_input,2,0x89,0);
 assert(chord_count==old_count+1&&chord_off_count==16); /* Ordinary Poly physical release. */
 assert(!mprotect((void*)((uintptr_t)slot&~(uintptr_t)4095),4096,PROT_READ));
 puts("PASS physical MIDI NoteOff and NoteOn-zero, repeated strikes, Back while held and normal Poly; old empty release reproduces stuck root and blocked retrigger");
 puts("PASS actual native Chords NoteOn payloads and pitch-specific recorder NoteOffs; overlapping roots, seven-note extensions, held exit, duplicate edges and ordinary Poly regression");
 return 0;
}
