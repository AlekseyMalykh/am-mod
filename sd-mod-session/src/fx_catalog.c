#include "fx_catalog.h"
#include "fx_own.h"
#include <math.h>
#include <string.h>
/* Defaults/complete labels read from actual 1.3.0.12 AIR/MPC instances.
 * IDs and display names cross-checked with AC50KnobFxInserts15581d0. */
static const struct fx_catalog_entry entries[]={
 {0,"Graphic EQ","EQ",0,{0},{0}},
 {0x6146446c,"Delay","Delay",10,{"Time","Sync","L/R Ratio","Feedback","Fdbk Damp","Fdbk Reso","Fdbk Reso Freq","HPF","Width","Mix"},{0.44f,1.0f,0.5f,0.4f,1.0f,0.0f,0.5f,0.0f,1.0f,0.5f}},
 {0x61464444,"Diff Delay","Delay",9,{"Mix","Delay","Feedback","High Damp","Diffusion","Sync","Low Cut","Pan","Width"},{0.4f,0.4f,0.5f,0.35f,0.4f,1.0f,0.0f,0.5f,1.0f}},
 {0x41435658,"Tape Delay","Delay",16,{"Ramp","Head4","","","Feedbk","Head3","WowFlut","","Time","Head2","Spread","","DryWet","Head1","Tone",""},{0.5f,0.0f,0.0f,0.0f,0.5f,0.0f,0.5f,0.0f,0.375f,0.0f,0.5f,0.0f,0.5f,1.0f,0.5f,0.0f}},
 {0x534d444c,"Sample Delay","Delay",4,{"Left Delay","Right Delay","Link L/R","Units"},{0.0f,0.0f,0.0f,1.0f}},
 {0x61465276,"Reverb","Reverb",18,{"Low Cut","Pre-Delay","ER/Tail Mix","Room Size","Time","Time High","Freq High","High Cut","Mix","Input Width","ER Type","ER Length","Reverb Delay","Density","Ambience","Time Low","Freq Low","Output Width"},{0.0f,0.0f,0.5f,1.0f,0.5f,0.5f,0.5f,0.75f,0.5f,0.5f,0.0f,1.0f,0.0f,1.0f,0.0f,0.5f,0.5f,0.5f}},
 {0x61465370,"Spring Reverb","Reverb",6,{"Pre Delay","Time","Low Cut","Diffusion","Width","Mix"},{0.1f,0.6f,0.5f,1.0f,0.0f,0.5f}},
 {0x41435663,"HP Filter","Filter",16,{"","","","","","","","","Res","","","","Freq","","",""},{0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.659218f,0.0f,0.0f,0.0f}},
 {0x41435662,"LP Filter","Filter",16,{"","","","","","","","","Res","","","","Freq","","",""},{0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.619776f,0.0f,0.0f,0.0f}},
 {0x61465346,"BP Filter","Filter",7,{"Filter Mode","Filter Type","Saturation Type","Saturation Drive","Cutoff Frequency","Resonance","Output Gain"},{0.0f,0.0f,1.0f,0.0f,1.0f,0.1f,1.0f}},
 {0x4143564a,"Bus Compressor","Dynamics",16,{"Thresh","","","","Rels","Output","","","Attack","Oldskool","","","DryWet","Ratio","",""},{1.0f,0.0f,0.0f,0.0f,0.5f,0.2f,0.0f,0.0f,0.5f,0.0f,0.0f,0.0f,1.0f,0.0f,0.0f,0.0f}},
 {0x61464c6d,"Limiter","Dynamics",5,{"Gain","Ceiling","LookAhead","Release","LF Mono"},{0.25f,1.0f,0.0f,0.5f,0.0f}},
 {0x61465075,"Pumper","Dynamics",7,{"Depth","Attack Time","Hold Time","Release Time","Release Shape","Trigger Offset","Speed"},{0.8f,0.05f,0.1f,0.6f,0.1f,0.5f,0.3f}},
 {0x61465472,"Transient","Dynamics",5,{"Attack","Attack Shape","Sustain","Output","Limit"},{0.5f,0.5f,0.5f,0.5f,1.0f}},
 {0x61464e47,"Noise Gate","Dynamics",7,{"Threshold","Attack","Hold","Release","Depth","Denoise Filter","Denoise Threshold"},{0.6f,0.25f,0.25f,0.25f,1.0f,0.0f,0.5f}},
 {0x61464153,"Amp Sim","Drive",11,{"Drive","Soft Clip","Top Boost","Bias","Bass","Mid","Mid Freq","Treble","Output","Cab Model","Mode"},{0.0f,0.0f,0.0f,0.0f,0.5f,0.5f,0.5f,0.5f,0.5f,0.1f,1.0f}},
 {0x61465444,"Tube Drive","Drive",4,{"Drive","Headroom","Saturation","Output"},{0.0f,0.5f,0.5f,0.5f}},
 {0x6153436c,"Soft Clipper","Drive",10,{"Drive","Shape","Post Level","Mix","Out Level","Anti-Aliasing","Stereo Link","Release Mode","Release Time","True Peak Lim"},{0.5f,0.0f,1.0f,1.0f,0.5f,0.0f,0.0f,0.0f,0.35f,0.0f}},
 {0x6146456e,"Ensemble","Modulation",6,{"Rate","Depth","Delay","Shimmer","Width","Mix"},{0.5f,0.5f,0.0f,0.5f,1.0f,0.75f}},
 {0x61464d43,"Multi Chorus","Modulation",8,{"Rate","Depth","Delay","Width","Low Cut","Voices","LFO Wave","Mix"},{0.5f,0.5f,0.5f,1.0f,0.0f,0.5f,1.0f,0.5f}},
 {0x61465073,"Phaser","Modulation",7,{"Rate","Depth","Feedback","Offset","Offset Type","Model","Mix"},{0.5f,0.5f,0.0f,0.5f,0.0f,0.67f,0.5f}},
 {0x6146466c,"Flanger","Modulation",5,{"Rate","Depth","Feedback","Headroom","Mix"},{0.5f,0.5f,0.5f,0.5f,0.5f}},
 {0x41435679,"Auto Wah","Modulation",16,{"Rels","","","","Attack","","","","Res","Sens","","","DryWet","Center","",""},{0.3f,0.0f,0.0f,0.0f,0.3f,0.0f,0.0f,0.0f,0.75f,0.5f,0.0f,0.0f,1.0f,0.5f,0.0f,0.0f}},
 {0x41435671,"Auto Pan","Modulation",16,{"","","","","","","","","Rate","","","","DryWet","","",""},{0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.0f,0.1f,0.0f,0.0f,0.0f,1.0f,0.0f,0.0f,0.0f}},
 {0x56456678,"Vinyl","Emulator",3,{"Tone","Crackle","Pitch"},{0.5f,0.0f,0.0f}},
 {0x54456678,"Tape","Emulator",3,{"Wow","Noise","Pitch"},{0.0f,0.0f,0.0f}},
};
unsigned fx_catalog_parameter_count(const struct fx_catalog_entry*e)
{
    unsigned n=0;
    if(e)for(unsigned i=0;i<e->count;++i)if(e->parameter[i]&&*e->parameter[i])++n;
    return n;
}
int fx_catalog_parameter_index(const struct fx_catalog_entry*e,unsigned position)
{
    if(e)for(unsigned i=0;i<e->count;++i)
        if(e->parameter[i]&&*e->parameter[i]&&position--==0)return (int)i;
    return -1;
}
static const struct fx_catalog_entry sidechain={FX_SIDECHAIN_ID,"Sidechain Compressor","Dynamics",6,
    {"Key Track","Threshold","Ratio","Attack","Release","Mix"},{0,.60f,.158f,.25f,.5f,1}};
