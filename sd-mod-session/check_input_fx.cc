/* Common native Mic/USB/Rear wrapper + actual recording writer. Native DSP
 * backend, archive and native renderer are fixtures; no hardware is claimed. */
#define MPC_MIXER_LIVE 1
#include "src/input_fx.cc"
#include "src/rear_native.cc"
#include <cassert>
#include <chrono>
#include <thread>
static mixer_project test_model;
static unsigned test_mode=1,source_id=0,created=0,destroyed=0,clock_calls=0;
static uint64_t test_generation=1;
static thread_local int on_audio;
extern "C" void*__real_malloc(size_t);
extern "C" void*__real_calloc(size_t,size_t);
extern "C" void*__real_realloc(void*,size_t);
extern "C" void __real_free(void*);
extern "C" void*__wrap_malloc(size_t n){assert(!on_audio);return __real_malloc(n);}
extern "C" void*__wrap_calloc(size_t n,size_t s){assert(!on_audio);return __real_calloc(n,s);}
extern "C" void*__wrap_realloc(void*p,size_t n){assert(!on_audio);return __real_realloc(p,n);}
extern "C" void __wrap_free(void*p){assert(!on_audio);__real_free(p);}
extern "C" unsigned music_fx_mode_active(void){return test_mode;}
/* rear_native_source's real function is kept; source_id controls its Session. */
extern "C" int mixer_archive_snapshot(mixer_project*out,uint64_t*revision,unsigned*mode){*out=test_model;*revision=1;*mode=MIXER_ARCHIVE_KNOWN;return 1;}
extern "C" uint64_t mixer_archive_generation(void){return test_generation;}
extern "C" int mixer_archive_loading(void){return 0;}
extern "C" int mixer_live_native_binding(mixer_native_binding*b){std::memset(b,0,sizeof(*b));b->clock_valid=1;double tempo=137;unsigned four=4;std::memcpy(b->position,&tempo,8);std::memcpy(b->position+8,&four,4);std::memcpy(b->position+12,&four,4);return 1;}
extern "C" int mixer_live_native_input_position(void*out){++clock_calls;std::memset(out,0,88);double tempo=137;std::memcpy(out,&tempo,8);return 1;}
extern "C" int fx_native_application_formats(uintptr_t base,fx_native_context*c){*c={};c->base=base;return 1;}
struct Gain {float gain;fx_native_context*context;};
static void*create_gain(void*u,uint32_t,double rate,const float*p){assert(!on_audio&&rate==44100);++created;return new Gain{p[0],static_cast<fx_native_context*>(u)};}
static void destroy_gain(void*,void*p){assert(!on_audio);++destroyed;delete static_cast<Gain*>(p);}
static void process_gain(void*,void*p,float*data,size_t n){assert(on_audio);auto*g=static_cast<Gain*>(p);unsigned char pos[88];uint64_t block;assert(g->context->position(nullptr,pos,&block)&&block);for(size_t i=0;i<n*2;++i)data[i]*=g->gain;}
extern "C" void fx_native_backend(fx_native_context*c,fx_backend*b){*b={c,create_gain,destroy_gain,process_gain};}
static unsigned char native_buffer[0x130];
static float l[512],r[512],other[512],*channels[]={l,r,other};
static float received[2][512];
static void buffer_setup(unsigned count=2,unsigned n=512){
 std::memset(native_buffer,0,sizeof native_buffer);FIELD(native_buffer,0,int)=count;FIELD(native_buffer,4,int)=n;FIELD(native_buffer,16,float**)=channels;
 for(unsigned i=0;i<512;++i){l[i]=.25f;r[i]=-.125f;other[i]=.75f;}
}
static void*original_input(void*,void*,void*b){int n=FIELD(b,4,int),count=FIELD(b,0,int);float**v=FIELD(b,16,float**);for(int i=0;i<n;++i){received[0][i]=v[0][i];received[1][i]=count>1?v[1][i]:v[0][i];v[0][i]=.9f;if(count>1)v[1][i]=.9f;}return b;}
static void render(unsigned count=2,unsigned n=512){buffer_setup(count,n);on_audio=1;assert(rear_native_process(nullptr,nullptr,native_buffer,original_input)==native_buffer);on_audio=0;}
static void settle(){for(unsigned i=0;i<5;++i){input_fx_ui_tick(1);render();}input_fx_ui_tick(1);}
static void wait_record(unsigned want){for(unsigned i=0;i<1000&&rear_record_state()!=want;++i)std::this_thread::sleep_for(std::chrono::milliseconds(2));assert(rear_record_state()==want);}
int main(){
 mixer_defaults(&test_model);rear_settings_reset();Session test_session;session.store(&test_session);test_session.source.store(0);
 /* Initial common callback discovers the actual input format. */
 render();test_model.input.fx.count=1;auto&s=test_model.input.fx.stage[0];s.kind=FX_STAGE_NATIVE;s.native_id=0x12345678;s.parameter[0]=.5f;
 settle();assert(created==1&&clock_calls>0);
 for(unsigned input=0;input<3;++input){source_id=input;test_session.source.store((int)input);input_active.store(input==1);settle();render(input==0?1:2);assert(std::fabs(received[0][511]-.125f)<1e-7f);if(input)assert(std::fabs(received[1][511]+.0625f)<1e-7f);}
 mixer_fx_monitor meter;input_fx_monitor(0,&meter);render();assert(input_fx_monitor(0,&meter)&&meter.channel==9&&meter.input_peak==.25f&&meter.output_peak==.125f);input_fx_monitor(~0u,nullptr);
 render(3);assert(other[511]==.75f);
 buffer_setup(3);FIELD(native_buffer,0x128,unsigned char)=1;on_audio=1;assert(input_fx_audio(native_buffer,44100));on_audio=0;assert(other[511]==0&&l[511]==0&&r[511]==0); /* unrelated physical channels are intact */
 test_model.input.fx_bypass=1;settle();assert(std::fabs(received[0][511]-.25f)<1e-7f);
 test_model.input.fx_bypass=0;settle();assert(std::fabs(received[0][511]-.125f)<1e-7f);
 unsigned before_flush=created;input_fx_flush();settle();assert(created>before_flush);
 test_mode=0;render();assert(received[0][511]==.25f&&received[1][511]==-.125f);test_mode=1;
 /* Mode selection is irrelevant to the common hook; Rear enables only its
  * extra monitor/record capture. Native record/monitor already saw the FX. */
 input_active.store(1);monitor_active.store(0);
 rear_set_setting(0,REAR_MODE,REAR_MIX);rear_set_setting(0,REAR_PAN,100);rear_set_setting(1,REAR_PAN,0);rear_set_setting(0,REAR_LEVEL,50);rear_set_setting(1,REAR_LEVEL,25);
 char directory[]="/tmp/am-input-fx-XXXXXX";assert(mkdtemp(directory));assert(rear_record_begin(directory,3,44100,0,101));wait_record(REAR_WAITING);
 render(2,101);wait_record(REAR_READY);
 assert(std::fabs(received[0][100]+.015625f)<1e-7f&&std::fabs(received[1][100]-.0625f)<1e-7f);
 FILE*f=std::fopen(rear_record_file(0),"rb");assert(f);unsigned char h[44];assert(std::fread(h,1,44,f)==44&&h[22]==2);
 for(unsigned n=0;n<101;++n)for(unsigned ch=0;ch<2;++ch){unsigned char b[3];assert(std::fread(b,1,3,f)==3);unsigned u=b[0]|unsigned(b[1])<<8|unsigned(b[2])<<16;int pcm=(u&0x800000)?int(u)-0x1000000:int(u);assert(std::fabs(float(pcm)/8388607.f-(ch?.0625f:-.015625f))<2e-7f);}
 std::fclose(f);
 rear_record_release(0);assert(!rmdir(directory));assert(selected_channel(REAR_MIX,0)&&!selected_channel(REAR_MIX,1));
 active.store(0);fx_exchange_destroy(exchange.exchange(nullptr));assert(created==destroyed);
 puts("PASS independent Input FX at common Mic/USB/Rear callback, mono/stereo/additional pairs, live clock, bypass, Stock bypass and Rear Pan/Level -> FX -> one stereo WAV; main playback never enters recording; no RT allocation/destruction");
}
