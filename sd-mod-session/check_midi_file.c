#include "src/midi_file.h"
#include "src/midi_tools.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const unsigned char running[]={
 'M','T','h','d',0,0,0,6,0,0,0,1,1,0xe0,
 'M','T','r','k',0,0,0,31,
 0,0xff,0x51,3,7,0xa1,0x20,
 0,0x92,60,90,0x78,64,80,0x78,60,0,0x78,64,0,
 0,0xc2,12,0,0xb2,1,50,0,0xff,0x2f,0
};
int main(void){
 struct am_smf d;am_smf_init(&d);char error[96];
 assert(am_smf_read(running,sizeof(running),&d,error));
 assert(d.count==2&&d.tracks==1&&d.division==480&&d.duration==360&&d.event_count==3);
 assert(d.notes[0].tick==0&&d.notes[0].duration==240&&d.notes[0].channel==2&&d.notes[0].velocity==90);
 assert(d.notes[1].tick==120&&d.notes[1].duration==240);
 for(size_t size=0;size<sizeof(running);++size){struct am_smf_note *old=d.notes;assert(!am_smf_read(running,size,&d,error)&&d.notes==old&&d.count==2);}
 unsigned char *bytes=NULL;size_t size=0;assert(am_smf_write(&d,&bytes,&size,error));
 struct am_smf round;am_smf_init(&round);assert(am_smf_read(bytes,size,&round,error));
 assert(round.count==2&&round.notes[1].tick==120&&round.notes[1].duration==240&&round.event_count==3);free(bytes);
 struct am_smf_note track={1000,300,3,15,127,127};assert(am_smf_note_add(&d,track));
 assert(am_smf_write(&d,&bytes,&size,error)&&am_smf_read(bytes,size,&round,error));
 assert(round.tracks==4&&round.count==3&&round.duration==1300&&round.notes[2].channel==15);free(bytes);
 am_midi_reset();int native[128];for(unsigned p=0;p<128;++p)native[p]=-1;native[0]=60;native[1]=64;native[2]=127;
 struct am_smf_import_options o={.track=-1,.channel=-1,.mode=AM_SMF_PAD_MAP,.length=4000,.original_note=-1};
 struct am_smf_pad_note plan[8];unsigned count=999;struct am_smf_import_info info;
 assert(am_smf_import(&d,&o,native,plan,8,&count,&info,error)&&count==3);
 assert(plan[0].sound==0&&plan[0].duration==480&&plan[1].tick==240&&plan[2].tick==2000);
 struct am_midi_map map={.in_kind=AM_MIDI_INPUT_NOTE,.in_number=60,.in_channel=3,.in_port=AM_MIDI_USB};
 assert(am_midi_map_set(7,&map));assert(am_smf_import(&d,&o,native,plan,8,&count,&info,error)&&plan[0].sound==7);
 map.in_port=AM_MIDI_EXTERNAL;assert(am_midi_map_set(8,&map));count=999;
 assert(!am_smf_import(&d,&o,native,plan,8,&count,&info,error)&&info.ambiguous==1&&count==999);
 o.port=AM_MIDI_USB;assert(am_smf_import(&d,&o,native,plan,8,&count,&info,error)&&plan[0].sound==7);
 o.mode=AM_SMF_TUNE;o.sound=7;o.channel=2;
 assert(!am_smf_import(&d,&o,native,plan,8,&count,&info,error));o.original_note=0;
 assert(am_smf_import(&d,&o,native,plan,8,&count,&info,error)&&count==2&&plan[0].tune==0&&plan[1].tune==4);
 o.mode=AM_SMF_CHOP;o.chops=5;assert(am_smf_import(&d,&o,native,plan,8,&count,&info,error)&&plan[1].slice==5);
 o.chops=4;count=999;assert(!am_smf_import(&d,&o,native,plan,8,&count,&info,error)&&count==999&&info.out_of_range==1);
 o.skip_unmapped=1;assert(am_smf_import(&d,&o,native,plan,8,&count,&info,error)&&count==1);
 o.mode=AM_SMF_TUNE;o.length=240;o.skip_unmapped=0;o.fit=1;
 assert(am_smf_import(&d,&o,native,plan,8,&count,&info,error)&&count==2&&plan[1].tick+plan[1].duration<=o.length);
 o.fit=0;assert(!am_smf_import(&d,&o,native,plan,8,&count,&info,error));
 assert(am_smf_native_tick(0xffffffffu,1)==UINT64_C(0xffffffff)*960);
 am_smf_free(&d);am_smf_free(&round);am_smf_init(&d);
 assert(am_smf_note_add(&d,(struct am_smf_note){0,0,0,0,60,100}));
 assert(am_smf_note_add(&d,(struct am_smf_note){0xf0000000u,1,0,0,61,100}));
 assert(am_smf_write(&d,&bytes,&size,error)&&am_smf_read(bytes,size,&round,error));
 assert(round.count==2&&round.notes[0].duration==1&&round.notes[1].tick==0xf0000000u);free(bytes);
 am_smf_free(&d);am_smf_free(&round);
 puts("PASS SMF 0/1 running status, channel/tempo/CC/Program, tracks, PPQN, transactional truncation, large VLQ gaps, zero lengths, MIDI input map ambiguity, Tune/Chop import and explicit Fit/Skip");
 return 0;
}
