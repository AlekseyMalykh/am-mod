#include "midi_ui.h"
#include "midi_tools.h"
#include "synth.h"
#include "note_native.h"
#include "music_ui.h"
#include "music_settings.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
#define FN(o,T) ((T)(image+(o)))
static uintptr_t image;
static void*root;
static void*input_host;
static _Atomic unsigned pad_hit; /* absolute pad + 1, published by the surface thread */
static _Atomic unsigned visible,page; /* 0 stock,1 tools,2 log,3 input map,4 output map */
static unsigned row,pad,field,edit,direction,port,type,channel,focus;
static unsigned map_layer;
static uint64_t scroll_anchor;
static _Atomic unsigned char held[112];
static unsigned held_count;
static uint64_t seen;
static char notice[56];
static const char*const menu[]={"MIDI Monitor","MIDI In Map","MIDI Out Map","MIDI Input Mode","MIDI Buttons","Synth"};
static unsigned mode_channel=1;
extern void phone_native_inject_button(unsigned code,unsigned down);
#pragma weak phone_native_inject_button
extern void phone_native_inject_controller(unsigned code,unsigned value);
#pragma weak phone_native_inject_controller
static const char*const filters[]={"All","Notes","CC","Program","Pitch/Pressure","Transport","Clock","SysEx"};
static void repaint(void){if(root&&image)FN(0x1149880,void(*)(void*,uint64_t,uint64_t))(root,0,F(root,0x28,uint64_t));}
static void rect(void*g,int x,int y,int w,int h,uint32_t c){FN(0x102d5e0,void(*)(void*,uint32_t))(g,c);FN(0x102e000,void(*)(void*,int,int,int,int))(g,x,y,w,h);}
static void text(void*g,const char*s,int x,int y,int w,int h,float size,uint32_t c,int align){
 uint64_t value=0,font=0;FN(0xf535c0,void(*)(void*,const char*))(&value,s);FN(0x1082810,void(*)(void*,int,float))(&font,0,size);FN(0x102dca0,void(*)(void*,void*))(g,&font);FN(0x102d5e0,void(*)(void*,uint32_t))(g,c);
 FN(0x10817b0,void(*)(void*,void*,int,int,int,int,int,int,float))(g,&value,x,y,w,h,align,1,.7f);FN(0x1070d60,void(*)(void*))(&font);FN(0xf0a290,void(*)(void*))(&value);
}
static void label(void*g,const char*s,int x,int y,int w,int h,unsigned size,uint32_t c){text(g,s,x,y,w,h,(float)size,c,0x21);}
static void centre(void*g,const char*s,int x,int y,int w,int h,unsigned size,uint32_t c){text(g,s,x,y,w,h,(float)size,c,0x24);}
static void heads(void*g,const char*a,const char*b,const char*c){const char*s[]={a,b,c};for(unsigned i=0;i<3;++i){int x=(int)i*106;rect(g,x+2,2,i==2?105:102,26,0xff242424);centre(g,s[i],x+4,2,98,25,13,0xffffffff);}}
static void knob(void*g,unsigned col,const char*name,const char*value){int x=(int)col*106;rect(g,x+2,199,col==2?105:102,39,focus==col?0xff302412:0xff202020);centre(g,name,x+3,200,99,13,11,0xffaaaaaa);centre(g,value,x+3,214,99,20,14,focus==col?0xffffc247:0xffffffff);}
void am_midi_ui_pad_hit(unsigned p){
 if(p>=128||atomic_load(&page)<3||atomic_load(&page)>5)return;
 am_midi_learn_retarget(p);atomic_store(&pad_hit,p+1);
}
void am_midi_ui_init(uintptr_t b){image=b;am_midi_io_ready();}
int am_midi_ui_active(void){return atomic_load(&visible)&&atomic_load(&page);}
void am_midi_ui_destroyed(void*p){if(input_host==p)input_host=NULL;if(root==p){root=NULL;atomic_store(&visible,0);atomic_store(&page,0);am_midi_learn(0,0);}}
void am_midi_ui_open_input(void*p){
 if(!p)return;
 input_host=root=p;atomic_store(&visible,1);atomic_store(&page,5);notice[0]=0;edit=0;
 am_midi_learn(0,0);repaint();
}
int am_midi_ui_input_host(void*p){return p&&p==input_host&&atomic_load(&page)!=0;}
static void mode_assign_pad(unsigned p);
void am_midi_ui_tick(void*p){
 if(input_host){
  if(FN(0x112fd50,int(*)(void*))(input_host)&&!FN(0x112f6b0,int(*)(void*))(input_host))p=input_host;
  else {am_midi_ui_destroyed(input_host);input_host=NULL;}
 }
 root=p;atomic_store(&visible,p!=NULL);if(!p){atomic_store(&page,0);am_midi_learn(0,0);}am_midi_learn_tick();am_midi_diagnostic_tick();music_global_sync(0);
 {unsigned c,d;while(am_midi_button_take(&c,&d)){if(p&&atomic_load(&page)==6)continue;if(c&0x80u){if(phone_native_inject_controller)phone_native_inject_controller(c&0x7fu,d);}else if(phone_native_inject_button){unsigned sh=c&0x200u;c&=0x1ffu;if(sh&&d)phone_native_inject_button(0x31,1);phone_native_inject_button(c,d);if(sh&&d)phone_native_inject_button(0x31,0);} /* Shift is only a tap: held MIDI key must not hold MPC Shift */}}
 {unsigned hit=atomic_exchange(&pad_hit,0);if(hit&&atomic_load(&page)>=3&&atomic_load(&page)<=5){pad=hit-1;notice[0]=0;if(atomic_load(&page)==5)mode_assign_pad(pad);repaint();}}
 /* Only copied program note numbers leave the UI owner. No native pointer is
  * read by an ALSA worker. Native pad-to-note getter 157f800 reads Program+12a8;
  * inverse getter 157f7d0 reads +14a8. Validate the reciprocal map. */
 void*things=note_native_context();void*program=things?F(things,0xb8,void*):NULL;
 if(program){void*instrument=F(things,0x668,void*);int selected=instrument?FN(0x286bc20,int(*)(void*,void*))(program,instrument):-1;am_midi_selected_pad(selected>=0&&selected<128?selected:-1);}
 if(program){int notes[128],valid=1;for(unsigned i=0;i<128;++i){notes[i]=F(program,0x12a8+i*4,int);if(notes[i]<0||notes[i]>127||F(program,0x14a8+notes[i]*4,int)!=(int)i)valid=0;}am_midi_native_notes(valid?notes:NULL);}else am_midi_native_notes(NULL);
 uint64_t revision=am_midi_log_revision();if(p&&atomic_load(&page)&&revision!=seen){seen=revision;repaint();}
}
static unsigned clamp(int n,unsigned hi);
static const char*const synth_waves[]={"Sine","Saw","Square","Triangle"};
static const char*const synth_names[]={"Wave","Cutoff","Attack","Decay","Sustain","Release","Level","MIDI channel","Mic (layer)","Fader","Direct out"};
static unsigned synth_cursor,synth_test;
static void synth_value(char*s,size_t n,unsigned i){unsigned v=am_synth_param_get(i);if(i==0)snprintf(s,n,"%s",synth_waves[v%4]);else if(i==7)snprintf(s,n,"%u",v+1);else if(i==8)snprintf(s,n,"%s",v?"On":"Off");else if(i==9)snprintf(s,n,"%s",v==1?"Mod":v==2?"Pitch":"Off");else if(i==10)snprintf(s,n,"%s",v?"On":"Off");else snprintf(s,n,"%u",v);}
static void synth_paint(void*g){
 heads(g,"Back","",synth_test?"Playing":"Test C4");centre(g,"Synth",4,31,312,22,16,0xffffc247);
 unsigned top=synth_cursor>7u?synth_cursor-7u:0; /* 8 rows fit above the hint and knobs: the list scrolls with the cursor */
 for(unsigned i=top;i<11u&&i<top+8u;++i){int y=52+(int)(i-top)*15;char v[16];synth_value(v,sizeof v,i);
  if(i==synth_cursor)rect(g,4,y,312,16,0xff302412);
  label(g,synth_names[i],12,y,140,16,13,i==synth_cursor?0xffffc247:0xffffffff);label(g,v,170,y,140,16,13,i==synth_cursor?0xffffc247:0xffffffff);}
 label(g,"DATA: row / change   B3: hold to play",8,174,304,12,10,0xff999999);
 char a[16],b[16],c[16];
 if(music_ui_shift_active()){synth_value(a,sizeof a,2);synth_value(b,sizeof b,3);synth_value(c,sizeof c,5);knob(g,0,"Attack",a);knob(g,1,"Decay",b);knob(g,2,"Release",c);}
 else{synth_value(a,sizeof a,0);synth_value(b,sizeof b,1);synth_value(c,sizeof c,6);knob(g,0,"Wave",a);knob(g,1,"Cutoff",b);knob(g,2,"Level",c);}
}
static unsigned synth_scaled(unsigned index,unsigned v){return index==0?v*4/128:index==7?v*16/128:index==10?v*2/128:v;}
static void synth_control(unsigned code,unsigned v,int delta,unsigned shift){
 static const unsigned plain[3]={0,1,6},fine[3]={2,3,5};
 if(code>=0x10&&code<=0x12){unsigned i=(shift?fine:plain)[code-0x10];am_synth_param_set(i,synth_scaled(i,v));synth_cursor=i;}
 else if(code==0x64&&delta){if(edit){int n=(int)am_synth_param_get(synth_cursor)+delta;if(synth_cursor==0)n=(n%4+4)%4;else if(synth_cursor==7)n=(n%16+16)%16;am_synth_param_set(synth_cursor,n<0?0:(unsigned)n);}else synth_cursor=clamp((int)synth_cursor+delta,11u-1);}
}
static int matches(const struct am_midi_record*r){
 if(direction&&r->direction!=direction-1)return 0;
 if(port&&r->port!=port)return 0;
 unsigned s=r->data[0],kind=s&0xf0;if(channel&&(s>=0xf0||(s&15)+1!=channel))return 0;
 return !type||(type==1&&(kind==0x80||kind==0x90))||(type==2&&kind==0xb0)||(type==3&&kind==0xc0)||(type==4&&(kind==0xa0||kind==0xd0||kind==0xe0))||(type==5&&(s==0xfa||s==0xfb||s==0xfc||s==0xf2))||(type==6&&(s==0xf8||s==0xfe))||(type==7&&s==0xf0);
}
static unsigned log_finish(const struct am_midi_record*all,const unsigned*indices,unsigned found){
 unsigned finish=found;if(scroll_anchor){while(finish&&all[indices[finish-1]].serial>scroll_anchor)--finish;unsigned minimum=found<7?found:7;if(finish<minimum)finish=minimum;}return finish;
}
static void scroll_by(int delta){
 struct am_midi_record all[AM_MIDI_LOG];unsigned indices[AM_MIDI_LOG],found=0;uint64_t latest;
 unsigned n=am_midi_snapshot(all,AM_MIDI_LOG,&latest);for(unsigned i=0;i<n;++i)if(matches(all+i))indices[found++]=i;
 if(!found){scroll_anchor=0;return;}int finish=(int)log_finish(all,indices,found)-delta,minimum=found<7?(int)found:7;
 if(finish<minimum)finish=minimum;
 if(finish>(int)found)finish=(int)found;
 scroll_anchor=finish<(int)found?all[indices[finish-1]].serial:0;
}
static void message_paint(void*g,const struct am_midi_record*r,int y){
 unsigned s=r->data[0],kind=s&0xf0;const char*name=kind==0x80?"Off":kind==0x90?"On":kind==0xa0?"PolyP":kind==0xb0?"CC":kind==0xc0?"PC":kind==0xd0?"Press":kind==0xe0?"Bend":s==0xf8?"Clock":s==0xfa?"Start":s==0xfb?"Cont":s==0xfc?"Stop":s==0xfe?"Sense":s==0xf0?"SysEx":"System";
 uint32_t color=r->direction?0xff86ceff:0xffb6ec9c;char ch[8],d1[8],d2[8];
 if(s<0xf0)snprintf(ch,sizeof ch,"%u",(s&15)+1);else snprintf(ch,sizeof ch,"--");
 snprintf(d1,sizeof d1,r->size>1?"%u":"--",r->data[1]);snprintf(d2,sizeof d2,r->size>2?"%u":"--",r->data[2]);
 centre(g,r->direction?"OUT":"IN",6,y,31,15,11,color);centre(g,r->port==1?"USB":r->port==2?"EXT":"?",42,y,42,15,12,color);
 centre(g,ch,89,y,25,15,12,color);label(g,name,124,y,73,15,12,color);centre(g,d1,203,y,43,15,12,color);centre(g,d2,254,y,43,15,12,color);
}
static void map_read(unsigned which,struct am_midi_map *m,unsigned *enabled){
 *enabled=1;
 if(which==4&&map_layer){
  am_midi_layer_get(pad,m,enabled);
  if(!*enabled)am_midi_layer_resolve(pad,AM_MIDI_LAYER_LEVELS,m);
  m->in_kind=m->in_port=m->in_channel=m->in_number=0;
 }else am_midi_map_get(pad,m);
}
static void pad_name(char*s,size_t n,unsigned v){if(!v)snprintf(s,n,"-");else snprintf(s,n,"%c%02u",'A'+(v-1)/16,(v-1)%16+1);}
static void mode_paint(void*g){
 struct am_midi_mode m;am_midi_mode_get(&m);unsigned sel=mode_channel-1;
 heads(g,"Back","Map / Learn",music_ui_shift_active()?"Clear":"Buttons");
 char title[48];snprintf(title,sizeof title,"MIDI Input: %s",m.mode==AM_MIDI_MODE_SELECTED?"Selected pad":m.mode==AM_MIDI_MODE_CHANNELS?"Channels":m.mode==AM_MIDI_MODE_INSTRUMENT?"Instrument":"Drum map");centre(g,title,4,31,312,20,15,0xffffc247);
 if(m.mode==AM_MIDI_MODE_INSTRUMENT){
  centre(g,"Play the Synth instrument",8,72,304,24,17,0xffffffff);
  centre(g,"Keys do not trigger pads",8,102,304,20,13,0xffaaaaaa);
  centre(g,"Channel All or 1-16. Sound: Mixer pad 13.",8,139,304,30,11,0xff999999);
  label(g,"DATA press: mode   B3: buttons   Shift+B3: clear",8,184,304,12,10,0xff999999);
  char ch[12];if(m.channel)snprintf(ch,sizeof ch,"%u",m.channel);else snprintf(ch,sizeof ch,"All");
  knob(g,0,"Channel",ch);knob(g,1,"Role","Synth");knob(g,2,"","");return;
 }
 if(m.mode==AM_MIDI_MODE_SELECTED){
  centre(g,"Play the selected MPC pad",8,72,304,24,17,0xffffffff);
  centre(g,"Tune / 16 Levels from your keyboard",8,102,304,20,13,0xffaaaaaa);
  centre(g,"Select another pad on the MPC to change sound.",8,139,304,30,11,0xff999999);
  label(g,"DATA press: mode   B3: buttons   Shift+B3: clear",8,184,304,12,10,0xff999999);
  char ch[12],r[12];if(m.channel)snprintf(ch,sizeof ch,"%u",m.channel);else snprintf(ch,sizeof ch,"All");snprintf(r,sizeof r,"%u",m.root);
  knob(g,0,"Channel",ch);knob(g,1,"Role","Tune");knob(g,2,"Root",r);return;
 }
 rect(g,4,53,312,16,0xff242424);label(g,"CH",12,53,40,16,11,0xff999999);label(g,"PAD",70,53,90,16,11,0xff999999);label(g,"ROLE",170,53,140,16,11,0xff999999);
 unsigned first=sel<3?0:sel>12?9:sel-3;
 for(unsigned r=0;r<7;++r){unsigned k=first+r;int y=70+(int)r*16;char c[8],p[8];
  if(k==sel)rect(g,4,y,312,16,0xff302412);
  snprintf(c,sizeof c,"%u",k+1);pad_name(p,sizeof p,m.pad[k]);
  uint32_t col=k==sel?0xffffc247:0xffffffff;
  label(g,c,12,y,40,16,13,col);label(g,m.pad[k]?p:"All pads",70,y,90,16,13,col);
  label(g,!m.pad[k]?"Stock drum map":m.kind[k]==AM_MIDI_KEY_CHOP?"Chop (16 keys)":m.kind[k]==AM_MIDI_KEY_SYNTH?"Synth":"Tune (16 Levels)",170,y,140,16,13,col);
 }
 label(g,"Tap pad: channel   DATA press: mode   B3: buttons",8,184,304,12,10,0xff999999);
 char a[24],b[24],c2[24];snprintf(a,sizeof a,"%u",mode_channel);
 snprintf(b,sizeof b,"%s",!m.pad[sel]?"Drum":m.kind[sel]==AM_MIDI_KEY_CHOP?"Chop":m.kind[sel]==AM_MIDI_KEY_SYNTH?"Synth":"Tune");
 snprintf(c2,sizeof c2,"%s",music_ui_shift_active()?"Root":"Pad");
 char pv[24];if(music_ui_shift_active())snprintf(pv,sizeof pv,"%u",m.root);else pad_name(pv,sizeof pv,m.pad[sel]);
 knob(g,0,"Channel",a);knob(g,1,"Role",b);knob(g,2,c2,pv);
}
static void mode_assign_pad(unsigned p){
 struct am_midi_mode m;am_midi_mode_get(&m);unsigned i=mode_channel-1;
 if(m.mode==AM_MIDI_MODE_SELECTED||m.mode==AM_MIDI_MODE_INSTRUMENT)return;
 m.pad[i]=(uint8_t)(p+1);m.mode=AM_MIDI_MODE_CHANNELS;am_midi_mode_set(&m);
}
static void mode_control(unsigned code,unsigned v,int delta,unsigned shift){
 struct am_midi_mode m;am_midi_mode_get(&m);unsigned i=mode_channel-1;
 if(m.mode==AM_MIDI_MODE_SELECTED||m.mode==AM_MIDI_MODE_INSTRUMENT){
  if(code==0x10)m.channel=(uint8_t)(v*17/128);
  else if(code==0x12)m.root=(uint8_t)v;
  else if(code==0x64){int n=(int)m.channel+delta;m.channel=(uint8_t)(n<0?0:n>16?16:n);}
  am_midi_mode_set(&m);return;
 }
 if(code==0x10)mode_channel=1+v*16/128;
 else if(code==0x11){unsigned a=v*4/128;if(!a)m.pad[i]=0;else{m.kind[i]=(uint8_t)(a==2?AM_MIDI_KEY_CHOP:a==3?AM_MIDI_KEY_SYNTH:AM_MIDI_KEY_TUNE);if(!m.pad[i])m.pad[i]=1;}}
 else if(code==0x12){if(shift)m.root=(uint8_t)v;else m.pad[i]=(uint8_t)(v*129/128>128?128:v*129/128);}
 else if(code==0x64){int n=(int)mode_channel+delta;mode_channel=(unsigned)(n<1?1:n>16?16:n);}
 if(code==0x11||(code==0x12&&!shift))m.mode=AM_MIDI_MODE_CHANNELS;
 am_midi_mode_set(&m);
}
static void button_name(char*s,size_t n,unsigned code){
 if(code&0x80){unsigned k=code&0x7f;if(k==0x64)snprintf(s,n,"Data encoder");else if(k==0x14)snprintf(s,n,"Fader");else snprintf(s,n,"K%u",k-0x0f);}
 else if(code==0x50)snprintf(s,n,"Rec");else if(code==0x51)snprintf(s,n,"Stop");else if(code==0x52)snprintf(s,n,"Play");else snprintf(s,n,"Button %02x",code);
}
static unsigned bsel,bpick;
static void blearn(unsigned code){am_midi_button_learn_shift(music_ui_shift_active());am_midi_button_learn(code,1);}
static unsigned button_used(unsigned*idx){unsigned n=0;for(unsigned i=0;i<AM_MIDI_BUTTON_SLOTS;++i){struct am_midi_button b;am_midi_button_get(i,&b);if(b.kind)idx[n++]=i;}return n;}
static void button_paint(void*g){
 unsigned learning=am_midi_button_learning(),idx[AM_MIDI_BUTTON_SLOTS],n=button_used(idx);char nm[48];
 if(n&&bsel>=n)bsel=n-1;
 heads(g,"Back",learning||bpick?"Cancel":"Delete",learning||bpick?"":"Assign");centre(g,"MIDI Buttons",4,31,312,20,15,0xffffc247);
 label(g,bpick?"Press an MPC button or knob to assign":learning?"Move a control on your MIDI device...":"B3: assign (or press an MPC button)",8,53,304,16,11,learning||bpick?0xffffc247:0xff999999);
 unsigned shown=0,rows=learning?6:7,first=bsel>=rows?bsel-rows+1:0;
 if(learning){button_name(nm,sizeof nm,learning-1);rect(g,4,70,312,16,0xff302412);label(g,nm,12,70,120,16,13,0xffffc247);label(g,"waiting...",140,70,170,16,13,0xffffc247);++shown;}
 for(unsigned k=first;k<n&&shown<7;++k){struct am_midi_button b;am_midi_button_get(idx[k],&b);if(learning&&(unsigned)b.code+1u==learning)continue;
  char src[48],chan[8];int y=70+(int)shown*16;button_name(nm,sizeof nm,b.code);if(b.mkind&&b.mshift){char t[64];snprintf(t,sizeof t,"Shift+%s",nm);memcpy(nm,t,sizeof nm-1);nm[sizeof nm-1]=0;}
  if(b.channel)snprintf(chan,sizeof chan,"%u",b.channel);else snprintf(chan,sizeof chan,"All");
  if(b.mkind)snprintf(src,sizeof src,"%s %u + %s %u",b.mkind==1?"N":"CC",b.mnumber,b.kind==1?"N":"CC",b.number);else snprintf(src,sizeof src,"%s %u  ch %s  %s",b.kind==1?"Note":"CC",b.number,chan,b.port?am_midi_port_name(b.port):"Any");
  int sel=!learning&&!bpick&&k==bsel;if(sel)rect(g,4,y,312,16,0xff302412);
  label(g,nm,12,y,110,16,13,sel?0xffffc247:0xffffffff);label(g,src,128,y,184,16,12,sel?0xffffc247:0xffaaaaaa);++shown;}
 if(!shown)label(g,"No buttons assigned",8,70,304,16,13,0xff999999);
 label(g,learning||bpick?"B2: cancel   B1: back":"DATA: scroll   B2: delete   Shift+B2: all",8,204,304,16,11,0xff999999);
}
static void map_paint(void*g,unsigned which){
 static const char*in_names[]={"Off","Note","CC"};static const char*out_names[]={"Native","Off","Note","CC","Program"};
 struct am_midi_map m;unsigned enabled;map_read(which,&m,&enabled);heads(g,"Back",am_midi_learn_active()?"Learning":which==3?"Learn":map_layer?"Inherit":"Reset",edit?"Done":"Edit");
 char title[64];snprintf(title,sizeof title,"MIDI %s Map   %c%02u",which==3?"In":map_layer?"Out Levels":"Out Pads",'A'+pad/16,pad%16+1);centre(g,title,4,33,312,23,17,0xffffc247);
 unsigned kind=which==3?m.in_kind:m.out_kind,p=which==3?m.in_port:m.out_port,ch=which==3?m.in_channel:m.out_channel,num=which==3?m.in_number:m.out_number;
 const char*names[]={"Action","Port scope","Channel","Number"};char values[4][24];
 snprintf(values[0],24,"%s",which==3?in_names[kind]:!enabled?"Inherit Pads":out_names[kind]);snprintf(values[1],24,"%s",p?am_midi_port_name(p):"Any active");snprintf(values[2],24,ch?"%u":which==3?"All":"Native",ch);snprintf(values[3],24,"%u",num);
 for(unsigned i=0;i<4;++i){if(field==i)rect(g,4,61+(int)i*25,312,24,edit?0xff403016:0xff262626);label(g,names[i],10,61+(int)i*25,145,24,15,0xffffffff);label(g,values[i],166,61+(int)i*25,140,24,15,field==i?0xffffc247:0xffffffff);}
 label(g,*notice?notice:which==4?"Shift+K1: Pads / Levels (+Tune/Chop)":"Maps use the stock active MIDI port",8,166,304,25,11,*notice?0xffffc247:0xff999999);
 char sound[12];snprintf(sound,sizeof sound,"%c%02u",'A'+pad/16,pad%16+1);knob(g,0,which==4&&music_ui_shift_active()?"Layer":"Sound",which==4&&music_ui_shift_active()?(map_layer?"Levels":"Pads"):sound);knob(g,1,"Field",names[field]);knob(g,2,"Value",values[field]);
}
void am_midi_ui_paint(void*p,void*g){
 root=p;atomic_store(&visible,1);unsigned which=atomic_load(&page);
 if(!which){rect(g,108,2,104,26,0xff242424);centre(g,"Map / Log",110,2,100,25,13,0xffffc247);return;}
 rect(g,0,0,320,240,0xff000000);
 if(which==1){heads(g,"Back","","Open");centre(g,"MIDI Tools",4,35,312,25,19,0xffffffff);for(unsigned i=0;i<6;++i){if(row==i)rect(g,4,64+(int)i*23,312,22,0xff302412);label(g,menu[i],12,64+(int)i*23,295,22,15,row==i?0xffffc247:0xffffffff);}label(g,"DATA: select   Press / B3: open",8,206,304,22,12,0xffaaaaaa);return;}
 if(which==5){mode_paint(g);return;}
 if(which==6){button_paint(g);return;}
 if(which==7){synth_paint(g);return;}
 if(which>=3){map_paint(g,which);return;}
 heads(g,"Back",am_midi_log_paused()?"Resume":"Pause","Clear");centre(g,"MIDI Monitor",4,30,312,20,16,0xffffffff);
 struct am_midi_record all[AM_MIDI_LOG];uint64_t latest;unsigned n=am_midi_snapshot(all,AM_MIDI_LOG,&latest),indices[AM_MIDI_LOG],found=0;
 for(unsigned i=0;i<n;++i)if(matches(all+i))indices[found++]=i;
 unsigned finish=log_finish(all,indices,found),start=finish>7?finish-7:0;
 centre(g,"I/O",6,52,31,17,10,0xff999999);centre(g,"PORT",42,52,42,17,10,0xff999999);centre(g,"CH",89,52,25,17,10,0xff999999);label(g,"COMMAND",124,52,73,17,10,0xff999999);centre(g,"D1",203,52,43,17,10,0xff999999);centre(g,"D2",254,52,43,17,10,0xff999999);
 for(unsigned i=start;i<finish;++i)message_paint(g,all+indices[i],72+(int)(i-start)*15);
 rect(g,312,72,4,105,0xff242424);if(found){unsigned count=finish-start,h=count*105/found;if(h<8)h=8;unsigned travel=105-h,pos=found>count?start*travel/(found-count):0;rect(g,312,72+(int)pos,4,(int)h,0xffffc247);}
 if(!found)centre(g,"Waiting for matching MIDI...",8,105,304,30,14,0xff999999);
 uint64_t ms=finish?all[indices[finish-1]].ms:0;char info[96];snprintf(info,sizeof info,"%s %u drop%llu t%llu.%03us",am_midi_log_paused()?"PAUSED":scroll_anchor?"HISTORY":"LIVE",found,(unsigned long long)am_midi_log_dropped(),(unsigned long long)(ms/1000),(unsigned)(ms%1000));label(g,info,8,180,304,16,10,0xff999999);
 knob(g,0,"Direction",direction==1?"IN":direction==2?"OUT":"Both");knob(g,1,"Port",port?am_midi_port_name(port):"All");char c[20];snprintf(c,sizeof c,"%u",channel);knob(g,2,music_ui_shift_active()?"Channel":"Message",music_ui_shift_active()?(channel?c:"All"):filters[type]);
}
int am_midi_ui_owns(unsigned code,int button){
 if(button&&input_host&&code>=0x6bu&&code<=0x6du)code-=0x5fu; /* Input Config reports B1-B3 as 0x6b-0x6d */
 if(button&&code<112&&held[code])return 1;
 if(!atomic_load(&visible))return 0;
 if(!atomic_load(&page))return button&&code==0x0d;
 if(button&&atomic_load(&page)==6)return code<112&&code!=0x31;
 return button?(code==0x0c||code==0x0d||code==0x0e||code==0x6f||code==0x36||code==0x37):code==0x10||code==0x11||code==0x12||code==0x64||(code==0x14&&atomic_load(&page)==6);
}
static unsigned clamp(int n,unsigned hi){return n<0?0:(unsigned)n>hi?hi:(unsigned)n;}
static void change_field(unsigned value,int relative){
 struct am_midi_map m;unsigned enabled,which=atomic_load(&page);map_read(which,&m,&enabled);uint8_t*p=which==3?(uint8_t*)&m:(uint8_t*)&m+4;unsigned limit=field==0?(which==3?2:which==4&&map_layer?5:4):field==1?2:field==2?16:127;
 if(which==4&&map_layer&&field==0){
  unsigned before=enabled?(unsigned)m.out_kind+1:0;
  unsigned next=relative?clamp((int)before+(int)value,limit):value*(limit+1)/128;
  if(next)m.out_kind=(uint8_t)(next-1);
  am_midi_layer_set(pad,&m,next!=0);notice[0]=0;return;
 }
 p[field]=(uint8_t)(relative?clamp((int)p[field]+(int)value,limit):(value*(limit+1)/128));
 if(!(which==4&&map_layer?am_midi_layer_set(pad,&m,1):am_midi_map_set(pad,&m)))snprintf(notice,sizeof notice,"Conflicts with another input mapping");else notice[0]=0;
}
int am_midi_ui_control(void*e,unsigned shift){
 unsigned code=F(e,8,uint8_t),v=F(e,9,uint8_t),which=atomic_load(&page);if(!am_midi_ui_owns(code,0))return 0;
 int delta=v>64?(int)v-128:v==64?0:(int)v;focus=code>=0x10&&code<=0x12?code-0x10:focus;
 if(which==1){if(code==0x64)row=clamp((int)row+delta,5);}
 else if(which==2){if(code==0x10){direction=v*3/128;scroll_anchor=0;}else if(code==0x11){port=v*3/128;scroll_anchor=0;}else if(code==0x12){if(shift)channel=v*17/128;else type=v*8/128;scroll_anchor=0;}else if(code==0x64)scroll_by(-delta);}
 else if(which==6){
  unsigned idx[AM_MIDI_BUTTON_SLOTS],n=button_used(idx);
  if(code==0x64&&!bpick&&!am_midi_button_learning()){if(n)bsel=clamp((int)bsel+delta,n-1);}
  else if(!am_midi_button_learning()){bpick=0;am_midi_button_learn(code|0x80u,1);}
  repaint();}
 else if(which==5)mode_control(code,v,delta,shift);
 else if(which==7){synth_control(code,v,delta,shift);repaint();}
 else if(which>=3&&which!=7){if(code==0x10){if(which==4&&shift){map_layer=v*2/128;edit=0;}else pad=v;notice[0]=0;am_midi_learn(0,0);}else if(code==0x11)field=v*4/128;else if(code==0x12)change_field(v,0);else if(code==0x64){if(edit)change_field((unsigned)delta,1);else field=clamp((int)field+delta,3);}}
 repaint();return 1;
}
int am_midi_ui_button(void*e){
 unsigned code=F(e,8,uint8_t),down=F(e,9,uint8_t)!=0;
 if(code>=112)return 0;
 if(input_host&&code>=0x6bu&&code<=0x6du)code-=0x5fu; /* Input Config reports B1-B3 as 0x6b-0x6d */
 if(held[code]){if(!down){if(code==0x0e&&synth_test){synth_test=0;am_synth_note(60,0,0);}held[code]=0;if(held_count)--held_count;}return 1;}
 if(!atomic_load(&visible))return 0;
 unsigned which=atomic_load(&page);
 if(!am_midi_ui_owns(code,1)){
    /* Mode changes retain stock action. Shift/transport and pads remain
     * available; selecting a Sound uses K1 so incoming notes cannot move it. */
    if(down&&which&&(code==0x06||code==0x07||code==0x01)){atomic_store(&page,0);am_midi_learn(0,0);repaint();}return 0;
 }
 if(!down)return 0;
 held[code]=1;++held_count;
 if(!which){atomic_store(&page,1);row=0;}
 else if(code==0x0c){
  if(input_host&&which==5){void*p=input_host;input_host=NULL;atomic_store(&page,0);atomic_store(&visible,0);repaint();if(root==p)root=NULL;}
  else atomic_store(&page,input_host?5:which==1?0:1);
  am_midi_learn(0,0);edit=0;bpick=0;
 }
 else if(which==1&&(code==0x0e||code==0x6f)){atomic_store(&page,row+2);notice[0]=0;scroll_anchor=0;}
 else if(which==2){if(code==0x0d)am_midi_log_pause(!am_midi_log_paused());else if(code==0x0e){am_midi_log_clear();scroll_anchor=0;}else if(code==0x36)scroll_by(-7);else if(code==0x37)scroll_by(7);}
 else if(which==7){if(code==0x0e){synth_test=1;am_synth_note(60,100,1);}else if(code==0x6f)edit=!edit;}
 else if(which==6){
  unsigned l=am_midi_button_learning(),idx[AM_MIDI_BUTTON_SLOTS],n=button_used(idx);
  if(bpick){bpick=0;if(code!=0x0d)blearn(code);}
  else if(l){if(code==0x0d)am_midi_button_learn(0,0);else if(code!=0x0e)blearn(code);}
  else if(code==0x0e)bpick=1;
  else if(code==0x0d){if(music_ui_shift_active())am_midi_button_clear();else if(n){struct am_midi_button b;am_midi_button_get(idx[bsel<n?bsel:n-1],&b);am_midi_button_remove(b.code);}}
  else if(code!=0x0c&&code!=0x6f)blearn(code);
 }
 else if(which==5){if(code==0x0d){atomic_store(&page,3);edit=0;}
  else if(code==0x0e&&!music_ui_shift_active()){atomic_store(&page,6);bpick=0;notice[0]=0;} /* B3: MIDI Buttons (Back returns here) */
  else if(code==0x0e||code==0x6f){struct am_midi_mode m;am_midi_mode_get(&m);if(code==0x0e){memset(&m,0,sizeof m);m.root=60;}else m.mode=m.mode==AM_MIDI_MODE_DRUM?AM_MIDI_MODE_CHANNELS:m.mode==AM_MIDI_MODE_CHANNELS?AM_MIDI_MODE_SELECTED:m.mode==AM_MIDI_MODE_SELECTED?AM_MIDI_MODE_INSTRUMENT:AM_MIDI_MODE_DRUM;am_midi_mode_set(&m);}}
 else if(which>=3&&which!=7){if(code==0x0e||code==0x6f)edit=!edit;else if(code==0x0d){if(which==3)am_midi_learn(pad,!am_midi_learn_active());else if(map_layer){struct am_midi_map m;am_midi_layer_get(pad,&m,NULL);am_midi_layer_set(pad,&m,0);}else{struct am_midi_map m;am_midi_map_get(pad,&m);m.out_kind=m.out_port=m.out_channel=m.out_number=0;am_midi_map_set(pad,&m);}}else if(code==0x36||code==0x37)pad=clamp((int)pad+(code==0x36?1:-1),127);}
 repaint();return 1;
}
