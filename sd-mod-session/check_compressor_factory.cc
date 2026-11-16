#define modern_fx_probe compressor_unused_parent_probe
#include "check_modern_fx.cc"
#undef modern_fx_probe
#include "src/compressor.h"
static bool showing=true;
static unsigned wakes,repaints,type_labels,indicators,native_calls;
static unsigned shifted,shift_labels;
static unsigned white_badges,red_badges;static uint32_t ink;
static float displayed_gr=-1;
static unsigned focus_lines,cell_clears,focus_column;
static bool bypass_underline;
static bool warm_underline;
static unsigned warm_labels;
static int repeat_active;
extern "C" int music_ui_repeat_active(void){return repeat_active;}
extern "C" int music_ui_shift_active(void){return shifted!=0;}
static void(*native_specific)(void*,void*,void*);
extern "C" void compressor_phone_wake(void){++wakes;}
static int visible(void*){return showing;}
static int modal(void*){return 0;}
static void repaint(void*,uint64_t,uint64_t){++repaints;}
static void colour(void*,uint32_t value){ink=value;}
static void rectangle(void*,int x,int y,int w,int h){
 if(y==198&&h==1){assert(ink==0xffffc247u);++focus_lines;focus_column=(unsigned)(x/106);}
 if(y==198&&h==40&&ink==0xff000000u)++cell_clears;
 assert(x>=0&&y>=0&&w>=0&&h>=0&&x+w<=320&&y+h<=240);if(y==205&&w==2&&h==4&&(x==98||x==204||x==310)){
    if(ink==0xffff4747)++red_badges;else{assert(ink==0xffffffff);++white_badges;}
}if(w==5&&y==25){assert(h==1||h==3);++indicators;}if(x==215&&y==1&&w==105&&h==28){assert(ink==0xff202020);bypass_underline=false;}if(x==221&&y==25&&w==91&&h==1){assert(ink==0xffff4040);bypass_underline=true;}if(x==1&&y==1&&w==105&&h==28){assert(ink==0xff202020);warm_underline=false;}if(x==8&&y==25&&w==90&&h==1){assert(ink==0xffffc247);warm_underline=true;}}
static void font_create(uint64_t*f,int,float){*f=0;}
static void font_destroy(uint64_t*){}
static void font_set(void*,const uint64_t*){}
static void text(void*,const uint64_t*s,int x,int y,int w,int h,int,int,float){assert(x>=0&&y>=0&&x+w<=320&&y+h<=240);const char*t=reinterpret_cast<const char*>(*s);if(!strcmp(t,"Type"))++type_labels;if(!strcmp(t,"Warm"))++warm_labels;if(!strcmp(t,"Attack"))shift_labels|=1;if(!strcmp(t,"Release"))shift_labels|=2;if(!strcmp(t,"Knee"))shift_labels|=4;if(!strncmp(t,"GR ",3))assert(sscanf(t,"GR %f dB",&displayed_gr)==1);
 if(!strcmp(t,"S")&&y==201){
    if(ink==0xffff4747)++red_badges;else{assert(ink==0xffffffff);++white_badges;}
 }
}
static unsigned lit_meters(uintptr_t base){
 const uint32_t*p=compressor_image_pixels(base);assert(p);unsigned n=0;
 for(int y=0;y<164;++y)for(int x=268;x<312;++x)n+=p[y*320+x]==0xff00ffa7u||p[y*320+x]==0xffd31145u;
 return n;
}
static void redirect(uintptr_t base,uintptr_t offset,void*fn){uint32_t code[]={0x58000050,0xd61f0200};memcpy(reinterpret_cast<void*>(base+offset),code,8);memcpy(reinterpret_cast<void*>(base+offset+8),&fn,8);__builtin___clear_cache(reinterpret_cast<char*>(base+offset),reinterpret_cast<char*>(base+offset+16));}
static void counted(void*p,void*b,void*m){++native_calls;native_specific(p,b,m);}
struct ImageRef {
 void*data;
 ImageRef():data(nullptr){}
 ImageRef(const ImageRef&)=delete;
 ImageRef(ImageRef&&r):data(r.data){r.data=nullptr;}
 ~ImageRef(){if(data&&__atomic_sub_fetch(reinterpret_cast<unsigned*>(static_cast<char*>(data)+8),1,__ATOMIC_ACQ_REL)==0)reinterpret_cast<void(*)(void*)>((*reinterpret_cast<uintptr_t**>(data))[1])(data);}
};
struct ContextRef {
 void*data;
 ContextRef():data(nullptr){}
 ContextRef(const ContextRef&)=delete;
 ContextRef(ContextRef&&r):data(r.data){r.data=nullptr;}
 ~ContextRef(){if(data)reinterpret_cast<void(*)(void*)>((*reinterpret_cast<uintptr_t**>(data))[1])(data);}
};
extern "C" int stock_icon_draw(uintptr_t,void*,unsigned,int,int,int);
extern "C" int stock_project_icons(uintptr_t,void*);
static void check_image(uintptr_t base){
 /* Isolated loader skips app static constructors. PNG metadata uses JUCE
  * var's Bool and Void descriptors (normally initialized at b6fd04..b6fdc0). */
 *reinterpret_cast<uintptr_t*>(base+0x71e3af8)=base+0x6d76430;
 *reinterpret_cast<uintptr_t*>(base+0x71e3ad0)=base+0x6d76518;
 *reinterpret_cast<uintptr_t*>(base+0x71e3ad8)=base+0x6d768b8;

 uint32_t*source=compressor_image_pixels(base);assert(source);
 for(unsigned i=0;i<320*164;++i)source[i]=0xff000000u|((i*17)&255)*0x010101u;
 uintptr_t type=base+0x6d794c8;
 auto create=reinterpret_cast<ImageRef(*)(void*,int,int,int,bool)>(base+0x100dd30);
 ImageRef destination=create(&type,2,320,240,true);assert(destination.data);
 auto vt=*reinterpret_cast<uintptr_t**>(destination.data);
 ContextRef context=reinterpret_cast<ContextRef(*)(void*)>(vt[2])(destination.data);
 uintptr_t graphics[]={0,reinterpret_cast<uintptr_t>(context.data),0};assert(compressor_image_draw(graphics));
 auto out=*reinterpret_cast<uint32_t**>(static_cast<char*>(destination.data)+56);
 assert(!memcmp(out+320*34,source,320*164*4));
 assert(out[0]==0&&out[320*239]==0);
 for(unsigned i=0;i<9;++i){
  memset(out,0,320*240*4);assert(stock_icon_draw(base,graphics,i,10,10,18));
  unsigned lit=0;for(unsigned j=0;j<320*240;++j)if(out[j]){++lit;assert(j%320>=10&&j%320<28&&j/320>=10&&j/320<28);}
  assert(lit>10);
 }
 alignas(8) unsigned char project[0x600]{},model[32]{},rows[7*0x80]{};
 *reinterpret_cast<void**>(project+0x410+0xc0)=model;
 *reinterpret_cast<void**>(model+8)=rows;*reinterpret_cast<void**>(model+16)=rows+sizeof(rows);
 for(unsigned n=0;n<4;++n){
  ImageRef original=create(&type,2,16+(int)n*6,32-(int)n*4,true);assert(original.data);
  auto pixels=*reinterpret_cast<uint32_t**>(static_cast<char*>(original.data)+56);
  for(unsigned j=0;j<(16+n*6)*(32-n*4);++j)pixels[j]=0xffffffffu;
  for(unsigned k=8;k<=16;k+=8)reinterpret_cast<void(*)(void*,const void*)>(base+0x10333c0)(rows+n*0x80+k,&original.data);
 }
 assert(stock_project_icons(base,project));
 for(unsigned n=0;n<7;++n)for(unsigned k=8;k<=16;k+=8){
  void*image=*reinterpret_cast<void**>(rows+n*0x80+k);
  assert(image);assert(*reinterpret_cast<int*>(static_cast<char*>(image)+16)==24&&*reinterpret_cast<int*>(static_cast<char*>(image)+20)==24);
 }
 void*image=*reinterpret_cast<void**>(rows+4*0x80+8);
 unsigned references=*reinterpret_cast<unsigned*>(static_cast<char*>(image)+8);
 assert(!stock_project_icons(base,project));assert(*reinterpret_cast<unsigned*>(static_cast<char*>(image)+8)==references);
 for(unsigned n=0;n<7;++n)for(unsigned k=8;k<=16;k+=8)
  reinterpret_cast<void(*)(void*)>(base+0x10334b0)(rows+n*0x80+k);
 puts("PASS nine stock PNGs decode and blit inside bounds; all seven Project rows have uniform 24px Image cells; repaint preserves refcounts");
puts("PASS real native software Image creation and single blit; pixels match at y34, header/footer untouched");
}
static int button(unsigned code,unsigned down,uint64_t ticket){unsigned char e[16]{};e[8]=static_cast<unsigned char>(code);e[9]=static_cast<unsigned char>(down);int handled=compressor_ui_button(e,ticket);
 /* Stock B3 Value/event boundary is a fixture; its actual Colour parameter
  * and DSP are native. Production deliberately does not own this event. */
 if(!handled&&code==0x0e){static bool held=false;if(down&&!held)compressor_bypass();held=down!=0;return 1;}
 return handled;}
