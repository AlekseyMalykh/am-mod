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
#include "fx_layout.inc"
static const struct fx_layout*layout_for(const struct fx_catalog_entry*e)
{
    if(e)for(unsigned i=0;i<sizeof(layouts)/sizeof(layouts[0]);++i)if(layouts[i].id==e->id)return layouts+i;
    return NULL;
}
unsigned fx_catalog_page_count(const struct fx_catalog_entry*e)
{
    const struct fx_layout*l=layout_for(e);
    if(l)return l->slots/6;
    unsigned n=fx_catalog_parameter_count(e);return n?(n+5)/6:1;
}
const char*fx_catalog_page_name(const struct fx_catalog_entry*e,unsigned page)
{
    const struct fx_layout*l=layout_for(e);
    return l&&page<l->slots/6?l->page[page]:"Parameters";
}
int fx_catalog_parameter_index(const struct fx_catalog_entry*e,unsigned position)
{
    const struct fx_layout*l=layout_for(e);
    if(l)return position<l->slots?l->index[position]:-1;
    if(e)for(unsigned i=0;i<e->count;++i)
        if(e->parameter[i]&&*e->parameter[i]&&position--==0)return (int)i;
    return -1;
}
const char*fx_catalog_parameter_label(const struct fx_catalog_entry*e,unsigned index)
{
    if(e&&e->id==0x61464153u&&index==FX_CONTROL_AMP_MODEL)return "Amp Model";
    if(!e||index>=e->count||!e->parameter[index])return "";
    const char*label=e->parameter[index];
    static const char*const short_names[][2]={
        {"Freq","Frequency"},{"Res","Resonance"},{"Thresh","Threshold"},
        {"Rels","Release"},{"DryWet","Mix"},{"Feedbk","Feedback"},
        {"WowFlut","Wow/Flutter"},{"Head1","Head 1"},{"Head2","Head 2"},
        {"Head3","Head 3"},{"Head4","Head 4"},{"Cutoff Frequency","Cutoff"},
        {"Saturation Drive","Sat Drive"},{"Saturation Type","Sat Type"},
        {"Denoise Threshold","Denoise Thresh"},{"True Peak Lim","True Peak"},
        {"Attack Time","Attack"},{"Hold Time","Hold"},{"Release Time","Release"},
        {"Trigger Offset","Offset"},{"Mode","Channels"}
    };
    for(unsigned i=0;i<sizeof(short_names)/sizeof(short_names[0]);++i)
        if(!strcmp(label,short_names[i][0]))return short_names[i][1];
    return label;
}
unsigned fx_catalog_choice_count(const struct fx_catalog_entry*e,unsigned index){
    if(e&&e->id==FX_OWN_HS_HEAT)return index==0||index==3?8u:index==14?7u:0u;
    return e&&e->id==0x61464153u?(index==FX_CONTROL_AMP_MODEL?fx_amp_model_count():index==9?11u:index==10?2u:0u):0u;
}
const char *fx_catalog_choice_name(const struct fx_catalog_entry*e,unsigned index,float v)
{
    /* Labels and round-to-nearest spacing verified on the actual pinned AIR instance. */
    static const char*const cab[11]={"D.I.","Brit","1x8\"","1x12\"","2x10\"","2x12\"","4x10\"","4x12\"","1x15 Bass","4x10 Bass","Radio"};
    unsigned n=fx_catalog_choice_count(e,index);
    if(!n)return "";
    v=fminf(1.f,fmaxf(0.f,v));
    if(e->id==FX_OWN_HS_HEAT){
        static const char*const circuit[8]={"Clean Boost","Saturation","Enhancement","Mid Drive","Rough Crunch","Classic Dist","Round Fuzz","High Gain"};
        static const char*const ftype[8]={"Off","LP 12 dB","LP 6 dB","Band Pass","HP 6 dB","HP 12 dB","Notch","Peak"};
        static const char*const wave[7]={"Triangle","Sine","Square","Saw","Exp","Ramp","Random"};
        unsigned k=(unsigned)(v*(float)(n-1)+.5f);
        return index==0?circuit[k]:index==3?ftype[k]:wave[k];
    }
    if(index==FX_CONTROL_AMP_MODEL)return fx_amp_model_name((unsigned)lroundf(v*(n-1)));
    if(index==10)return v<.5f?"Mono":"Stereo";
    return cab[(unsigned)(v*(float)(n-1)+.5f)];
}
#include "fx_amp_models.inc"

