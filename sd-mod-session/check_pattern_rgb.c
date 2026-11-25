/* Actual RGB interception + refresh and real colour settings. Native screen,
 * editor publication and physical receiver are fixtures. */
#define MPC_PATTERN_TOOLS_TRIAL 1
#define MPC_MIXER_LIVE 1
#define main legacy_pad_rgb_main
#include "check_pad_colour_rgb.c"
#undef main
static int pattern_visible;
static int pattern_bank_fixture;
static uint64_t pattern_colours[16];
static unsigned bank_sent[8];
static void capture_bank(void*s,unsigned value,unsigned code){assert(s==(void*)0x2345&&code>=BANK_LED_FIRST&&code<BANK_LED_FIRST+8);bank_sent[code-BANK_LED_FIRST]=value;}
static void*empty_instrument(void*p,int pad){assert(p==&source_program&&pad>=0&&pad<16);return NULL;}
void mixer_leds_native_rgb(unsigned pad){(void)pad;}
int mixer_leds_light(unsigned pad,uint32_t native,uint64_t*out){(void)pad;(void)native;(void)out;return 0;}
void mixer_control_view(struct mixer_control*c,uint64_t now,struct mixer_view_state*out){(void)c;(void)now;memset(out,0,sizeof *out);}
int step_pattern_light(unsigned pad,uint64_t*out){
 if(!pattern_visible||pad>=16)return 0;
 *out=pattern_colours[pad];return 1;
}
int step_pattern_bank(void){return pattern_visible?pattern_bank_fixture:-1;}
int main(void){
 void*p=mmap(NULL,0x7500000,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(p!=MAP_FAILED);base=(uintptr_t)p;
 stub(phone_hooks[H_RGB].trampoline,capture);stub(0x23b00c0,convert);__builtin___clear_cache((char*)p,(char*)p+0x7500000);
 music_reset();sample_visible=0;seq_visible=1;seq_selected=0;
 native_bank_light=capture_bank;
 for(unsigned i=0;i<8;++i)mixer_bank_light((void*)0x2345,i==5,BANK_LED_FIRST+i);
 for(unsigned i=0;i<16;++i)native_rgb(i,0x3000+i);
 send_step_lights();assert(atomic_load(&sequence_palette_enabled));
 pattern_visible=1;atomic_store(&led_enabled,1); /* stale naming must lose */
 for(int b=0;b<8;++b){pattern_bank_fixture=b;mixer_bank_lights_tracks(-1);for(int i=0;i<8;++i)assert(bank_sent[i]==(unsigned)(i==b));}
 mixer_bank_light((void*)0x2345,1,BANK_LED_FIRST+2);assert(!bank_sent[2]&&bank_sent[7]);
 for(unsigned i=0;i<16;++i){pattern_colours[i]=i%2?0xffffff:0;native_rgb(i,0x4000+i);assert(sent[i]==pattern_colours[i]);}
 send_step_lights();for(unsigned i=0;i<16;++i)assert(sent[i]==pattern_colours[i]);
 /* Global Pad Colour dialog retains priority in both send paths. */
 stub(0x286bbe0,empty_instrument);__builtin___clear_cache((char*)p+0x286bbe0,(char*)p+0x286bbf0);
 FIELD(source_things,0xb8,void*)=&source_program;settings_visible=1;send_step_lights();
 for(unsigned i=0;i<16;++i){assert(sent[i]==0);native_rgb(i,0x4500+i);assert(sent[i]==0);}
 settings_visible=0;FIELD(source_things,0xb8,void*)=NULL;send_step_lights();
 for(unsigned i=0;i<16;++i)assert(sent[i]==pattern_colours[i]);
 /* Empty banks still own all pads. Sequence lights remain available
  * underneath and must not leak through either sender. */
 memset(pattern_colours,0,sizeof pattern_colours);send_step_lights();
 for(unsigned i=0;i<16;++i)assert(sent[i]==0);
 for(unsigned i=0;i<16;++i){native_rgb(i,0x5000+i);assert(sent[i]==0);}
 atomic_store(&led_enabled,0);pattern_visible=0;send_step_lights();
 mixer_bank_lights_tracks(-1);assert(bank_sent[2]&&bank_sent[5]&&!bank_sent[7]);
 for(unsigned i=0;i<16;++i){uint64_t expected;assert(sequence_light(i,&expected)&&sent[i]==(uint32_t)expected);}
 seq_visible=0;send_step_lights();for(unsigned i=0;i<16;++i)assert(sent[i]==0x5000+i);
 puts("PASS pattern RGB: production interception/refresh prioritize pattern over sequence and stale naming, empty bank remains dark, closing restores sequence then latest native colours; no GUI access on RGB receiver");
 munmap(p,0x7500000);return 0;
}
