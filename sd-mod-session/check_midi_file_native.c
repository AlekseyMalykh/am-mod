#define main legacy_steps_main
#include "check_steps.c"
#undef main
#include "src/midi_file_native.h"
#include "src/midi_tools.h"
static _Alignas(8) unsigned char midi_project[0x1300],midi_sequences[2][0x800],midi_track[0xc00],midi_song[0x60],midi_signatures[0x60],midi_signature_node[0x20];
static void *midi_sequence_lookup(void*p,int index){assert(p==midi_project&&index>=0&&index<2);return midi_sequences[index];}
static void *midi_list_lookup(void*p,void*t,int create){assert((p==midi_sequences[0]||p==midi_sequences[1])&&t==midi_track&&!create);return list_storage;}
int main(int argc,char**argv){
 assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);
 load_elf(argv[1],0x1800000000ULL);setup();phase="MIDI file real native snapshots and modifiers";
 stub(0x2909be0,midi_sequence_lookup);stub(0x2957ae0,midi_list_lookup);
 FIELD(midi_project,0,uintptr_t)=base+0x6ecc648;FIELD(midi_project,0xac0,float)=100.f;
 FIELD(things,0x368,void*)=midi_track;FIELD(midi_track,0xbb8,void*)=program;
 for(unsigned i=0;i<2;++i){FIELD(midi_sequences[i],0,uintptr_t)=base+0x6ecdff0;FIELD(midi_sequences[i],0x188,float)=i?90.f:120.f;FIELD(midi_sequences[i],0x491,unsigned char)=1;FIELD(midi_sequences[i],0x348,int64_t)=i?5760:7680;}
 FIELD(midi_sequences[0],0xe8,void*)=time_list;FIELD(midi_sequences[1],0xe8,void*)=midi_signatures;
 FIELD(midi_signatures,0x30,void*)=midi_signature_node;FIELD(midi_signature_node,0,void*)=midi_signatures+0x30;
 FIELD(midi_signature_node,0x10,int)=3;FIELD(midi_signature_node,0x14,int)=960;FIELD(midi_signature_node,0x18,int)=0;
 struct step_event a=make_note(2,119,.7f),b=make_note(3,480,.8f);event_choice_set(&a,0,7);event_choice_set(&a,15,3);insert_note(&a);insert_note(&b);
 struct step_event channel_test=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))(2,0,240,100,6,.5f);
 assert(((channel_test.type>>12)&15)==6);
 am_midi_reset();struct am_midi_map map={.out_kind=AM_MIDI_NOTE,.out_number=67,.out_channel=3};assert(am_midi_map_set(2,&map));
 struct am_smf d;am_smf_init(&d);char error[96];
 assert(am_midi_file_snapshot(base,midi_project,things,midi_sequences[0],NULL,&d,error));
 assert(d.count==2&&d.duration==7680&&d.notes[0].note==76&&d.notes[0].channel==2&&d.notes[0].tick==119&&d.notes[0].duration==240);
 assert(d.notes[1].note==3&&d.events[0].kind==AM_SMF_TEMPO&&d.events[0].value==500000);
 unsigned char *bytes=NULL;size_t size=0;assert(am_smf_write(&d,&bytes,&size,error));struct am_smf round;am_smf_init(&round);assert(am_smf_read(bytes,size,&round,error)&&round.count==2);free(bytes);
 int song_steps[]={0,2,1,1};FIELD(midi_song,0x30,void*)=song_steps;FIELD(midi_song,0x38,void*)=song_steps+4;
 assert(am_midi_file_snapshot(base,midi_project,things,NULL,midi_song,&d,error)&&d.count==6&&d.duration==21120);
 assert(d.notes[2].tick==7799&&d.notes[4].tick==15479);
 unsigned tempos=0,signatures=0;for(unsigned i=0;i<d.event_count;++i){
  if(d.events[i].kind==AM_SMF_TEMPO){assert(d.events[i].value==(tempos==2?666667u:500000u));++tempos;}
  if(d.events[i].kind==AM_SMF_SIGNATURE){assert(d.events[i].value>>24==(signatures==2?3u:4u));++signatures;}
 }assert(tempos==3&&signatures==3);
 FIELD(midi_song,0x28,unsigned char)=1;assert(am_midi_file_snapshot(base,midi_project,things,NULL,midi_song,&d,error));
 for(unsigned i=0;i<d.event_count;++i)if(d.events[i].kind==AM_SMF_TEMPO)assert(d.events[i].value==600000);
 struct am_smf_note *prior=d.notes;FIELD(midi_sequences[0],0x491,unsigned char)=0;FIELD(midi_project,0xac0,float)=NAN;
 assert(!am_midi_file_snapshot(base,midi_project,things,midi_sequences[0],NULL,&d,error)&&d.notes==prior&&d.count==6);
 am_smf_free(&d);am_smf_free(&round);
 puts("PASS ARM MIDI native snapshot: actual packed Tune+Chop, pad output map, channels, gate lengths, tempo/master selection, time signatures, repeated Song steps and transactional rejection (track/sequence lookup fixtures)");
 return 0;
}
