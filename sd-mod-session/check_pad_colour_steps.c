#define _GNU_SOURCE
#define MPC_LOOP_FEEDBACK 1
#define MPC_MIXER_LIVE 1
/* Production Steps palette and Events row wrapper; capture the original
 * painter's inputs rather than run a display/audio service. */
#include "src/step_native.c"
#include <assert.h>
#include <stdlib.h>
static unsigned row_calls;
static uint32_t row_normal,row_selected,row_second;
static int row_selection;
static uint32_t draw_ink,frame_mask,label_ink[16];
static int drawn_numbers[16];
static unsigned frame_edges;
static void text_make(uint64_t*out,const char*s){*out=(uintptr_t)s;}
static void font_make(uint64_t*out,int style,float size){(void)style;(void)size;*out=0;}
static void font_use(void*g,const uint64_t*font){(void)g;(void)font;}
static void font_end(uint64_t*font){(void)font;}
static void ink_set(void*g,uint32_t ink){(void)g;draw_ink=ink;}
static void grid_rect(void*g,int x,int y,int w,int h){
 (void)g;assert(x>=0&&y>=5&&x+w<=320&&y+h<=240);
 if(w==75&&h==25)return;
 assert(draw_ink==WHITE);++frame_edges;
 if(w==77&&h==1&&y>=81&&(y-81)%28==0&&x>=3&&(x-3)%79==0){
  int row=(y-81)/28,col=(x-3)/79;assert(row<4&&col<4);frame_mask|=1u<<((3-row)*4+col);
 }
}
static void grid_text(void*g,const uint64_t*s,int x,int y,int w,int h,int align,int lines,float scale){
 (void)g;(void)align;(void)lines;(void)scale;assert(w==75&&h==23);
 int row=(y-83)/28,col=(x-4)/79;assert(row>=0&&row<4&&col>=0&&col<4);
 unsigned physical=(unsigned)((3-row)*4+col);drawn_numbers[physical]=atoi((const char*)(uintptr_t)*s);label_ink[physical]=draw_ink;
}
static void screen_paint(void*p,void*g){(void)p;(void)g;}
static const char*row_names[]={"Pad A01 C","Pad B02 L","Control","Pad H16 CL","Pad A00","Pad A17"};
static void cell_text(void*m,int row,const void*column,uint64_t*out){
 assert(m&&column&&row>=0&&row<6);*out=(uintptr_t)row_names[row];
}
static void release_text(uint64_t*s){*s=0;}
static void native_row(void*m,int row,void*g,int w,int h,int selected){
 assert(row>=0&&g==(void*)1&&w==320&&h==28);++row_calls;row_selection=selected;
 void*c=FIELD(m,0x20,void*);row_normal=FIELD(c,16,uint32_t);row_selected=FIELD(c,20,uint32_t);row_second=FIELD(c,40,uint32_t);
}
static void stub(uintptr_t off,void*fn,int string_result){
 uint32_t plain[]={0x58000050,0xd61f0200};
 uint32_t sret[]={0xaa0803e3,0x58000070,0xd61f0200,0xd503201f};
 unsigned n=string_result?16:8;memcpy((void*)(base+off),string_result?(void*)sret:(void*)plain,n);memcpy((void*)(base+off+n),&fn,8);
}
static unsigned char sample_instruments[128][0x80],sample_used[128],sample_program[8];
static unsigned active_voices[128];
int sample_playing_active(const void*p,unsigned pad){assert(p==sample_program&&pad<128);return active_voices[pad];}
static struct mixer_project sample_mix;
enum mixer_sample_gate mixer_phone_sample_gate(unsigned pad)
{return mixer_sample_project_gate(&sample_mix,pad);}
static void*sample_instrument(void*p,int n){assert(p==sample_program&&n>=0&&n<128);return sample_used[n]?sample_instruments[n]:NULL;}
static int sample_has_audio(void*p){for(unsigned n=0;n<128;++n)if(p==sample_instruments[n])return sample_used[n];assert(0);return 0;}
static void empty_sample_cursor_checks(void){
 unsigned char things[0x700]={0};
 FIELD(things,0x60,void*)=sample_instruments[0]; /* Deliberately stale loaded-pad cache. */
 stub(0x286bbe0,sample_instrument,0);stub(0x2804760,sample_has_audio,0);__builtin___clear_cache((char*)base,(char*)base+0x7500000);
 atomic_store(&ticket,0);atomic_store(&requested_shift,0);atomic_store(&requested_erase,0);
 memset(playback_until,0,sizeof(playback_until));memset(sample_native_rgb,0,sizeof(sample_native_rgb));
 for(unsigned mode=0;mode<4;++mode){
  assert(music_set_mod_setting(MUSIC_SETTING_BLINK,mode));unsigned period=mode==1?1200:mode==2?800:400;
  for(unsigned selected=0;selected<128;++selected){
   unsigned bank=selected/16,pad=selected%16,other=bank*16+(pad+1)%16;
   memset(sample_used,0,sizeof(sample_used));sample_used[other]=1;
   FIELD(things,0x5f8,unsigned)=selected;sample_focus_index=-1;
   for(unsigned elapsed=0;elapsed<2400;elapsed+=40){
    sample_palette_publish(things,sample_program,(int)bank,1,0,10000+elapsed);
    int white=!mode||elapsed%period<period/2;
    uint64_t got;assert(step_native_light(pad,&got));
    assert(got==pad_light(white?PAD_SELECT:0,white?.95f:0));
    for(unsigned j=0;j<16;++j)if(j!=pad){
     assert(step_native_light(j,&got));
     assert(got==pad_light(bank*16+j==other?music_identity_rgb(music_pad_colour(other)):0,bank*16+j==other?.30f:0)); /* fix6: dimmer rest level */
    }
   }
   assert(FIELD(things,0x60,void*)==sample_instruments[0]);
  }
 }
 memset(sample_used,0,sizeof(sample_used));FIELD(things,0x5f8,unsigned)=3;sample_focus_index=-1;
 sample_palette_publish(things,NULL,0,1,0,20000);uint64_t got;assert(step_native_light(3,&got)&&(uint32_t)got==PAD_SELECT);
 FIELD(things,0x5f8,unsigned)=128;sample_palette_publish(things,NULL,0,1,0,20040);
 for(unsigned j=0;j<16;++j)assert(step_native_light(j,&got)&&got==pad_light(0,0));
 assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_STANDARD)&&!step_native_light(3,&got));
 assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_MOD)&&music_mod_restore_recommended(3));
 sample_palette_publish(NULL,NULL,0,0,0,20100);
 puts("PASS Sample empty selected cursor: all128 slots/eight banks, stale loaded-pad cache, all4 pulse modes, immediate white on selection, empty off phase, steady loaded neighbours, absent program, invalid target and Standard pass-through");
}
static void sample_playing_checks(void){
 unsigned char things[0x700]={0};
 memset(sample_used,1,sizeof(sample_used));memset(sample_native_rgb,0,sizeof(sample_native_rgb));
 memset(playback_until,0,sizeof(playback_until));assert(music_set_mod_setting(MUSIC_SETTING_BLINK,2));
 for(unsigned bank=0;bank<8;++bank)for(unsigned pad=0;pad<16;++pad){
  unsigned n=bank*16+pad;FIELD(things,0x5f8,unsigned)=(n+1)%128;sample_focus_index=-1;
  for(unsigned colour=1;colour<MUSIC_IDENTITY_CHOICES;++colour){
   assert(music_set_pad_colour(n,music_colour_choice(colour)));active_voices[n]=1;
   sample_palette_publish(things,sample_program,(int)bank,1,0,40000);uint64_t out;
   assert(step_native_light(pad,&out)&&out==pad_light(music_identity_rgb(music_colour_choice(colour)),.95f));
   sample_palette_publish(things,sample_program,(int)bank,1,0,40400);
   assert(step_native_light(pad,&out)&&out==pad_light(0,0));
   atomic_store(&requested_shift,1);
   assert(step_native_light(pad,&out)&&out==pad_light(0,0));
   unsigned neighbour=(pad+2)%16;
   assert(step_native_light(neighbour,&out)&&out==pad_light(music_identity_rgb(music_pad_colour(bank*16+neighbour)),.30f));
   atomic_store(&requested_shift,0);
   active_voices[n]=0;sample_palette_publish(things,sample_program,(int)bank,1,0,40800);
   assert(step_native_light(pad,&out)&&out==pad_light(music_identity_rgb(music_colour_choice(colour)),.30f));
  }
 }
 /* Cursor is still white; its colour phase retains real playback intensity. */
 FIELD(things,0x5f8,unsigned)=0;sample_focus_index=-1;active_voices[0]=1;
 sample_palette_publish(things,sample_program,0,1,0,40000);uint64_t out;assert(step_native_light(0,&out)&&(uint32_t)out==PAD_SELECT);
 sample_palette_publish(things,sample_program,0,1,0,40400);assert(step_native_light(0,&out)&&out==pad_light(music_identity_rgb(music_pad_colour(0)),.95f));
 /* Moving the cursor away exposes the playing pad's real off phase. */
 FIELD(things,0x5f8,unsigned)=1;sample_palette_publish(things,sample_program,0,1,0,40440);
 assert(step_native_light(0,&out)&&out==pad_light(0,0));assert(step_native_light(1,&out)&&(uint32_t)out==PAD_SELECT);
 /* Recorded muted events keep their grey flash above the loop off phase. */
 FIELD(sample_instruments[0],0x78,unsigned char)=1;FIELD(things,0x5f8,unsigned)=0;playback_until[0]=41000;
 sample_palette_publish(things,sample_program,0,1,0,40480);assert(step_native_light(0,&out)&&out==pad_light(PAD_MUTED,.65f));
 playback_until[0]=0;FIELD(things,0x5f8,unsigned)=1;
 sample_palette_publish(things,sample_program,0,1,0,40520);assert(step_native_light(0,&out)&&out==pad_light(0,0));
 sample_palette_publish(things,sample_program,0,1,0,40800);assert(step_native_light(0,&out)&&out==pad_light(PAD_MUTED,.65f));
 sample_used[2]=0;FIELD(things,0x5f8,unsigned)=0;
 sample_palette_publish(things,sample_program,0,1,1,40800);
 /* fix6 agreed Mute: muted = danger colour, others keep their own colour, brighter. */
 assert(step_native_light(0,&out)&&out==pad_light(PAD_DANGER,.70f));
 assert(step_native_light(1,&out)&&out==pad_light(music_identity_rgb(music_pad_colour(1)),.70f));
 assert(step_native_light(2,&out)&&out==0);
 FIELD(sample_instruments[0],0x78,unsigned char)=0;
 active_voices[0]=0;sample_palette_publish(NULL,NULL,0,0,0,50500);
 puts("PASS active Sample/Loop voices blink assigned hue/OFF across128 pads/eight banks/eight colours with transport stopped; moving selection keeps the playing pad blinking, stop restores content; selected white cursor keeps priority");
}
static void mixer_sample_feedback_checks(void)
{
 unsigned char things[0x700]={0};
 FIELD(things,0x5f8,unsigned)=0;
 for(unsigned i=0;i<4;++i){
  sample_used[i]=i!=3;
  active_voices[i]=1;
  playback_until[i]=60900;
  step_native_observe_rgb(i,0xffffffu);
 }
 sample_mix.output[0]=1;
 sample_mix.output[1]=2;
 sample_mix.track[0].mix.mute=1;
 sample_mix.track[0].mix.solo=1;
 uint32_t hue=music_identity_rgb(music_pad_colour(1));
 uint64_t light;
 sample_palette_publish(things,sample_program,0,1,0,60000);
 assert(step_native_light(0,&light)&&light==pad_light(MIXER_MUTE_RGB,1.f));
 for(unsigned now=60100;now<=60500;now+=400){
  sample_palette_publish(things,sample_program,0,1,0,now);
  uint32_t mute_phase=now%1000<500?MIXER_MUTE_RGB:music_identity_rgb(music_pad_colour(0));
  assert(step_native_light(0,&light)&&light==pad_light(mute_phase,1.f));
  assert(step_native_light(1,&light)&&light==pad_light(hue,.12f));
  assert(step_native_light(2,&light)&&light==pad_light(music_identity_rgb(music_pad_colour(2)),.12f));
  /* Empty slots keep their existing event indication and can never acquire a Mixer mute colour. */
  assert(step_native_light(3,&light)&&light==pad_light(music_identity_rgb(music_pad_colour(3)),.80f));
 }
 sample_mix.track[0].mix.mute=0;
 assert(mixer_phone_sample_gate(0)==MIXER_SAMPLE_SOLO_ACTIVE);
 sample_palette_publish(things,sample_program,0,1,0,61000);
 assert(step_native_light(0,&light)&&light==pad_light(MIXER_SOLO_RGB,.70f));
 sample_palette_publish(things,sample_program,0,1,0,61500);
 assert(step_native_light(0,&light)&&light==pad_light(music_identity_rgb(music_pad_colour(0)),.70f));
 sample_mix.track[1].mix.solo=1;
 assert(mixer_phone_sample_gate(1)==MIXER_SAMPLE_SOLO_ACTIVE);
 sample_mix=(struct mixer_project){0};
 for(unsigned i=0;i<4;++i){active_voices[i]=0;playback_until[i]=0;step_native_observe_rgb(i,0);}
 sample_palette_publish(NULL,NULL,0,0,0,62000);
 puts("PASS Sample Mixer Mute and Solo alternate with pad hue, preserve empty-pad events and gate Master Direct");
}
int main(void){
 void*p=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(p!=MAP_FAILED);base=(uintptr_t)p;
 stub(0x3b774b0,cell_text,1);stub(0x3b72210,native_row,0);stub(0xf0a290,release_text,0);__builtin___clear_cache(p,(char*)p+0x7500000);
 stub(0xf535c0,text_make,0);stub(0x1082810,font_make,0);stub(0x102dca0,font_use,0);stub(0x1070d60,font_end,0);
 stub(0x102d5e0,ink_set,0);stub(0x102e000,grid_rect,0);stub(0x10817b0,grid_text,0);__builtin___clear_cache(p,(char*)p+0x7500000);
 music_reset();assert(music_pad_lighting_mode()==MUSIC_LIGHTING_MOD&&music_mod_setting(MUSIC_SETTING_BLINK)==2);sound=0;assert(music_set_pad_colour(0,8));assert(music_set_pad_colour(17,7));assert(music_set_pad_colour(127,4));
 assert((uint32_t)step_palette_value(1,0,0,0,0,0,400)==0x2030ff);
 sound=17;assert((uint32_t)step_palette_value(1,0,0,0,0,0,400)==music_identity_rgb(7));sound=0;
 for(unsigned colour=0;colour<MUSIC_SEQUENCE_COLOURS;++colour){
  assert(music_set_pad_colour(0,colour));uint32_t expected=colour?music_identity_rgb(colour):PAD_CONTENT;
  assert((uint32_t)step_palette_value(1,0,0,0,0,0,400)==expected);
  assert((uint32_t)step_palette_value(1,1,0,0,0,0,400)==PAD_MUTED);
  assert((uint32_t)step_palette_value(1,0,0,1,0,0,0)==PAD_DANGER);
  assert((uint32_t)step_palette_value(1,0,0,0,1,0,0)==PAD_SELECT);
  assert((uint32_t)step_palette_value(1,0,0,0,0,1,0)==PAD_PLAYHEAD);
  assert(step_palette_value(0,0,0,0,0,0,0)==pad_light(0xffffff,.06f));
 }
 assert(music_set_pad_colour(0,8));length=7680;menu=0;browse_bank=1;sound=16;sound_present=3;
 /* Sample playback used to force cyan before the ordinary Pad Colour
  * receiver. Keep assigned hues, bank identity and the .9 activity pulse. */
 atomic_store(&ticket,0);atomic_store(&playback_mask,3);atomic_store(&playback_colour_bank,0);
 uint64_t pulse=0;
 for(unsigned c=0;c<MUSIC_SEQUENCE_COLOURS;++c){
  assert(music_set_pad_colour(0,c));assert(step_native_light(0,&pulse));assert((uint32_t)pulse==music_identity_rgb(c));
  uint32_t bits=(uint32_t)(pulse>>32);float brightness;memcpy(&brightness,&bits,4);assert(brightness==.9f);
 }
 assert(music_set_pad_colour(0,5));atomic_store(&playback_colour_bank,1);assert(step_native_light(1,&pulse)&&(uint32_t)pulse==music_identity_rgb(7));
 atomic_store(&playback_colour_bank,-1);assert(step_native_light(1,&pulse)&&(uint32_t)pulse==PAD_PLAYHEAD); /* Levels/picker unchanged. */
 atomic_store(&playback_colour_bank,0);atomic_store(&native_mute,1);assert(step_native_light(0,&pulse));atomic_store(&native_mute,0);
 atomic_store(&requested_erase,1);assert(!step_native_light(0,&pulse));atomic_store(&requested_erase,0);
 atomic_store(&playback_mask,0);assert(!step_native_light(0,&pulse));assert(music_set_pad_colour(0,8));
 puts("PASS Sample playback flashes all12 assigned pad hues at activity brightness; Stock, Levels/picker cyan preserved; bank, Mute/Erase and stopped-mask boundaries");
 atomic_store(&requested_sound,1);palette();assert((uint32_t)atomic_load(&lights[1])==music_identity_rgb(7));
 assert((uint32_t)atomic_load(&lights[0])==PAD_SELECT||(uint32_t)atomic_load(&lights[0])==PAD_CONTENT);assert((uint32_t)atomic_load(&lights[2])==PAD_EMPTY);
 /* Sound chooser: selected white cursor alternates with its actual hue;
  * other pads stay steady, including after bank changes and empty slots. */
 for(unsigned b=0;b<8;++b){
  browse_bank=(int)b;sound=(int)b*16+5;sound_present=0xffff;
  for(unsigned c=0;c<MUSIC_SEQUENCE_COLOURS;++c){
   assert(music_set_pad_colour((unsigned)sound,c));
   uint32_t hue=music_identity_rgb(c);
   for(unsigned ms=0;ms<1280;ms+=80){
    uint64_t light=sound_palette_value(5,ms);
    assert((uint32_t)light==(ms%800<400?PAD_SELECT:hue));
    assert((uint32_t)sound_palette_value(4,ms)!=(uint32_t)0);
   }
   sound_present=0;
   assert((uint32_t)sound_palette_value(5,0)==PAD_SELECT);
   assert((uint32_t)sound_palette_value(5,400)==PAD_EMPTY);
   assert((uint32_t)sound_palette_value(4,0)==PAD_EMPTY);
   sound_present=0xffff;
  }
 }
 browse_bank=0;sound=0;assert(music_set_pad_colour(0,8));
 puts("PASS held Sound cursor alternates white and assigned hue across all13 choices/eight banks; empty-selected visibility and other pads preserved");
 atomic_store(&requested_sound,0);
 assert(show_other_sounds);cursor_visible=0;was_playing=0;grid=2;page=0;sound=0;other_sound_steps=1;
 memset(slots,0,sizeof(slots));palette();
 assert(atomic_load(&lights[12])==pad_light(PAD_CONTEXT,.20f)&&atomic_load(&lights[13])==pad_light(0xffffff,.06f));
 slots[0].count=1;palette();assert((uint32_t)atomic_load(&lights[12])==0x2030ff);
 slots[0].muted=1;palette();assert((uint32_t)atomic_load(&lights[12])==PAD_MUTED);
 memset(slots,0,sizeof(slots));other_sound_steps=0;
 /* Sparse and dense sequence context: only real other-sound steps get a
  * dim background, and own events win even where another sound also plays. */
 palette();for(unsigned key=0;key<16;++key)assert(atomic_load(&lights[key])==pad_light(0xffffff,.06f));
 slots[2].count=slots[9].count=1;
 other_sound_steps=(1u<<1)|(1u<<2)|(1u<<12);palette();
 for(unsigned key=0;key<16;++key){
  unsigned step=(3-key/4)*4+key%4;
  uint64_t expected=(step==2||step==9)?pad_light(0x2030ff,.80f)
      :(step==1||step==12)?pad_light(PAD_CONTEXT,.20f):pad_light(0xffffff,.06f);
  assert(atomic_load(&lights[key])==expected);
 }
 other_sound_steps=0xffff;palette();
 for(unsigned key=0;key<16;++key){
  unsigned step=(3-key/4)*4+key%4;
  assert(atomic_load(&lights[key])==((step==2||step==9)?pad_light(0x2030ff,.80f):pad_light(PAD_CONTEXT,.20f)));
 }
 show_other_sounds=0;palette();
 for(unsigned key=0;key<16;++key){unsigned step=(3-key/4)*4+key%4;assert(atomic_load(&lights[key])==((step==2||step==9)?pad_light(0x2030ff,.80f):pad_light(0xffffff,.06f)));}
 /* A partial page must distinguish writable empties from out-of-range pads. */
 int64_t old_length=length;length=5*grid_ticks[grid];memset(slots,0,sizeof(slots));other_sound_steps=0;palette();
 for(unsigned key=0;key<16;++key)assert(atomic_load(&lights[key])==(pad_step(key)<5?pad_light(0xffffff,.06f):0));
 length=old_length;
 show_other_sounds=1;memset(slots,0,sizeof(slots));other_sound_steps=0;
 /* Actual production grid drawing must agree with physical pads, not just
  * with its own numbering. Cover mirrored pairs and every page position. */
 scan_ok=1;
 for(int pg=0;pg<2;++pg)for(unsigned physical=0;physical<16;++physical){
  unsigned logical=(3-physical/4)*4+physical%4;
  page=pg;cursor=pg*16+(int)logical;cursor_visible=1;
  memset(lights,0,sizeof(lights));atomic_store(&lights[physical],pad_light(PAD_SELECT,.8f));
  frame_mask=frame_edges=0;steps_grid_paint((void*)1);
  assert(frame_mask==(1u<<physical)&&frame_edges==8);
  for(unsigned key=0;key<16;++key)assert(drawn_numbers[key]==pg*16+(int)((3-key/4)*4+key%4)+1);
  cursor_visible=0;frame_mask=frame_edges=0;steps_grid_paint((void*)1);assert(!frame_mask&&!frame_edges);
  cursor_visible=1;cursor=(pg+1)*16;frame_mask=frame_edges=0;steps_grid_paint((void*)1);assert(!frame_mask&&!frame_edges);
 }
 page=0;cursor_visible=0;memset(lights,0,sizeof(lights));
 for(unsigned logical=0;logical<16;++logical){
  memset(slots,0,sizeof(slots));slots[logical].count=1;steps_grid_paint((void*)1);
  for(unsigned key=0;key<16;++key)assert(label_ink[key]==(((3-key/4)*4+key%4)==logical?WHITE:GREY));
 }
 puts("PASS production Steps grid: physical-pad frame and fill aligned at all16 positions on two pages; chronological numbering, event-label state and cursor hide/page bounds");
 showing=1;steps=0;menu=0;publish();assert(step_native_sound_layout());
 menu=1;publish();assert(!step_native_sound_layout());menu=0;steps=1;publish();assert(!step_native_sound_layout());showing=0;publish();assert(!step_native_sound_layout());
 unsigned char screen[0x2200]={0},model[0xa8]={0},columns[48]={0};root=screen;
 FIELD(root,0,uintptr_t)=base+SCREEN_VPTR;FIELD(root,0x1288,void*)=model;
 FIELD(model,0xa0,void*)=screen+0x11c8;FIELD(model,0x20,void*)=columns;FIELD(model,0x28,void*)=columns+48;
 FIELD(columns,16,uint32_t)=AMBER;FIELD(columns,20,uint32_t)=WHITE;FIELD(columns,40,uint32_t)=RED;
 for(int selected=0;selected<2;++selected){
  event_row_paint(model,0,(void*)1,320,28,selected);assert(row_normal==0xff2030ff&&row_selected==0xff2030ff&&row_second==RED&&row_selection==selected);
  assert(FIELD(columns,16,uint32_t)==AMBER&&FIELD(columns,20,uint32_t)==WHITE);
  event_row_paint(model,1,(void*)1,320,28,selected);assert(row_normal==(0xff000000u|music_identity_rgb(7)));
  event_row_paint(model,3,(void*)1,320,28,selected);assert(row_normal==0xffff20b0);
 }
 for(int i=2;i<6;++i)if(i!=3){event_row_paint(model,i,(void*)1,320,28,0);assert(row_normal==AMBER);}
 assert(music_set_pad_colour(0,0));event_row_paint(model,0,(void*)1,320,28,0);assert(row_normal==AMBER);
 assert(music_set_pad_colour(0,8));atomic_store(&dead,1);event_row_paint(model,0,(void*)1,320,28,0);assert(row_normal==AMBER);atomic_store(&dead,0);
 FIELD(model,0xa0,void*)=screen;event_row_paint(model,0,(void*)1,320,28,0);assert(row_normal==AMBER);
 uintptr_t*screen_slot=(uintptr_t*)(base+SCREEN_VPTR+0xd8),*row_slot=(uintptr_t*)(base+0x706d840);
 *screen_slot=base+0x10e2e10;*row_slot=base+0x3b72214;assert(!step_native_install(NULL)&&*screen_slot==base+0x10e2e10);
 *row_slot=base+0x3b72210;assert(step_native_install(screen_paint)&&*row_slot==(uintptr_t)event_row_paint);
 assert(row_calls);
 puts("PASS Pad Colour in Steps/Events: all colours, selected sound/bank, semantic priorities, row-specific C/L names, native paint arguments/styles restored, Stock/unrelated models unchanged, guarded row hook and String-result ABI");
 assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_STANDARD));
 assert((uint32_t)step_palette_value(1,0,0,0,0,0,400)==PAD_CONTENT);
 assert((uint32_t)step_palette_value(1,1,0,0,0,0,400)==PAD_DANGER);
 assert((uint32_t)step_palette_value(1,0,0,0,0,1,400)==PAD_FREE);
 atomic_store(&ticket,0);atomic_store(&sample_palette_enabled,1);assert(!step_native_light(0,&pulse));
 assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_MOD));
 puts("PASS Standard fixed Steps colours and native Sample pass-through; identity settings retained");
 empty_sample_cursor_checks();
 sample_playing_checks();
 mixer_sample_feedback_checks();
 return 0;
}
