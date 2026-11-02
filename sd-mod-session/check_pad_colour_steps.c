#define _GNU_SOURCE
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
int main(void){
 void*p=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(p!=MAP_FAILED);base=(uintptr_t)p;
 stub(0x3b774b0,cell_text,1);stub(0x3b72210,native_row,0);stub(0xf0a290,release_text,0);__builtin___clear_cache(p,(char*)p+0x7500000);
 stub(0xf535c0,text_make,0);stub(0x1082810,font_make,0);stub(0x102dca0,font_use,0);stub(0x1070d60,font_end,0);
 stub(0x102d5e0,ink_set,0);stub(0x102e000,grid_rect,0);stub(0x10817b0,grid_text,0);__builtin___clear_cache(p,(char*)p+0x7500000);
 music_reset();sound=0;assert(music_set_pad_colour(0,8));assert(music_set_pad_colour(17,7));assert(music_set_pad_colour(127,4));
 assert((uint32_t)step_palette_value(1,0,0,0,0,0,400)==0x2030ff);
 sound=17;assert((uint32_t)step_palette_value(1,0,0,0,0,0,400)==0xffe000);sound=0;
 for(unsigned colour=0;colour<MUSIC_SEQUENCE_COLOURS;++colour){
  assert(music_set_pad_colour(0,colour));uint32_t expected=colour?music_sequence_colours[colour]:PAD_CONTENT;
  assert((uint32_t)step_palette_value(1,0,0,0,0,0,400)==expected);
  assert((uint32_t)step_palette_value(1,1,0,0,0,0,400)==PAD_MUTED);
  assert((uint32_t)step_palette_value(1,0,0,1,0,0,0)==PAD_DANGER);
  assert((uint32_t)step_palette_value(1,0,0,0,1,0,0)==PAD_SELECT);
  assert((uint32_t)step_palette_value(1,0,0,0,0,1,0)==PAD_PLAYHEAD);
  assert(step_palette_value(0,0,0,0,0,0,0)==0);
 }
 assert(music_set_pad_colour(0,8));length=7680;menu=0;browse_bank=1;sound=16;sound_present=3;
 /* Sample playback used to force cyan before the ordinary Pad Colour
  * receiver. Keep assigned hues, bank identity and the .9 activity pulse. */
 atomic_store(&ticket,0);atomic_store(&playback_mask,3);atomic_store(&playback_colour_bank,0);
 uint64_t pulse=0;
 for(unsigned c=0;c<MUSIC_SEQUENCE_COLOURS;++c){
  assert(music_set_pad_colour(0,c));assert(step_native_light(0,&pulse));assert((uint32_t)pulse==(c?music_sequence_colours[c]:PAD_PLAYHEAD));
  uint32_t bits=(uint32_t)(pulse>>32);float brightness;memcpy(&brightness,&bits,4);assert(brightness==.9f);
 }
 assert(music_set_pad_colour(0,5));atomic_store(&playback_colour_bank,1);assert(step_native_light(1,&pulse)&&(uint32_t)pulse==music_sequence_colours[7]);
 atomic_store(&playback_colour_bank,-1);assert(step_native_light(1,&pulse)&&(uint32_t)pulse==PAD_PLAYHEAD); /* Levels/picker unchanged. */
 atomic_store(&playback_colour_bank,0);atomic_store(&native_mute,1);assert(!step_native_light(0,&pulse));atomic_store(&native_mute,0);
 atomic_store(&requested_erase,1);assert(!step_native_light(0,&pulse));atomic_store(&requested_erase,0);
 atomic_store(&playback_mask,0);assert(!step_native_light(0,&pulse));assert(music_set_pad_colour(0,8));
 puts("PASS Sample playback flashes all12 assigned pad hues at activity brightness; Stock, Levels/picker cyan preserved; bank, Mute/Erase and stopped-mask boundaries");
 atomic_store(&requested_sound,1);palette();assert((uint32_t)atomic_load(&lights[1])==0xffe000);
 assert((uint32_t)atomic_load(&lights[0])==PAD_SELECT||(uint32_t)atomic_load(&lights[0])==PAD_CONTENT);assert((uint32_t)atomic_load(&lights[2])==PAD_EMPTY);
 /* Sound chooser: selected white cursor alternates with its actual hue;
  * other pads stay steady, including after bank changes and empty slots. */
 for(unsigned b=0;b<8;++b){
  browse_bank=(int)b;sound=(int)b*16+5;sound_present=0xffff;
  for(unsigned c=0;c<MUSIC_SEQUENCE_COLOURS;++c){
   assert(music_set_pad_colour((unsigned)sound,c));
   uint32_t hue=c?music_sequence_colours[c]:PAD_CONTENT;
   for(unsigned ms=0;ms<1280;ms+=80){
    uint64_t light=sound_palette_value(5,ms);
    assert((uint32_t)light==(ms%640<240?PAD_SELECT:hue));
    assert((uint32_t)sound_palette_value(4,ms)!=(uint32_t)0);
   }
   sound_present=0;
   assert((uint32_t)sound_palette_value(5,0)==PAD_SELECT);
   assert((uint32_t)sound_palette_value(5,400)==(c?hue:PAD_EMPTY));
   assert((uint32_t)sound_palette_value(4,0)==PAD_EMPTY);
   sound_present=0xffff;
  }
 }
 browse_bank=0;sound=0;assert(music_set_pad_colour(0,8));
 puts("PASS held Sound cursor alternates white and assigned hue across all13 choices/eight banks; empty-selected visibility and other pads preserved");
 atomic_store(&requested_sound,0);
 assert(show_other_sounds);cursor_visible=0;was_playing=0;grid=2;page=0;sound=0;other_sound_steps=1;
 memset(slots,0,sizeof(slots));palette();
 assert((uint32_t)atomic_load(&lights[12])==PAD_CONTEXT&&atomic_load(&lights[13])==0);
 slots[0].count=1;palette();assert((uint32_t)atomic_load(&lights[12])==0x2030ff);
 slots[0].muted=1;palette();assert((uint32_t)atomic_load(&lights[12])==PAD_MUTED);
 memset(slots,0,sizeof(slots));other_sound_steps=0;
 /* Sparse and dense sequence context: only real other-sound steps get a
  * dim background, and own events win even where another sound also plays. */
 palette();for(unsigned key=0;key<16;++key)assert(atomic_load(&lights[key])==0);
 slots[2].count=slots[9].count=1;
 other_sound_steps=(1u<<1)|(1u<<2)|(1u<<12);palette();
 for(unsigned key=0;key<16;++key){
  unsigned step=(3-key/4)*4+key%4;
  uint64_t expected=(step==2||step==9)?pad_light(0x2030ff,PAD_BRIGHT_CONTENT)
      :(step==1||step==12)?pad_light(PAD_CONTEXT,PAD_BRIGHT_CONTEXT):0;
  assert(atomic_load(&lights[key])==expected);
 }
 other_sound_steps=0xffff;palette();
 for(unsigned key=0;key<16;++key){
  unsigned step=(3-key/4)*4+key%4;
  assert(atomic_load(&lights[key])==((step==2||step==9)?pad_light(0x2030ff,PAD_BRIGHT_CONTENT):pad_light(PAD_CONTEXT,PAD_BRIGHT_CONTEXT)));
 }
 show_other_sounds=0;palette();
 for(unsigned key=0;key<16;++key){unsigned step=(3-key/4)*4+key%4;assert(atomic_load(&lights[key])==((step==2||step==9)?pad_light(0x2030ff,PAD_BRIGHT_CONTENT):0));}
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
  event_row_paint(model,1,(void*)1,320,28,selected);assert(row_normal==0xffffe000);
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
 return 0;
}
