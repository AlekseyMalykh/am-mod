#include "src/midi_tools.h"
#include "src/midi_wire.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sound/asequencer.h>
extern unsigned char fake_input[],fake_output[];extern unsigned fake_in_size,fake_in_at,fake_out_size;extern long fake_limit,fake_write_error;
extern struct snd_seq_event fake_seq_in,fake_seq_out;extern int fake_seq_result;
int snd_rawmidi_open(void**,void**,const char*,int);int snd_rawmidi_close(void*);long snd_rawmidi_read(void*,void*,size_t);long snd_rawmidi_write(void*,const void*,size_t);
int snd_seq_connect_from(void*,int,int,int);int snd_seq_connect_to(void*,int,int,int);int snd_seq_event_input(void*,struct snd_seq_event**);int snd_seq_event_output_direct(void*,struct snd_seq_event*);
int main(void){
 void*in,*out,*surface;assert(!snd_rawmidi_open(&in,&out,"usb",0));assert(!snd_rawmidi_open(&surface,NULL,"surface",0));
 int notes[128];for(unsigned i=0;i<128;++i)notes[i]=(int)i;am_midi_native_notes(notes);am_midi_reset();
 struct am_midi_map m={1,1,1,60,2,1,3,67};assert(am_midi_map_set(5,&m));
 const unsigned char stream[]={0x90,60,100,0xf8,60,0};memcpy(fake_input,stream,sizeof stream);fake_in_size=sizeof stream;fake_in_at=0;
 unsigned char bytes[64];long n=snd_rawmidi_read(in,bytes,sizeof bytes);const unsigned char expected[]={0x90,5,100,0xf8,0x80,5,0};assert(n==7&&!memcmp(bytes,expected,7));
 unsigned char note[]={0x90,5,100};fake_limit=1;long result;unsigned retries=0;
 do{result=snd_rawmidi_write(out,note,3);assert(++retries<8);}while(result==-EAGAIN);
 assert(result==3&&fake_out_size==3&&fake_output[0]==0x92&&fake_output[1]==67);
 m.out_number=77;m.out_channel=7;assert(am_midi_map_set(5,&m));note[0]=0x80;fake_limit=8192;assert(snd_rawmidi_write(out,note,3)==3);assert(fake_out_size==6&&fake_output[3]==0x82&&fake_output[4]==67);
 /* Surface stream is byte-identical and completely absent from the log. */
 struct am_midi_record log[AM_MIDI_LOG];uint64_t before,after;am_midi_snapshot(log,AM_MIDI_LOG,&before);memcpy(fake_input,stream,sizeof stream);fake_in_size=sizeof stream;fake_in_at=0;
 assert(snd_rawmidi_read(surface,bytes,sizeof bytes)==(long)sizeof stream&&!memcmp(bytes,stream,sizeof stream));am_midi_snapshot(log,AM_MIDI_LOG,&after);assert(before==after);
 /* A failed write must neither appear as OUT_ACCEPTED nor consume routing state. */
 fake_write_error=-EAGAIN;note[0]=0x90;assert(snd_rawmidi_write(out,note,3)==-EAGAIN);am_midi_snapshot(log,AM_MIDI_LOG,&after);assert(before==after);
 fake_write_error=0;assert(snd_rawmidi_write(out,note,3)==3&&fake_output[7]==77);
 void*seq=(void*)0x1234;assert(!snd_seq_connect_from(seq,1,20,0));assert(!snd_seq_connect_to(seq,2,20,0));
 fake_seq_in.type=SNDRV_SEQ_EVENT_NOTEON;fake_seq_in.source.client=20;fake_seq_in.data.note.channel=0;fake_seq_in.data.note.note=60;fake_seq_in.data.note.velocity=90;struct snd_seq_event*event;
 assert(!snd_seq_event_input(seq,&event)&&event->data.note.note==5);
 struct snd_seq_event send={0};send.type=SNDRV_SEQ_EVENT_NOTEON;send.source.port=2;send.data.note.note=5;send.data.note.velocity=88;
 assert(!snd_seq_event_output_direct(seq,&send)&&fake_seq_out.data.note.channel==6&&fake_seq_out.data.note.note==77&&send.data.note.note==5);
 send.type=SNDRV_SEQ_EVENT_KEYPRESS;send.data.note.velocity=55;
 assert(!snd_seq_event_output_direct(seq,&send)&&fake_seq_out.type==SNDRV_SEQ_EVENT_KEYPRESS&&fake_seq_out.data.note.channel==6&&fake_seq_out.data.note.note==77&&fake_seq_out.data.note.velocity==55&&send.data.note.note==5);
 fake_seq_in.type=SNDRV_SEQ_EVENT_KEYPRESS;fake_seq_in.data.note.note=60;fake_seq_in.data.note.velocity=61;
 assert(!snd_seq_event_input(seq,&event)&&event->type==SNDRV_SEQ_EVENT_KEYPRESS&&event->data.note.note==5&&event->data.note.velocity==61);
 send.type=SNDRV_SEQ_EVENT_NOTEOFF;assert(!snd_seq_event_output_direct(seq,&send)&&fake_seq_out.type==SNDRV_SEQ_EVENT_NOTEOFF&&fake_seq_out.data.note.note==77);
 /* Native sender provenance chooses the inner layer, and bypasses legacy
  * mapping at this real interposer boundary. Off retains its original route. */
 struct am_midi_map level={.out_kind=AM_MIDI_NOTE,.out_channel=4,.out_number=60};assert(am_midi_layer_set(5,&level,1));
 struct am_midi_wire w={1,5,1,7},previous;
 send.type=SNDRV_SEQ_EVENT_NOTEON;send.data.note.channel=0;send.data.note.note=5;send.data.note.velocity=88;
 am_midi_wire_begin(&w,&previous);assert(!snd_seq_event_output_direct(seq,&send));am_midi_wire_end(&previous);
 assert(fake_seq_out.type==SNDRV_SEQ_EVENT_NOTEON&&fake_seq_out.data.note.channel==3&&fake_seq_out.data.note.note==67);
 level.out_number=95;level.out_channel=10;assert(am_midi_layer_set(5,&level,1));
 send.type=SNDRV_SEQ_EVENT_NOTEOFF;
 am_midi_wire_begin(&w,&previous);assert(!snd_seq_event_output_direct(seq,&send));am_midi_wire_end(&previous);
 assert(fake_seq_out.type==SNDRV_SEQ_EVENT_NOTEOFF&&fake_seq_out.data.note.channel==3&&fake_seq_out.data.note.note==67);
 /* Raw MIDI follows the same captured layer despite partial writes. */
 w.id=2;unsigned start=fake_out_size;note[0]=0x90;note[1]=5;note[2]=100;fake_limit=1;retries=0;
 do{am_midi_wire_begin(&w,&previous);result=snd_rawmidi_write(out,note,3);am_midi_wire_end(&previous);assert(++retries<8);}while(result==-EAGAIN);
 assert(result==3&&fake_out_size==start+3&&fake_output[start]==0x99&&fake_output[start+1]==102);
 am_midi_reset();note[0]=0x80;fake_limit=8192;
 am_midi_wire_begin(&w,&previous);assert(snd_rawmidi_write(out,note,3)==3);am_midi_wire_end(&previous);
 assert(fake_output[start+3]==0x89&&fake_output[start+4]==102);
 am_midi_diagnostic_tick();assert(!snd_rawmidi_close(in)&&!snd_rawmidi_close(out)&&!snd_rawmidi_close(surface));puts("PASS MIDI ALSA raw/sequencer, running status, partial writes, retry, native surface exclusion, live two-layer routes, captured releases after map/project reset and wire acknowledgements");return 0;
}