int fx_catalog_shift_layer(const struct fx_catalog_entry*e,unsigned page)
{
    for(unsigned k=0;k<3;++k)if(fx_catalog_parameter_index(e,page*6+3+k)>=0)return 1;
    return 0;
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
 {FX_OWN_HS_SATURATOR,"HS Saturator","Drive",3,{"Drive","Tone","Mix"},{.25f,.5f,1.f}},
 {FX_OWN_HS_BASS,"HS Bass Focus","Dynamics",3,{"Crossover","Bass","Mono Bass"},{.5f,.75f,1.f}},
 {FX_OWN_HS_HEAT,"HS Heat","Drive",18,{"Circuit","Drive","Wet","Filter Type","Filter Freq","Resonance","Dirt","EQ Low","EQ High","Env > Freq","Env Attack","Env Release","LFO > Freq","LFO Speed","LFO Wave","Freq Pan","Mix","Volume"},{.1429f,.25f,.8f,0.f,.75f,0.f,0.f,.5039f,.5039f,.502f,.25f,.5f,.502f,.5f,0.f,.5039f,1.f,.6667f}},
};
unsigned fx_mixer_catalog_count(void){return fx_catalog_count()+1+FX_OWN_COUNT;}
static unsigned category_list(const char**out,unsigned max)
{
    unsigned n=0;
    for(unsigned i=0;i<fx_mixer_catalog_count();++i){
        const char*c=fx_mixer_catalog_at(i)->category;unsigned seen=0;
        for(unsigned k=0;k<n;++k)if(!strcmp(out[k],c))seen=1;
        if(!seen&&n<max)out[n++]=c;
    }
    return n;
}
/* Author tab: the last filter, "HS" = every Henry Skillz effect (they also stay in their semantic category). */
static int is_hs(unsigned i){const char*n=fx_mixer_catalog_at(i)->name;return n[0]=='H'&&n[1]=='S'&&n[2]==' ';}
unsigned fx_mixer_category_count(void){const char*c[16];return 2+category_list(c,15);}
const char *fx_mixer_category_name(unsigned filter){const char*c[16];unsigned n=category_list(c,15);return filter==0||filter>n+1?"All":filter==n+1?"HS":c[filter-1];}
static char search_text[32];
void fx_mixer_search_set(const char*t){size_t n=0;search_text[0]=0;if(!t)return;while(t[n]&&n<sizeof(search_text)-1){search_text[n]=t[n];++n;}search_text[n]=0;}
const char*fx_mixer_search_get(void){return search_text;}
static int ci_find(const char*h,const char*n)
{
    for(;*h;++h){const char*a=h,*b=n;
        while(*a&&*b&&((*a|32)==(*b|32)))++a,++b;
        if(!*b)return 1;}
    return 0;
}
static int in_filter(unsigned filter,unsigned i)
{
    if(search_text[0]&&!ci_find(fx_mixer_catalog_at(i)->name,search_text))return 0;
    if(!filter)return 1;
    const char*name=fx_mixer_category_name(filter);
    return name[0]=='H'&&name[1]=='S'&&!name[2]?is_hs(i):!strcmp(fx_mixer_catalog_at(i)->category,name);
}
unsigned fx_mixer_filtered_count(unsigned filter)
{
    unsigned n=0;
    for(unsigned i=0;i<fx_mixer_catalog_count();++i)if(in_filter(filter,i))++n;
    return n;
}
unsigned fx_mixer_filtered_index(unsigned filter,unsigned position)
{
    for(unsigned i=0;i<fx_mixer_catalog_count();++i)if(in_filter(filter,i)&&position--==0)return i;
    return 0;
}
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