extern "C" int modern_fx_probe(uintptr_t base,const char**phase){
 native_base=base;
 *phase="OptoCat native software Image ABI";check_image(base);
 *phase="native Colour parameter descriptors";
 reinterpret_cast<void(*)()>(base+0xc034d0)();
 reinterpret_cast<void(*)(float)>(base+0xd8d320)(48000.f);
 alignas(8) unsigned char format[24]{},description[112]{},presets[256]{};
 uintptr_t preset_vtable[3]={0,0,reinterpret_cast<uintptr_t>(no_preset_directory)};
 *reinterpret_cast<uintptr_t**>(presets)=preset_vtable;
 new(presets+8)std::map<int,int>();new(presets+0x68)std::map<int,int>();void*empty=presets;
 reinterpret_cast<void(*)(void*,void*)>(base+0x2530ee0)(format,&empty);
 *reinterpret_cast<uint32_t*>(description+72)=0x43434d50;
 NativePtr instance;
 using Callback=std::function<void(NativePtr,const NativeText&)>;
 Callback callback=[&](NativePtr p,const NativeText&e){printf("Colour error=%s\n",e.data?e.data:"");instance=std::move(p);};
 *phase="native Colour factory";
 reinterpret_cast<void(*)(void*,const void*,double,int,Callback)>(base+0x2531110)(format,description,48000.,256,std::move(callback));
 assert(instance);auto*v=*reinterpret_cast<uintptr_t**>(instance.get());
 printf("Colour host vptr=%lx\n",reinterpret_cast<uintptr_t>(v)-base);
 unsigned n=reinterpret_cast<int(*)(void*)>(v[24])(instance.get());
 for(unsigned i=0;i<n;++i){auto name=reinterpret_cast<EmptyNativeString(*)(void*,int)>(v[25])(instance.get(),i);auto display=reinterpret_cast<EmptyNativeString(*)(void*,int)>(v[33])(instance.get(),i);printf("Colour p%u=%g %s [%s]\n",i,double(reinterpret_cast<float(*)(void*,int)>(v[27])(instance.get(),i)),name.data,display.data);}
 assert(n==6&&!compressor_select(COMP_OPTO));
 float native_parameters[6];for(unsigned i=0;i<6;++i)native_parameters[i]=reinterpret_cast<float(*)(void*,int)>(v[27])(instance.get(),i);
 *phase="native Colour prepare";reinterpret_cast<void(*)(void*,double,int)>(v[4])(instance.get(),48000.,256);
 OfflinePlayHead playhead;*reinterpret_cast<void**>(reinterpret_cast<char*>(instance.get())+16)=&playhead;
 printf("Engine rate=%g\n",double(*reinterpret_cast<float*>(base+0x7173b80)));
 redirect(base,0x112fd50,reinterpret_cast<void*>(visible));redirect(base,0x112f6b0,reinterpret_cast<void*>(modal));redirect(base,0x1149880,reinterpret_cast<void*>(repaint));
 redirect(base,0x102d5e0,reinterpret_cast<void*>(colour));redirect(base,0x102e000,reinterpret_cast<void*>(rectangle));redirect(base,0x1082810,reinterpret_cast<void*>(font_create));
 redirect(base,0x1070d60,reinterpret_cast<void*>(font_destroy));redirect(base,0x102dca0,reinterpret_cast<void*>(font_set));redirect(base,0x10817b0,reinterpret_cast<void*>(text));
 alignas(8) unsigned char screen[0x2700]{},meter[0x88]{};
 *reinterpret_cast<uintptr_t*>(screen)=base+0x6e1bf30;
 *reinterpret_cast<void**>(screen+0x2698)=meter;
 *reinterpret_cast<uintptr_t*>(meter)=base+0x6d8fac0;*reinterpret_cast<void**>(meter+0x80)=instance.get();
 compressor_ui_init(base);assert(compressor_ui_install());
 auto ui=reinterpret_cast<uintptr_t*>(base+0x6e1bf30);
 *phase="native Compressor Type binding";reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);
 assert(compressor_ready()&&wakes&&type_labels==1&&indicators==2&&compressor_ui_active());white_badges=red_badges=0;
 auto vt=*reinterpret_cast<uintptr_t**>(instance.get());native_specific=reinterpret_cast<void(*)(void*,void*,void*)>(vt[114]);vt[114]=reinterpret_cast<uintptr_t>(counted);
 float l[256],r[256],*channels[]={l,r};alignas(8) unsigned char audio[320]{},midi[16]{};
 *reinterpret_cast<int*>(audio)=2;*reinterpret_cast<int*>(audio+4)=256;*reinterpret_cast<float***>(audio+16)=channels;
 auto process=reinterpret_cast<void(*)(void*,void*,void*)>(vt[7]);
 auto audio_block=[&](unsigned block){for(unsigned i=0;i<256;++i)l[i]=r[i]=.7f*sinf(float(block*256+i)*.057f);process(instance.get(),audio,midi);playhead.frame+=256;for(unsigned i=0;i<256;++i)assert(std::isfinite(l[i])&&l[i]==r[i]);};
 *phase="Note Repeat owns every rate button above both compressor types";
 repeat_active=1;
 if(compressor_ui_ticket()&(UINT64_C(1)<<62)){
  for(unsigned algorithm=0;algorithm<2;++algorithm){compressor_select(algorithm);uint64_t rate_ticket=compressor_ui_ticket();
   for(unsigned code=0x0c;code<=0x0e;++code){unsigned char rate[16]{};rate[8]=static_cast<unsigned char>(code);rate[9]=1;
    assert(!compressor_ui_owns(2,code));assert(!compressor_ui_button(rate,rate_ticket));rate[9]=0;assert(!compressor_ui_button(rate,rate_ticket));assert(compressor_type()==algorithm);}
   repeat_active=0;unsigned char rate[16]{};rate[8]=0x0d;rate[9]=1;assert(!compressor_ui_button(rate,rate_ticket)&&compressor_type()==algorithm);repeat_active=1;
  }
  puts("PASS Note Repeat B1/B2/B3 and queued quick release pass through above Duck and OptoCat; Type unchanged");
 }
 repeat_active=0;compressor_select(COMP_DUCK);
 *phase="native Duck pass-through";audio_block(0);assert(native_calls==1&&compressor_type()==COMP_DUCK);
 uint64_t ticket=compressor_ui_ticket();
 if(compressor_ui_owns(1,0x64)){
  const unsigned saved_type_labels=type_labels,saved_indicators=indicators,saved_white=white_badges,saved_red=red_badges,saved_shift_labels=shift_labels;
  assert(compressor_type()==COMP_DUCK&&compressor_ui_owns(2,0x6f));
  float saved[4];for(unsigned i=0;i<4;++i)saved[i]=reinterpret_cast<float(*)(void*,int)>(v[27])(instance.get(),(int)i);
  unsigned char turn[16]{};turn[8]=0x64;
  focus_lines=cell_clears=0;reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);
  assert(focus_lines==1&&focus_column==0&&cell_clears==3);
  for(unsigned i=0;i<3;++i){
   if(i)assert(button(0x6f,1,ticket)&&button(0x6f,0,ticket));
   float old=reinterpret_cast<float(*)(void*,int)>(v[27])(instance.get(),(int)i);
   turn[9]=1;assert(compressor_ui_control(turn,ticket));
   assert(fabsf(reinterpret_cast<float(*)(void*,int)>(v[27])(instance.get(),(int)i)-fminf(1,old+1.f/127))<1e-6f);
   char label[64];assert(compressor_duck_text(i,label,sizeof(label))&&label[0]);
  }
  shifted=1;uint64_t back=compressor_ui_ticket();shifted=0;
  turn[9]=1;assert(compressor_ui_control(turn,back));
  assert(fabsf(reinterpret_cast<float(*)(void*,int)>(v[27])(instance.get(),3)-(saved[3]+1.f/127))<1e-6f);
  assert(button(0x6f,1,back)&&button(0x6f,0,back));
  float release=reinterpret_cast<float(*)(void*,int)>(v[27])(instance.get(),1);
  assert(compressor_ui_control(turn,back));assert(reinterpret_cast<float(*)(void*,int)>(v[27])(instance.get(),1)>release);
  for(unsigned i=0;i<4;++i)reinterpret_cast<void(*)(void*,int,float)>(v[40])(instance.get(),(int)i,saved[i]);
  assert(button(0x6f,1,ticket)&&button(0x6f,0,ticket));
  focus_lines=cell_clears=0;reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);
  assert(focus_lines==1&&cell_clears==3);
  for(unsigned algorithm=0;algorithm<2;++algorithm){
   assert(compressor_select(algorithm));
   for(unsigned step=0;step<6;++step){
    shifted=step>=3;uint64_t move=compressor_ui_ticket();
    assert(button(0x6f,1,move)&&button(0x6f,0,move));
    focus_lines=0;reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);
    const unsigned expected[]={0,1,2,1,0,2};assert(focus_lines==1&&focus_column==expected[step]);
   }
   shifted=0;
  }
  assert(compressor_select(COMP_DUCK));
  puts("PASS Duck/Opto initial K1, forward K1-K2-K3 and reverse K3-K2-K1 cyclic focus with matching amber cell edges");
  puts("PASS Duck DATA owns real native Attack/Release/Amount; Shift K3 edits In Boost; Shift click reverses columns; native text and other parameters preserved");
  type_labels=saved_type_labels;indicators=saved_indicators;white_badges=saved_white;red_badges=saved_red;shift_labels=saved_shift_labels;
 }
 assert(button(0x0d,1,ticket)&&compressor_type()==COMP_OPTO);
 assert(button(0x0d,1,ticket)&&compressor_type()==COMP_OPTO);assert(button(0x0d,0,ticket));
 *phase="Shift controls captured per queued event";
 comp_config before,after;compressor_config(&before);assert(before.gain==0);
 compressor_knob(2,0);compressor_config(&after);assert(after.gain==-6);
 compressor_knob(2,127);compressor_config(&after);assert(after.gain==6);
 compressor_knob(2,64);compressor_config(&after);assert(after.gain==0);
 assert(before.attack==10&&before.release==1200&&before.knee==6);
 {
  uint64_t saved=compressor_project_snapshot();compressor_project_reset();uint64_t defaults=compressor_project_snapshot();assert(compressor_project_restore(saved));
  const unsigned shifts[6]={2,10,17,25,34,43},bits[6]={255,127,255,511,511,63};
  for(unsigned i=0;i<6;++i){
   compressor_knob(i%3,127);compressor_shift_knob(i%3,127);uint64_t old=compressor_project_snapshot();
   compressor_reset_knob(i%3,i>=3);uint64_t now=compressor_project_snapshot(),mask=(uint64_t)bits[i]<<shifts[i];
   assert((now&mask)==(defaults&mask)&&(now&~mask)==(old&~mask));
  }
  assert(compressor_project_restore(saved));
  puts("PASS Opto per-knob reset restores exact normal/Shift defaults without changing the other five controls, Type, Warm or Bypass");
 }
 if(compressor_ui_owns(1,0x64)){
  uint64_t saved=compressor_project_snapshot();unsigned char turn[16]{};turn[8]=0x64;
  assert(compressor_ui_owns(2,0x6f));
  const unsigned shifts[]={2,10,17,25,34,43};
  shifted=1;uint64_t back=compressor_ui_ticket();shifted=0;
  for(unsigned column=0;column<3;++column){
   assert(button(0x6f,1,ticket)&&button(0x6f,0,ticket));
   for(unsigned layer=0;layer<2;++layer){
    uint64_t t=layer?back:ticket,old=compressor_project_snapshot();
    turn[9]=1;assert(compressor_ui_control(turn,t));
    assert(compressor_project_snapshot()==old+(UINT64_C(1)<<shifts[column+layer*3]));
    turn[9]=127;assert(compressor_ui_control(turn,t));assert(compressor_project_snapshot()==old);
   }
  }
  assert(button(0x6f,1,back)&&button(0x6f,0,back)); // column 2 -> column 1, same Shift layer
  uint64_t old=compressor_project_snapshot();turn[9]=1;assert(compressor_ui_control(turn,back));
  assert(compressor_project_snapshot()==old+(UINT64_C(1)<<34));
  old=compressor_project_snapshot();assert(compressor_ui_control(turn,ticket));
  assert(compressor_project_snapshot()==old+(UINT64_C(1)<<10)); // release Shift immediately returns to Ratio
  assert(compressor_project_restore(saved));
  puts("PASS DATA cycles three columns; Shift click reverses within columns; Shift press/release selects alternate/normal parameter without latching a page");
 }
 auto control=[&](unsigned k,unsigned value,uint64_t t){unsigned char e[16]{};e[8]=static_cast<unsigned char>(0x10+k);e[9]=static_cast<unsigned char>(value);assert(compressor_ui_control(e,t));};
 shifted=1;uint64_t shift_ticket=compressor_ui_ticket();assert(shift_ticket!=ticket);shifted=0;
 control(0,127,shift_ticket);compressor_config(&after);
 assert(after.attack==100&&after.threshold==before.threshold&&after.gain==before.gain);
 shifted=1;control(2,127,ticket);compressor_config(&after);assert(after.gain==6&&after.knee==6);
 for(unsigned k=0;k<3;++k){control(k,0,shift_ticket);compressor_config(&after);assert((k==0?after.attack:k==1?after.release:after.knee)==(k==0?1:k==1?100:0));control(k,127,shift_ticket);compressor_config(&after);assert((k==0?after.attack:k==1?after.release:after.knee)==(k==0?100:k==1?4000:12));}
 reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);assert(shift_labels==7&&red_badges==3);
 control(0,64,shift_ticket);control(1,86,shift_ticket);control(2,64,shift_ticket);
 shifted=0;control(2,64,ticket); /* -6 + round(64*48/127)/4 = 0dB. */
 compressor_config(&after);assert(after.gain==0&&after.threshold==before.threshold&&after.ratio==before.ratio);
 *phase="Opto in actual native AudioPlugin callback";
 for(unsigned i=0;i<300;++i)audio_block(i);
 unsigned settled=native_calls;for(unsigned i=300;i<320;++i)audio_block(i);
 assert(settled==native_calls&&compressor_reduction()>5);
 assert(compressor_audio_blocks()>=320&&compressor_input_level()>-12&&compressor_hits()>0);
 reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);assert(type_labels==3&&indicators==6&&white_badges==3);
 assert(lit_meters(base)>0&&displayed_gr>5);
 assert(!bypass_underline);
 assert(button(0x0e,1,ticket)&&button(0x0e,0,ticket));
 /* Paint before the next audio block: stale peak/GR must not light a sleeping
  * cat. The backing telemetry still contains the real ongoing signal. */
 assert(compressor_reduction()>5);
 reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);assert(!lit_meters(base)&&displayed_gr==0);
 assert(bypass_underline);
 for(unsigned i=0;i<80;++i)audio_block(i);
 assert(compressor_reduction()==0);
 *phase="real stereo meter, bypass, silence and mono";
 for(unsigned block=0;block<400;++block){
  for(unsigned i=0;i<256;++i){l[i]=.2f;r[i]=-.05f;}
  process(instance.get(),audio,midi);playhead.frame+=256;
 }
 comp_meter levels;compressor_meter_levels(&levels);
 assert(std::fabs(levels.input[0]-.2f)<1e-5&&std::fabs(levels.input[1]-.05f)<1e-5);
 assert(std::fabs(levels.output[0]-.2f)<1e-5&&std::fabs(levels.output[1]-.05f)<1e-5);
 reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);assert(!lit_meters(base)&&displayed_gr==0);
 assert(button(0x0e,1,ticket)&&button(0x0e,0,ticket));
 reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);assert(lit_meters(base)>0);
 assert(!bypass_underline);
 assert(button(0x0e,1,ticket)&&button(0x0e,0,ticket));
 reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);assert(!lit_meters(base)&&displayed_gr==0);
 assert(bypass_underline);
 puts("PASS OptoCat Bypass uses the red one-pixel underline only while enabled, cleared on wake and restored on repeated sleep");
 puts("PASS production OptoCat paint hides input/output/GR immediately on Bypass with live and stale telemetry, restores meters on wake and preserves actual bypass audio levels");
 *reinterpret_cast<int*>(audio)=1;
 for(unsigned i=0;i<256;++i)l[i]=.2f;
 process(instance.get(),audio,midi);playhead.frame+=256;compressor_meter_levels(&levels);
 assert(levels.input[1]==0&&levels.output[1]==0);
 for(unsigned block=0;block<1000;++block){
  memset(l,0,sizeof(l));process(instance.get(),audio,midi);playhead.frame+=256;
 }
 compressor_meter_levels(&levels);assert(levels.input[0]<1e-7&&levels.output[0]<1e-7);
 *reinterpret_cast<int*>(audio)=2;
 puts("PASS live stereo peak meters: independent L/R, actual bypass output, mono channel clearing and silence decay");
 *phase="Warm B1 through actual master audio and native header";
 compressor_config(&before);assert(!before.warm&&before.bypass);
 compressor_knob(0,127);compressor_knob(1,0);compressor_knob(2,64);
 compressor_shift_knob(1,0); /* Let the preceding optical-memory tail settle. */
 assert(button(0x0e,1,ticket)&&button(0x0e,0,ticket)); /* Wake. */
 auto steady=[&](){for(unsigned block=0;block<256;++block){for(unsigned i=0;i<256;++i)l[i]=r[i]=.5f;process(instance.get(),audio,midi);playhead.frame+=256;}return l[255];};
 assert(std::fabs(steady()-.5f)<2e-5);
 compressor_config(&before);assert(!before.warm);
 assert(button(0x0c,1,ticket));compressor_config(&after);assert(after.warm==1);
 assert(button(0x0c,1,ticket));compressor_config(&after);assert(after.warm==1); /* Paired edge, no double toggle. */
 assert(button(0x0c,0,ticket));
 assert(after.threshold==before.threshold&&after.ratio==before.ratio&&after.gain==before.gain&&after.attack==before.attack&&after.release==before.release&&after.knee==before.knee);
 assert(std::fabs(steady()-.4671875f)<2e-5);
 reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);assert(warm_labels&&warm_underline&&!bypass_underline);
 assert(button(0x0e,1,ticket)&&button(0x0e,0,ticket));assert(std::fabs(steady()-.5f)<2e-5);
 reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);assert(warm_underline&&bypass_underline&&!lit_meters(base));
 assert(button(0x0e,1,ticket)&&button(0x0e,0,ticket));assert(std::fabs(steady()-.4671875f)<2e-5);
 assert(button(0x0c,1,ticket)&&button(0x0c,0,ticket));assert(std::fabs(steady()-.5f)<2e-5);
 reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);assert(!warm_underline);
 puts("PASS Warm B1 edge pairing, native underline, independent K/Shift values, actual master audio colour, dry Bypass, stored Warm on wake and native Duck Color isolation");
 assert(button(0x0d,1,ticket)&&button(0x0d,0,ticket)&&compressor_type()==COMP_DUCK);audio_block(0);assert(native_calls>settled);
 assert(!button(0x0c,1,ticket)); /* Duck keeps its original B1 Color. */
 for(unsigned i=0;i<6;++i)assert(reinterpret_cast<float(*)(void*,int)>(v[27])(instance.get(),i)==native_parameters[i]);
 *phase="shared native Bypass and page opening policy";
 assert(!compressor_ui_owns(2,0x0e));
 auto reopen=[&](){showing=false;compressor_ui_tick();showing=true;compressor_ui_tick();ticket=compressor_ui_ticket();};
 auto native_bypass=[&](){return reinterpret_cast<float(*)(void*,int)>(v[27])(instance.get(),5)>=.5f;};
 assert(compressor_select(COMP_OPTO));reopen();assert(compressor_type()==COMP_OPTO);
 assert(button(0x0e,1,ticket)&&button(0x0e,0,ticket)&&native_bypass());
 reopen();assert(compressor_type()==COMP_OPTO);compressor_config(&after);assert(after.bypass);
 compressor_config(&after);assert(after.bypass&&native_bypass());
 assert(button(0x0e,1,ticket)&&button(0x0e,0,ticket)&&!native_bypass());
 reopen();assert(compressor_type()==COMP_OPTO);
 /* Direct native/project parameter updates must drive both algorithms. */
 reinterpret_cast<void(*)(void*,int,float)>(v[40])(instance.get(),5,1);audio_block(0);
 compressor_config(&after);assert(after.bypass);reopen();assert(compressor_type()==COMP_OPTO);
 assert(button(0x0e,1,ticket)&&button(0x0e,0,ticket)&&!native_bypass());
 puts("PASS shared native Bypass disables both types, Type browsing stays bypassed, reopening retains selected type even while bypassed; native B3 retained");
 *phase="OptoCat knob takeover";
 uint64_t takeover_saved=compressor_project_snapshot();
 compressor_project_reset();
 assert(compressor_select(COMP_OPTO));
 compressor_config(&before);
 /* The production handler reads the live Parameter Takeover property,
  * independently of the adjacent Knob FX setting. */
 alignas(8) unsigned char app[0x100]{},audio_owner[0x150]{},core[0x5a8]{},properties[0x60]{};
 *reinterpret_cast<uintptr_t*>(app)=base+0x6e07288;
 *reinterpret_cast<uintptr_t*>(audio_owner)=base+0x6e0c430;
 *reinterpret_cast<uintptr_t*>(core)=base+0x6e0cf38;
 *reinterpret_cast<void**>(app+0xd8)=audio_owner;
 *reinterpret_cast<void**>(audio_owner+0x148)=core;
 *reinterpret_cast<void**>(core+0x5a0)=properties;
 auto &app_slot=*reinterpret_cast<void**>(base+0x71e3dc8);
 void *old_app=app_slot;
 app_slot=app;
 auto &mode=*reinterpret_cast<unsigned*>(properties+0x1c);
 *reinterpret_cast<unsigned*>(properties+0x3c)=2;
 mode=1;
 uint64_t initial=compressor_project_snapshot();
 control(0,24,ticket);assert(compressor_project_snapshot()==initial);
 control(0,25,ticket);compressor_config(&after);
 assert(after.threshold>=before.threshold&&after.threshold-before.threshold<=1);
 control(0,127,ticket);compressor_config(&after);assert(after.threshold==0);
 control(0,0,ticket);compressor_config(&after);assert(after.threshold==-48);
 assert(compressor_project_restore(initial));
 mode=0;
 control(2,20,ticket);control(2,40,ticket);
 assert(compressor_project_snapshot()==initial);
 control(2,80,ticket);compressor_config(&after);assert(after.gain>0);
 mode=2;
 control(2,0,ticket);compressor_config(&after);assert(after.gain==-6);
 assert(compressor_project_restore(initial));
 app_slot=old_app;
 puts("PASS OptoCat follows Parameter Takeover for Scaled, Pickup and Jump");
 assert(compressor_project_restore(takeover_saved));
 *phase="master replacement, delayed old destruction and repeated New Project";
 assert(compressor_select(COMP_OPTO));compressor_warm();compressor_config(&before);assert(before.warm);
 auto immutable=*reinterpret_cast<uintptr_t**>(instance.get());
 uintptr_t saved[125];memcpy(saved,immutable-2,sizeof(saved));
 for(unsigned cycle=0;cycle<4;++cycle){
  if(cycle&1){instance.reset();assert(!compressor_ready()&&compressor_type()==COMP_OPTO);}
  NativePtr replacement;Callback next=[&](NativePtr p,const NativeText&){replacement=std::move(p);};
  reinterpret_cast<void(*)(void*,const void*,double,int,Callback)>(base+0x2531110)(format,description,48000.,256,std::move(next));assert(replacement);
  auto nv=*reinterpret_cast<uintptr_t**>(replacement.get());
  reinterpret_cast<void(*)(void*,double,int)>(nv[4])(replacement.get(),48000.,256);
  *reinterpret_cast<void**>(reinterpret_cast<char*>(replacement.get())+16)=&playhead;
  *reinterpret_cast<void**>(meter+0x80)=replacement.get();
  compressor_ui_tick();assert(compressor_ready()&&compressor_type()==COMP_OPTO);
  assert(*reinterpret_cast<uintptr_t**>(replacement.get())==immutable);
  assert(!memcmp(saved,immutable-2,sizeof(saved))); /* No retained table rewrite. */
  if(instance)audio_block(0); /* Retired still-live master must remain a native pass-through. */
  assert(compressor_ready()&&compressor_type()==COMP_OPTO);
  instance.reset(); /* Late old destructor must not retire the new owner. */
  assert(compressor_ready()&&compressor_type()==COMP_OPTO);
  instance=std::move(replacement);process=reinterpret_cast<void(*)(void*,void*,void*)>(immutable[7]);
  for(unsigned i=0;i<10;++i)audio_block(i);
  compressor_config(&after);assert(after.warm&&after.attack==before.attack&&after.release==before.release&&!after.bypass);
 }
 if(compressor_ui_ticket()&(UINT64_C(1)<<62))assert(!"Repeat state must be released before header check");
 repeat_active=1;bool has_repeat_guard=!!(compressor_ui_ticket()&(UINT64_C(1)<<62));repeat_active=0;
 if(has_repeat_guard){
  compressor_select(COMP_DUCK);reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);unsigned labels=type_labels;
  repeat_active=1;compressor_ui_tick();reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);assert(type_labels==labels);
  unsigned before_restore=repaints;repeat_active=0;compressor_ui_tick();assert(repaints==before_restore+1);unsigned restored_repaints=repaints;
  for(unsigned frame=0;frame<32;++frame){reinterpret_cast<void(*)(void*,void*)>(ui[27])(screen,nullptr);assert(type_labels==++labels&&compressor_type()==COMP_DUCK);}
  assert(repaints==restored_repaints);compressor_select(COMP_OPTO);
  puts("PASS Repeat release restores Type on every frame, requests one restore repaint and has no repeating header invalidation loop");
 }
 instance.reset();*reinterpret_cast<void**>(meter+0x80)=nullptr;
 assert(!compressor_ready()&&compressor_type()==COMP_OPTO);
 puts("PASS real Colour master replacement across four project lifetimes, immutable table, old callbacks and late destruction preserve new master and Opto/Warm settings");
 compressor_select(COMP_DUCK);
 showing=false;compressor_ui_tick();assert(!compressor_ui_active());assert(button(0x0d,1,ticket)&&compressor_type()==COMP_DUCK);assert(!compressor_ui_owns(2,0x0d));
 assert(repaints);puts("PASS native master binding, Type caption/two indicators, B2 edge pairing, actual Duck/Opto audio dispatch, no serial compression, Opto bypass, native parameter preservation, stale press rejection");return 0;
}
