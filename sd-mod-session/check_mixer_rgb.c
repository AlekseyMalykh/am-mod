/* Production final sender/timer + actual native chain and Playing callbacks.
 * Fixtures supply native app services and a receiver, never physical MIDI. */
#define base elf_image
#define main unused_fx_main
#define load load_elf
#include "check_fx_native.c"
#undef main
#undef load
#undef base
#undef FN
#define PHONE_RGB_TEST 1
#define MPC_MIXER_LIVE 1
#define MPC_RESAMPLE_TRIAL 1
#define MPC_STOCK_RESAMPLE 1
#include "src/phone_native.c"
#include <assert.h>
static _Alignas(16) unsigned char program_data[0x2900],instrument_data[3][0x2400],layer_data[8][0xb30];
static _Alignas(16) unsigned char things_data[0x160],playing[0xd0],chain[0x60],critical_layer[0x40],voice[0x930];
static _Alignas(16) unsigned char slice[0xb60];
static _Alignas(16) unsigned char native_lights[0x50],light_values[128][0xbc],light_sink[0x10];
static uintptr_t light_methods[3],sink_methods[5];
static uintptr_t program_methods[32],critical_methods[4];
static void *instruments_by_pad[128],*layers_by_priority[2],*voices[1];
static unsigned sent_count,sent_per_pad[16],critical_on,preview_on,bank_fixture;
static uint32_t sent[16],native_value;
static int workflow_colours,resample_colours;
static _Atomic unsigned pause_send,send_entered,send_release;
int workflow_light(unsigned pad,uint64_t *out){if(!workflow_colours||pad>=16)return 0;*out=(UINT64_C(0x3f800000)<<32)|0xab12cd;return 1;}
int resample_ui_light(unsigned pad,uint64_t *out){if(!resample_colours||pad>=16)return 0;*out=(UINT64_C(0x3f800000)<<32)|0x12abcd;return 1;}
uint64_t resample_ui_palette_generation(void){return (uint64_t)resample_colours;}
int workflow_native_pad_colours(void){return 0;}
uint64_t workflow_palette_generation(void){return (uint64_t)workflow_colours;}
int step_native_light(unsigned pad,uint64_t *out){(void)pad;(void)out;return 0;}
int step_native_browser_bank(int selected){if(selected>=0)bank_fixture=(unsigned)selected;return (int)bank_fixture;}