/* Am MOD's own effects (fx_own.c). Offered by the Mixer only: they are not firmware plug-ins. */
static const struct fx_catalog_entry own_entries[FX_OWN_COUNT]={
 {FX_OWN_SHIMMER,"Shimmer Reverb","Reverb",7,{"Mix","Decay","Shimmer","Pitch","Size","Damp","Pre-Delay"},{.35f,.6f,.5f,.8f,.5f,.5f,.1f}},
 {FX_OWN_PITCH,"Pitch Shifter","Pitch",3,{"Interval","Detune","Mix"},{.75f,.5f,.5f}},
 {FX_OWN_REVERSE,"Reverse Delay","Delay",4,{"Time","Feedback","Mix","Tone"},{.4f,.3f,.5f,.7f}},
 {FX_OWN_TREMOLO,"Tremolo","Modulation",4,{"Rate","Depth","Shape","Stereo"},{.35f,.6f,0.f,.5f}},
 {FX_OWN_SWELL,"Slow Attack","Dynamics",3,{"Attack","Sensitivity","Mix"},{.5f,.5f,1.f}},
 {FX_OWN_RING,"Ring Modulator","Modulation",2,{"Frequency","Mix"},{.4f,.5f}},
 {FX_OWN_CRUSH,"Bit Crusher","Drive",3,{"Bits","Rate","Mix"},{.6f,.2f,1.f}},
 {FX_OWN_OPTO,"OptoCat Comp","Dynamics",7,{"Threshold","Ratio","Gain","Attack","Release","Knee","Warm"},{.625f,.2857f,.5f,.5f,.6738f,.5f,0.f}},
};
unsigned fx_mixer_catalog_count(void){return fx_catalog_count()+1+FX_OWN_COUNT;}
const struct fx_catalog_entry *fx_mixer_catalog_at(unsigned i)
{return i==fx_catalog_count()?&sidechain:i>fx_catalog_count()&&i<fx_mixer_catalog_count()?own_entries+(i-fx_catalog_count()-1):fx_catalog_at(i);}
int fx_mixer_catalog_stage(unsigned i,double rate,struct fx_stage*out)
{
    if(i>fx_catalog_count()&&i<fx_mixer_catalog_count()){
        if(!out||rate<8000||rate>192000)return 0;
        const struct fx_catalog_entry*e=own_entries+(i-fx_catalog_count()-1);
        memset(out,0,sizeof(*out));out->kind=FX_STAGE_NATIVE;out->native_id=e->id;
        memcpy(out->parameter,e->defaults,sizeof(e->defaults));return 1;
    }
    if(i!=fx_catalog_count())return fx_catalog_stage(i,rate,out);
    if(!out||rate<8000||rate>192000)return 0;
    memset(out,0,sizeof(*out));out->kind=FX_STAGE_SIDECHAIN;out->native_id=FX_SIDECHAIN_ID;
    memcpy(out->parameter,sidechain.defaults,sizeof(out->parameter));return 1;
}
unsigned fx_catalog_count(void){return sizeof(entries)/sizeof(entries[0]);}
const struct fx_catalog_entry*fx_catalog_at(unsigned i){return i<fx_catalog_count()?entries+i:0;}
const struct fx_catalog_entry*fx_catalog_find(uint32_t id){if(id==FX_SIDECHAIN_ID)return &sidechain;if(fx_own_is(id))return own_entries+(id-FX_OWN_SHIMMER);for(unsigned i=0;i<fx_catalog_count();++i)if(entries[i].id==id)return entries+i;return 0;}
const char*fx_catalog_stage_name(const struct fx_stage*s){const struct fx_catalog_entry*e=s?fx_catalog_find(s->kind==FX_STAGE_EQ?0:s->native_id):0;return e?e->name:"Unknown effect";}
int fx_catalog_stage(unsigned i,double rate,struct fx_stage*out){const struct fx_catalog_entry*e=fx_catalog_at(i);if(!e||!out||rate<8000||rate>192000)return 0;
 memset(out,0,sizeof(*out));if(!e->id){out->kind=FX_STAGE_EQ;fx_eq_default(&out->eq);for(unsigned b=0;b<out->eq.count;++b)out->eq.band[b].hz=fmin(out->eq.band[b].hz,rate*.45);}
 else{out->kind=FX_STAGE_NATIVE;out->native_id=e->id;memcpy(out->parameter,e->defaults,sizeof(out->parameter));}return 1;}