int music_ui_pad_settings_visible(void){return 0;}
int music_ui_palette_light(unsigned pad,uint64_t *out){(void)pad;(void)out;return 0;}
int note_native_sample_visible(void){return 1;}
int music_ui_sequence_visible(void){return 0;}
int step_native_sound_layout(void){return 0;}
int step_native_active(void){return 0;}
unsigned step_native_transport_state(void){return 0;} /* r27r: stopped, invalid */
void step_native_observe_rgb(unsigned pad,unsigned rgb){(void)pad;(void)rgb;}
int workflow_native_occupied(void){return -1;}
int workflow_native_selected(void){return -1;}
void *note_native_context(void){return NULL;} /* r27r: no CurrentThings in this fixture */
int music_lighting_levels_state(unsigned *on,unsigned *mode,unsigned *original){*on=*mode=*original=0;return 1;}
static void capture(void *s,uint32_t colour,unsigned pad)
{
    assert(s==(void*)0x1234&&pad<16);++sent_count;++sent_per_pad[pad];sent[pad]=colour;
    if(atomic_exchange(&pause_send,0)){
        atomic_store(&send_entered,1);
        while(!atomic_load(&send_release))sched_yield();
    }
}
static void *overlapping_send(void *p)
{(void)p;send_pad_rgb((void*)0x1234,0x804020,0);return NULL;}
static uint32_t convert(uint64_t packed){return FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&packed);}
static void native_light(void *sink,unsigned sound,uint64_t packed)
{assert(sink==light_sink&&sound<128);native_value=convert(packed);rgb((void*)0x1234,native_value,sound%16);}
static float global_brightness(void *p){(void)p;return 1.f;}
static void *drum_cast(void *p,void *from,void *to,long offset)
{(void)from;(void)to;assert(!offset&&p==program_data);return p;}
static unsigned occupied(void *p,unsigned sound){assert(p==program_data&&sound<128);return instruments_by_pad[sound]!=NULL;}
static int voice_active(void *p){assert(p==voice);return 1;}
static int preview(void *p){assert(p==slice);return (int)preview_on;}
static void preview_colour(void *out,int value){(void)value;FIELD(out,0,unsigned)=0;}
static unsigned preview_rgb(void *p){(void)p;return 0xed3412;}
static uint64_t base_light(void *p,unsigned sound,unsigned loaded,float intensity)
{assert(p==program_data+0x16a8&&sound<128);(void)loaded;(void)intensity;return (UINT64_C(0x3f800000)<<32)|0x456789;}
static int critical_refresh(void *p,unsigned sound)
{assert(p==critical_layer);if(critical_on)FN(0x1a3ffb0,void(*)(void*,unsigned,uint64_t))(native_lights,sound,(UINT64_C(0x3f800000)<<32)|0xfc1234);return (int)critical_on;}
static void stub(uintptr_t offset,void *fn)
{
    void *page=(void*)((base+offset)&~(uintptr_t)4095);
    assert(!mprotect(page,4096,PROT_READ|PROT_WRITE));
    uint32_t code[]={0x58000050,0xd61f0200};memcpy((void*)(base+offset),code,8);memcpy((void*)(base+offset+8),&fn,8);
    __builtin___clear_cache((char*)(base+offset),(char*)(base+offset+16));
    assert(!mprotect(page,4096,PROT_READ|PROT_EXEC));
}
static void native_refresh(unsigned sound)
{assert(FN(0x1535170,int(*)(void*,unsigned))(chain,sound));}
static void native_event(unsigned sound,float intensity)
{assert(FN(0x15398d0,int(*)(void*,unsigned,unsigned,unsigned,float))(playing,sound,0,0,intensity));}
static struct mixer_project accepted;
static int submit(void *u,const struct mixer_project *p,uint64_t revision,uint64_t *next)
{(void)u;accepted=*p;*next=revision+1;return 1;}
static void publish(struct mixer_control *control,unsigned selected_bank)
{
    mixer_control_presence(control,UINT64_C(7)|(UINT64_C(1)<<16),UINT64_C(1)<<63,selected_bank);
    uint32_t rgb[16];for(unsigned i=0;i<16;++i)assert(mixer_control_led(control,i,&rgb[i]));
    mixer_leds_publish(program_data,selected_bank,rgb);
}
int main(int argc,char **argv)
{
    assert(argc==2);setbuf(stdout,NULL);load_elf(argv[1]);base=elf_image;
    struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);sigaction(SIGILL,&sa,NULL);
    phase="Mixer native lighting installer";
    /* r27r appended Pink after the twelve Mixer colours; the shared prefix is unchanged. */
    assert(MIXER_COLOURS+1<=MUSIC_SEQUENCE_COLOURS);
    for(unsigned i=0;i<MIXER_COLOURS;++i){assert(mixer_colour_at(i)==music_sequence_colours[i+1]);assert(!strcmp(mixer_colour_name(mixer_colour_at(i)),music_sequence_colour_names[i+1]));}
    assert(mixer_leds_install(base)&&mixer_leds_install(base));
    assert(!mixer_leds_install(base+4096));
    stub(phone_hooks[H_RGB].trampoline,capture);
    stub(0xb51d78,drum_cast);stub(0x1a39c30,global_brightness);stub(0x2835ee0,base_light);
    stub(0x221b220,voice_active);stub(0x221af00,voice_active);stub(0x29c3fc0,preview);
    stub(0x101bc50,preview_colour);stub(0x1a4cb10,preview_rgb);
    FIELD(program_data,0,void*)=program_methods;program_methods[0xd0/8]=(uintptr_t)occupied;
    FIELD(program_data,0x2808,void*)=instruments_by_pad;FIELD(program_data,0x2810,void*)=instruments_by_pad+128;
    FIELD(program_data,0x2838,void*)=voices;FIELD(program_data,0x2840,void*)=voices;
    for(unsigned i=0;i<3;++i){FIELD(instrument_data[i],0x2310,void*)=program_data;FIELD(instrument_data[i],0x22e8,void*)=layer_data;instruments_by_pad[i]=instrument_data[i];}
    instruments_by_pad[16]=instrument_data[0];instruments_by_pad[127]=instrument_data[2];
    FIELD(things_data,0xb8,void*)=program_data;
    FIELD(playing,0,uintptr_t)=base+0x6e10068;playing[0x2b]=1;
    FIELD(playing,0x38,void*)=native_lights;FIELD(playing,0xc8,void*)=things_data;FIELD(playing,0xc4,float)=-1;
    light_methods[2]=base+0x1a3ff00;FIELD(native_lights,0,void*)=light_methods;
    FIELD(native_lights,0x18,void*)=light_values;FIELD(native_lights,0x20,void*)=light_values+128;
    FIELD(native_lights,0x40,void*)=light_sink;FIELD(light_sink,0,void*)=sink_methods;sink_methods[4]=(uintptr_t)native_light;
    critical_methods[3]=(uintptr_t)critical_refresh;FIELD(critical_layer,0,void*)=critical_methods;
    layers_by_priority[0]=critical_layer;layers_by_priority[1]=playing;
    FIELD(chain,0x40,void*)=layers_by_priority;FIELD(chain,0x48,void*)=layers_by_priority+2;
    music_reset();mixer_defaults(&accepted);accepted.track[0].colour=0x804020;accepted.track[1].colour=0x20c080;
    accepted.output[0]=accepted.output[16]=1;accepted.output[1]=accepted.output[127]=2;
    struct mixer_control_ops ops={NULL,submit};struct mixer_control *control=mixer_control_create(&ops,48000);assert(control);
    assert(mixer_control_confirm(control,&accepted,1,1)&&mixer_control_open(control,1,0));
    uint64_t ticket=mixer_control_ticket(control);
    assert(mixer_control_button(control,MIXER_PAD_FX,1,ticket)&&mixer_control_button(control,MIXER_PAD_FX,0,ticket));
    publish(control,0);
    phase="native chain and background routing";
    for(unsigned i=0;i<16;++i)native_refresh(i);
    assert(sent[0]==0x804020&&sent[1]==0x083020&&sent[2]==0x181818&&sent[3]==0);
    send_step_lights();unsigned previous=sent_count;send_step_lights();assert(sent_count==previous);
    /* Real native voice-list getter blocks routing, regardless of RGB hue. */
    voices[0]=voice;FIELD(voice,0x8f8,void*)=instrument_data[0];FIELD(voice,0x924,unsigned)=0;
    FIELD(program_data,0x2840,void*)=voices+1;native_refresh(0);assert(sent[0]==native_value&&sent[0]!=0x804020);
    send_step_lights();assert(sent[0]==native_value);
    FIELD(program_data,0x2840,void*)=voices;native_event(0,.5f);assert(sent[0]==native_value);
    send_step_lights();assert(sent[0]==native_value);native_event(0,0);assert(sent[0]==0x804020);
    critical_on=1;native_refresh(0);send_step_lights();assert(sent[0]==0xfc1234);
    critical_on=0;native_refresh(0);assert(sent[0]==0x804020);
    preview_on=1;FIELD(things_data,0x140,void*)=slice;native_refresh(0);assert(sent[0]==0xed3412);
    preview_on=0;FIELD(things_data,0x140,void*)=NULL;native_refresh(0);assert(sent[0]==0x804020);
    rgb((void*)0x1234,0x314159,0);send_step_lights();assert(sent[0]==0x314159); /* Unknown native writer wins. */
    native_refresh(0);assert(sent[0]==0x804020);
    /* A native/UI sender can return after a newer RGB was already queued.
     * Such overlap must invalidate the cache, without serializing audio. */
    send_step_lights();atomic_store(&pause_send,1);pthread_t producer;
    assert(!pthread_create(&producer,NULL,overlapping_send,NULL));
    while(!atomic_load(&send_entered))sched_yield();
    send_pad_rgb((void*)0x1234,0x102030,0);atomic_store(&send_release,1);
    assert(!pthread_join(producer,NULL)&&sent[0]==0x102030);
    send_step_lights();assert(sent[0]==0x804020);
    for(unsigned mode=0;mode<3;++mode){
        if(!mode)atomic_store(&native_mute_mode,1);else atomic_store(&native_buttons_down[mode==1?0x2b:0x09],1);
        native_refresh(0);send_step_lights();assert(sent[0]==native_value);
        if(!mode)atomic_store(&native_mute_mode,0);else atomic_store(&native_buttons_down[mode==1?0x2b:0x09],0);
        send_step_lights();assert(sent[0]==0x804020);
    }
    /* Confirmed-only assignment. Pending does not recolour the physical pad. */
    assert(mixer_control_button(control,MIXER_B2,1,ticket)&&mixer_control_button(control,MIXER_B2,0,ticket));
    assert(mixer_control_pad(control,1,1,ticket)&&mixer_control_pad(control,1,0,ticket));mixer_control_tick(control);
    publish(control,0);native_refresh(1);assert(sent[1]==0x083020);
    assert(mixer_control_confirm(control,&accepted,2,1));publish(control,0);send_step_lights();assert(sent[1]==0x804020);
    publish(control,1);send_step_lights();assert(sent[0]==(uint32_t)atomic_load(&original_lights[0]));
    native_refresh(16);assert(sent[0]==0x804020);
    publish(control,7);native_refresh(127);assert(sent[15]==0x083020);
    publish(control,1);native_refresh(16);assert(sent[0]==0x804020);
    mixer_leds_reset();publish(control,1);send_step_lights();assert(sent[0]==native_value);
    native_refresh(16);assert(sent[0]==0x804020);
    workflow_colours=1;native_refresh(16);send_step_lights();assert(sent[0]==0xab12cd);
    resample_colours=1;native_refresh(16);send_step_lights();assert(sent[0]==0x12abcd);
    workflow_colours=resample_colours=0;atomic_store(&led_enabled,1);native_refresh(16);
    assert(sent[0]!=0x804020);atomic_store(&led_enabled,0);send_step_lights();assert(sent[0]==0x804020);
    /* User K3 colour edit is visible on physical output only after confirm. */
    assert(mixer_control_button(control,MIXER_PAD_FX,1,ticket)&&mixer_control_button(control,MIXER_PAD_FX,0,ticket));
    assert(mixer_control_cc(control,0x12,127,ticket));mixer_control_tick(control); /* K3 on the Mixer page */
    assert(mixer_control_button(control,MIXER_PAD_FX,1,ticket)&&mixer_control_button(control,MIXER_PAD_FX,0,ticket));
    publish(control,1);native_refresh(16);assert(sent[0]==0x804020);
    assert(mixer_control_confirm(control,&accepted,3,1));publish(control,1);send_step_lights();assert(sent[0]==mixer_colour_at(11));
    /* r27r: with Mixer released, the AM Mod pad-colour layer owns the Sample
     * grid again (lighting mode AM Mod); Standard mode is plain native. */
    mixer_leds_disable();send_step_lights();assert(sent[0]!=mixer_colour_at(11));
    assert(music_set_pad_lighting_mode(MUSIC_LIGHTING_STANDARD));native_refresh(16);send_step_lights();assert(sent[0]==native_value);
    mixer_control_destroy(control);
    puts("PASS Mixer physical RGB path: actual native chain/Playing refresh/event, real voice membership and converter, confirmed routing colours, other/direct/empty, playback/tail/preview/unknown/Mute/Erase priority, bank identity, epoch reset, modal priorities, delta refresh and native restore; receiver/app-model fixtures, no hardware");
    return 0;
}
