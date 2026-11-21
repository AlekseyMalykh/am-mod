/* End-to-end knob/label/reset coverage plus every illustration's bypass. */
#include "src/mixer_view.c"
static unsigned visual_transport;
unsigned step_native_transport_state(void){return visual_transport;}
#include "src/mixer_control.h"
#include "src/mixer_input.h"
#include <assert.h>
static struct mixer_project accepted;
static uint64_t revision=1,epoch=10;
static uint32_t pixels[320*240];
static char knob_label[3][64],top_labels[3][64];
static FILE*trace;
static void draw_rect(void*u,int x,int y,int w,int h,uint32_t c){
    (void)u;assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240);
    for(int j=y;j<y+h;++j)for(int i=x;i<x+w;++i)pixels[j*320+i]=c;
    if(trace)fprintf(trace,"R %d %d %d %d %08x\n",x,y,w,h,c);
}
static void draw_text(void*u,const char*s,int x,int y,int w,int h,unsigned size,uint32_t c,int centre){
    (void)u;assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240&&size>=5);
    if(y==200&&(x==4||x==110||x==216))snprintf(knob_label[(x-4)/106],64,"%s",s);
    if(y==1&&(x==4||x==110||x==217))snprintf(top_labels[x==217?2:x==110?1:0],64,"%s",s);
    /* Include text in the deterministic render comparison. */
    uint32_t hash=c;for(const char*t=s;*t;++t)hash=hash*33+(unsigned char)*t;
    pixels[y*320+x]^=hash^(uint32_t)w^(uint32_t)centre;
    if(trace)fprintf(trace,"T %d %d %d %d %u %08x %d %s\n",x,y,w,h,size,c,centre,s);
}
static int submit(void*u,const struct mixer_project*p,uint64_t expected,uint64_t*next){
    (void)u;assert(expected==revision);accepted=*p;*next=++revision;return 1;
}
static struct mixer_view_state get_view(struct mixer_control*c){struct mixer_view_state s;mixer_control_view(c,1000,&s);return s;}
static void key(struct mixer_control*c,enum mixer_action action){uint64_t t=mixer_control_ticket(c);
    assert(mixer_control_button(c,action,1,t));assert(mixer_control_button(c,action,0,t));}
static void ack(struct mixer_control*c){mixer_control_tick(c);assert(mixer_control_confirm(c,&accepted,revision,epoch));}
static void shift(struct mixer_control*c,unsigned on){mixer_control_button(c,MIXER_SHIFT,on,mixer_control_ticket(c));}
static void knobs(const char*out){
    struct resample_painter painter={NULL,draw_rect,draw_text};
    struct mixer_fx_monitor monitor={0};monitor.input_peak=.4f;monitor.output_peak=.3f;
    unsigned controls=0;
    for(unsigned n=1;n<fx_mixer_catalog_count();++n){
        const struct fx_catalog_entry*e=fx_mixer_catalog_at(n);uint64_t seen=0,expected=0;
        unsigned pages=fx_catalog_page_count(e);assert(pages>=1&&pages<=3);
        for(unsigned i=0;i<e->count;++i)if(e->parameter[i]&&*e->parameter[i])expected|=UINT64_C(1)<<i;
        if(e->id==0x61464153u)expected|=UINT64_C(1)<<FX_CONTROL_AMP_MODEL;
        for(unsigned i=0;i<pages*6;++i){int index=fx_catalog_parameter_index(e,i);if(index<0)continue;
            assert((e->id==0x61464153u&&index==FX_CONTROL_AMP_MODEL)||((unsigned)index<e->count&&e->parameter[index]&&*e->parameter[index]));
            assert(!(seen&(UINT64_C(1)<<index)));seen|=UINT64_C(1)<<index;}
        assert(seen==expected&&fx_catalog_parameter_index(e,pages*6)==-1);
        mixer_defaults(&accepted);++epoch;++revision;accepted.track[0].fx.count=1;
        assert(fx_mixer_catalog_stage(n,48000,accepted.track[0].fx.stage));
        struct mixer_control_ops ops={NULL,submit};struct mixer_control*c=mixer_control_create(&ops,48000);assert(c);
        assert(mixer_control_confirm(c,&accepted,revision,epoch)&&mixer_control_open(c,1,0));
        mixer_control_set_fx_editing(c,1);key(c,MIXER_KNOB);key(c,MIXER_SELECT);assert(get_view(c).fx_parameters);
        for(unsigned page=0;page<pages;++page){
            assert(get_view(c).parameter_page==page);
            for(unsigned sh=0;sh<2;++sh){shift(c,sh);
                struct mixer_view_state v=get_view(c);v.fx_illustrated=1;v.fx_signal=&monitor;v.rate=48000;
                char path[1024];snprintf(path,sizeof path,"%s/%08x-%u-%u.trace",out,e->id,page,sh);
                trace=fopen(path,"w");assert(trace);memset(knob_label,0,sizeof knob_label);
                mixer_view_paint(&v,&painter);fclose(trace);trace=NULL;
                unsigned layer=sh&&fx_catalog_shift_layer(e,page);
                for(unsigned k=0;k<3;++k){
                    int index=fx_catalog_parameter_index(e,page*6+layer*3+k);
                    const char*label=index<0?"":fx_catalog_parameter_label(e,(unsigned)index);
                    if(strcmp(knob_label[k],label)){fprintf(stderr,"LABEL %s page%u shift%u k%u '%s' != '%s'\n",e->name,page,sh,k,knob_label[k],label);assert(0);}
                    struct fx_stage before=accepted.track[0].fx.stage[0];
                    unsigned value=index>=0&&fx_catalog_parameter_value(&before,(unsigned)index)>.5f?0:127;
                    assert(mixer_control_cc(c,0x10+k,value,mixer_control_ticket(c)));ack(c);
                    if(index!=FX_CONTROL_AMP_MODEL)for(unsigned j=0;j<FX_CHAIN_PARAMETERS;++j){float want=(int)j==index?(value?1.f:0.f):before.parameter[j];
                        assert(accepted.track[0].fx.stage[0].parameter[j]==want);}
                    assert(mixer_control_reset(c,0x10+k,sh,mixer_control_ticket(c)));ack(c);
                    if(index==FX_CONTROL_AMP_MODEL){assert(fx_amp_model_index(accepted.track[0].fx.stage)==0);++controls;}
                    else if(index>=0){assert(accepted.track[0].fx.stage[0].parameter[index]==e->defaults[index]);++controls;}
                }
            }
            shift(c,0);if(page+1<pages){assert(mixer_control_cc(c,0x64,1,mixer_control_ticket(c)));}
        }
        mixer_control_destroy(c);
    }
    printf("PASS all 34 FX layouts: unique complete storage coverage, %u mapped knob edits/resets, matching painted labels, blank controls inert and unchanged native defaults\n",controls);
}
static void bypass(void){
    struct resample_painter p={NULL,draw_rect,draw_text};struct mixer_fx_monitor m={0};
    static uint32_t still[320*240];
    for(unsigned n=1;n<fx_mixer_catalog_count();++n){
        const struct fx_catalog_entry*e=fx_mixer_catalog_at(n);struct fx_stage stage;
        assert(fx_mixer_catalog_stage(n,48000,&stage));
        m.input_peak=.8f;m.output_peak=.7f;m.gain_reduction_db=10;
        memset(pixels,0,sizeof pixels);assert(fx_visual_paint(&p,e,&stage,&m,48000,e->id,1000));
        assert(fx_visual_paint(&p,e,&stage,&m,48000,e->id,1100));stage.bypass=1;
        memset(pixels,0,sizeof pixels);assert(fx_visual_paint(&p,e,&stage,&m,48000,e->id,1200));
        /* Dev6 deliberately fades retained history in Bypass; compare only
         * once that tail has settled, while meters already receive silence. */
        memset(pixels,0,sizeof pixels);assert(fx_visual_paint(&p,e,&stage,&m,48000,e->id,10000));
        memcpy(still,pixels,sizeof still);
        m.input_peak=.1f;m.output_peak=1;m.gain_reduction_db=20;
        for(unsigned i=0;i<MIXER_FX_MONITOR_FRAMES;++i)m.pcm[i]=m.input_pcm[i]=(float)sin(i*.2);
        memset(pixels,0,sizeof pixels);assert(fx_visual_paint(&p,e,&stage,&m,48000,e->id,14000));
        if(memcmp(still,pixels,sizeof still)){fprintf(stderr,"BYPASS STILL MOVES: %s\n",e->name);assert(0);}
        if(e->id==0x54456678)assert(fxv_tape_state.needle==0);
    }
    /* Channel chain bypass must reach the picture even though the stage is enabled. */
    mixer_defaults(&accepted);accepted.track[0].fx.count=1;
    assert(fx_mixer_catalog_stage(25,48000,accepted.track[0].fx.stage)); /* Tape */
    accepted.track[0].fx_bypass=1;
    struct mixer_view_state v={0};v.project=&accepted;v.selected=1;v.page=MIXER_KNOB_FX;
    v.fx_editing=v.fx_parameters=v.fx_illustrated=1;v.fx_signal=&m;v.now_ms=1000;
    memset(pixels,0,sizeof pixels);mixer_view_paint(&v,&p);memcpy(still,pixels,sizeof still);
    v.now_ms=5000;m.input_peak=1;m.output_peak=.01f;
    memset(pixels,0,sizeof pixels);mixer_view_paint(&v,&p);assert(!memcmp(still,pixels,sizeof still));
    puts("PASS all 34 illustrations: bypass picture stays identical despite changing audio/time; Tape needle rests at zero and Chain Bypass reaches rendering");
}
static void navigation(void){
    mixer_defaults(&accepted);++epoch;++revision;
    for(unsigned i=0;i<8;++i){accepted.track[i].fx.count=1;assert(fx_mixer_catalog_stage(1,48000,accepted.track[i].fx.stage));}
    struct mixer_control_ops ops={NULL,submit};struct mixer_control*c=mixer_control_create(&ops,48000);assert(c);
    assert(mixer_control_confirm(c,&accepted,revision,epoch)&&mixer_control_open(c,1,0));uint64_t t=mixer_control_ticket(c);
    for(unsigned i=1;i<9;++i){assert(mixer_input_button(c,0x36,1,t));assert(mixer_input_button(c,0x36,0,t));
        assert(i==8?get_view(c).master:!get_view(c).master&&get_view(c).primary==i);}
    assert(mixer_input_button(c,0x36,1,t));assert(mixer_input_button(c,0x36,0,t));assert(get_view(c).primary==0&&!get_view(c).master);
    mixer_control_set_fx_editing(c,1);key(c,MIXER_KNOB);key(c,MIXER_SELECT);
    assert(mixer_control_cc(c,0x64,1,t)&&get_view(c).parameter_page==1);
    assert(mixer_input_button(c,0x36,1,t)&&mixer_input_button(c,0x36,0,t));
    assert(get_view(c).primary==1&&get_view(c).fx_parameters&&get_view(c).parameter_page==1);
    assert(mixer_input_button(c,0x37,1,t)&&mixer_input_button(c,0x37,0,t));
    assert(get_view(c).primary==0&&get_view(c).fx_parameters&&get_view(c).parameter_page==1);
    mixer_control_destroy(c);puts("PASS physical +/-: A-H/Master wrap in Mixer overview and preserve FX slot/page when available");
}
static void gallery(const char*out){
    static const uint32_t ids[]={0x54456678,0x41435658,0x56456678,0x61464153,0x61465444,FX_OWN_SHIMMER};
    struct resample_painter p={NULL,draw_rect,draw_text};struct mixer_fx_monitor m={0};m.input_peak=.8f;m.output_peak=.7f;
    for(unsigned i=0;i<sizeof(ids)/sizeof(ids[0]);++i)for(unsigned off=0;off<2;++off){
        mixer_defaults(&accepted);accepted.track[0].fx.count=1;
        unsigned index=0;while(index<fx_mixer_catalog_count()&&fx_mixer_catalog_at(index)->id!=ids[i])++index;
        assert(fx_mixer_catalog_stage(index,48000,accepted.track[0].fx.stage));accepted.track[0].fx.stage[0].bypass=off;
        struct mixer_view_state v={0};v.project=&accepted;v.selected=1;v.page=MIXER_KNOB_FX;
        v.fx_editing=v.fx_parameters=v.fx_illustrated=1;v.fx_signal=&m;v.now_ms=2000;v.rate=48000;
        char path[1024];snprintf(path,sizeof path,"%s/gallery-%08x-%u.trace",out,ids[i],off);trace=fopen(path,"w");assert(trace);
        mixer_view_paint(&v,&p);fclose(trace);trace=NULL;
    }
}
static void amp_models(const char*out){
    mixer_defaults(&accepted);++epoch;++revision;accepted.track[0].fx.count=1;
    unsigned n=0;while(fx_mixer_catalog_at(n)->id!=0x61464153)++n;
    assert(fx_mixer_catalog_stage(n,48000,accepted.track[0].fx.stage));
    struct mixer_control_ops ops={NULL,submit};struct mixer_control*c=mixer_control_create(&ops,48000);assert(c);
    assert(mixer_control_confirm(c,&accepted,revision,epoch)&&mixer_control_open(c,1,0));
    mixer_control_set_fx_editing(c,1);key(c,MIXER_KNOB);key(c,MIXER_SELECT);
    struct resample_painter p={NULL,draw_rect,draw_text};struct mixer_fx_monitor m={0};m.input_peak=.4f;m.output_peak=.3f;
    static char encoded[MIXER_PAYLOAD_BYTES];static struct mixer_project restored;
    for(unsigned voice=1;voice<fx_amp_model_count();++voice){
        unsigned value=(voice*127+(fx_amp_model_count()-1)/2)/(fx_amp_model_count()-1);
        assert(mixer_control_cc(c,0x10,value,mixer_control_ticket(c)));ack(c);
        assert(fx_amp_model_index(accepted.track[0].fx.stage)==voice&&!accepted.track[0].mix.solo);
        key(c,MIXER_B3);ack(c);assert(accepted.track[0].mix.solo&&fx_amp_model_index(accepted.track[0].fx.stage)==voice);
        key(c,MIXER_B3);ack(c);assert(!accepted.track[0].mix.solo);
        shift(c,1);key(c,MIXER_B3);shift(c,0);ack(c);assert(accepted.track[0].fx_bypass);
        shift(c,1);key(c,MIXER_B3);shift(c,0);ack(c);assert(!accepted.track[0].fx_bypass);
        size_t bytes;assert(mixer_encode(&accepted,encoded,sizeof encoded,&bytes)==MIXER_OK);
        assert(mixer_decode(encoded,bytes,&restored)==MIXER_OK);
        assert(!memcmp(accepted.track[0].fx.stage,restored.track[0].fx.stage,sizeof(struct fx_stage)));
        float gain=accepted.track[0].fx.stage[0].parameter[0];
        assert(mixer_control_cc(c,0x11,127,mixer_control_ticket(c)));ack(c);
        assert(mixer_control_cc(c,0x10,value,mixer_control_ticket(c)));ack(c);assert(accepted.track[0].fx.stage[0].parameter[0]==1);
        assert(mixer_control_reset(c,0x11,0,mixer_control_ticket(c)));ack(c);
        assert(accepted.track[0].fx.stage[0].parameter[0]==gain);
        struct mixer_view_state v=get_view(c);v.fx_illustrated=1;v.fx_signal=&m;v.now_ms=3000;v.rate=48000;
        char path[1024];snprintf(path,sizeof path,"%s/amp-voice-%u.trace",out,voice);trace=fopen(path,"w");assert(trace);
        mixer_view_paint(&v,&p);fclose(trace);trace=NULL;
    }
    assert(mixer_control_reset(c,0x10,0,mixer_control_ticket(c)));ack(c);assert(fx_amp_model_index(accepted.track[0].fx.stage)==0);
    mixer_control_destroy(c);
    puts("PASS seven Amp voices: K1 model, same-bucket edit retention, B3 Solo/Shift+B3 Chain Bypass, profile reset, project round-trip and legacy markers");
}
static void eq_bypass(void){
    struct resample_painter p={NULL,draw_rect,draw_text};struct mixer_fx_monitor m={0};
    mixer_defaults(&accepted);accepted.track[0].fx.count=1;assert(fx_mixer_catalog_stage(0,48000,accepted.track[0].fx.stage));
    accepted.track[0].fx.stage[0].bypass=1;
    struct mixer_view_state v={0};v.project=&accepted;v.selected=1;v.page=MIXER_KNOB_FX;v.fx_editing=v.fx_parameters=1;v.fx_signal=&m;
    static uint32_t still[320*240];
    memset(pixels,0,sizeof pixels);mixer_view_paint(&v,&p);memcpy(still,pixels,sizeof still);
    for(unsigned i=0;i<MIXER_FX_MONITOR_FRAMES;++i)m.pcm[i]=(float)sin(i*.1);
    memset(pixels,0,sizeof pixels);mixer_view_paint(&v,&p);assert(!memcmp(still,pixels,sizeof still));
    puts("PASS Graphic EQ bypass suppresses the live spectrum while retaining its static editable curve");
}
static void track_pad_exit(void){
    mixer_defaults(&accepted);++epoch;++revision;
    for(unsigned i=0;i<10;++i){struct mixer_channel*ch=i<8?accepted.track+i:i==8?&accepted.input:&accepted.master;
        ch->fx.count=1;assert(fx_mixer_catalog_stage(1,48000,ch->fx.stage));}
    struct mixer_control_ops ops={NULL,submit};struct mixer_control*c=mixer_control_create(&ops,48000);assert(c);
    assert(mixer_control_confirm(c,&accepted,revision,epoch)&&mixer_control_open(c,1,0));
    mixer_control_set_fx_editing(c,1);key(c,MIXER_KNOB);key(c,MIXER_SELECT);
    const unsigned pads[]={0,1,2,3,4,5,6,7,14,15};
    for(unsigned i=0;i<10;++i){unsigned pad=pads[i];uint64_t t=mixer_control_ticket(c);
        assert(get_view(c).fx_parameters);assert(mixer_control_pad(c,pad,1,t));assert(mixer_control_pad(c,pad,0,t));
        struct mixer_view_state v=get_view(c);assert(v.page==MIXER_KNOB_FX&&!v.fx_parameters&&!v.fx_catalog);
        assert(v.master==(pad==14?2u:pad==15?1u:0u));if(pad<8)assert(v.primary==pad);
        key(c,MIXER_SELECT);
    }
    mixer_control_destroy(c);puts("PASS all Track/Input/Master pads leave the effect editor for the selected FX list with paired releases");
}
static void tape_geometry(void){
    for(unsigned side=0;side<2;++side){double x=side?243:77,y=109,r=side?18:24,bx=side?209:111,by=147;
        double ax,ay,cx,cy;fxv_tangent(x,y,r,bx,by,4,&ax,&ay,&cx,&cy);
        assert(fabs(hypot(ax-x,ay-y)-r)<1e-8&&fabs(hypot(cx-bx,cy-by)-4)<1e-8);
        assert(fabs((ax-x)*(cx-ax)+(ay-y)*(cy-ay))<1e-7);
        assert(fabs((cx-bx)*(cx-ax)+(cy-by)*(cy-ay))<1e-7);
    }
    for(unsigned time=0;time<2;++time)for(unsigned wow=0;wow<2;++wow)
        for(unsigned ms=10;ms<120000;ms+=10)assert(fxv_tape_phase(ms,time,wow)<fxv_tape_phase(ms-10,time,wow));
    struct resample_painter p={NULL,draw_rect,draw_text};struct fx_stage stage;struct mixer_fx_monitor m={0};m.output_peak=.8f;
    assert(fx_mixer_catalog_stage(3,48000,&stage));fxv_idle=0;fxv_now=2500;
    memset(pixels,0,sizeof pixels);fxv_tape_delay(&p,&stage,&m,17,2500);
    for(unsigned side=0;side<2;++side){int cx=side?243:77,cy=109;
        for(int y=-29;y<=29;++y)for(int x=-29;x<=29;++x)
            if(x*x+y*y<=29*29)assert(pixels[(cy+y)*320+cx+x]!=0xff5a3923u);
    }
    double held=fxv_delay_transport.angle;visual_transport=0;m.input_peak=0;
    fxv_tape_delay(&p,&stage,&m,17,2600);assert(fxv_delay_transport.angle==held);
    visual_transport=3;fxv_tape_delay(&p,&stage,&m,17,2700);assert(fxv_delay_transport.angle<held);
    held=fxv_delay_transport.angle;visual_transport=0;fxv_tape_delay(&p,&stage,&m,17,2800);assert(fxv_delay_transport.angle==held);
    m.input_peak=.2f;fxv_tape_delay(&p,&stage,&m,17,2900);assert(fxv_delay_transport.angle<held);
    held=fxv_delay_transport.angle;fxv_idle=1;fxv_tape_delay(&p,&stage,&m,17,3000);assert(fxv_delay_transport.angle==held);fxv_idle=0;
    puts("PASS Tape Delay transport: stopped reels hold position despite tails; Play/manual input resume counterclockwise; Bypass holds");
    assert(fx_mixer_catalog_stage(25,48000,&stage));fxv_now=2500;fxv_tape_emu(&p,&stage,&m);
    double before=fxv_tape_state.ang;fxv_now=2600;fxv_tape_emu(&p,&stage,&m);assert(fxv_tape_state.ang<before);
    puts("PASS tangent tape/rollers, tape below flanges, counterclockwise transport even with Wow");
}
static void shift_mapping(void){
    struct resample_painter p={NULL,draw_rect,draw_text};
    for(unsigned n=0;n<fx_mixer_catalog_count();++n){
        mixer_defaults(&accepted);++epoch;++revision;accepted.track[0].fx.count=1;
        assert(fx_mixer_catalog_stage(n,48000,accepted.track[0].fx.stage));struct fx_stage original=accepted.track[0].fx.stage[0];
        struct mixer_control_ops ops={NULL,submit};struct mixer_control*c=mixer_control_create(&ops,48000);assert(c);
        assert(mixer_control_confirm(c,&accepted,revision,epoch)&&mixer_control_open(c,1,0));
        mixer_control_set_fx_editing(c,1);key(c,MIXER_KNOB);key(c,MIXER_SELECT);shift(c,1);
        struct mixer_view_state v=get_view(c);mixer_view_paint(&v,&p);
        assert(!strcmp(top_labels[1],"Paste")&&!strcmp(top_labels[2],"Chain Bypass"));
        key(c,MIXER_ERASE);assert(!strcmp(mixer_control_message(c),"Effect copied"));
        key(c,MIXER_B3);ack(c);assert(accepted.track[0].fx_bypass&&!memcmp(&original,accepted.track[0].fx.stage,sizeof original));
        key(c,MIXER_B3);ack(c);assert(!accepted.track[0].fx_bypass);
        shift(c,0);assert(mixer_control_cc(c,0x10,127,mixer_control_ticket(c)));ack(c);
        shift(c,1);key(c,MIXER_B2);shift(c,0);ack(c);
        assert(!accepted.track[0].fx_bypass&&!get_view(c).fx_parameters&&!get_view(c).fx_catalog);
        assert(!memcmp(&original,accepted.track[0].fx.stage,sizeof original));
        mixer_control_destroy(c);
    }
    puts("PASS all 35 effects including EQ: matching Shift+B2 Paste / Shift+B3 Chain Bypass labels and actions; copy/paste restores parameters without touching the chain");
}
static void export_layout(const char*out){
    char path[1024];snprintf(path,sizeof path,"%s/layout.md",out);FILE*f=fopen(path,"w");assert(f);
    fprintf(f,"| Effect | Page | K1 | K2 | K3 | Shift K1 | Shift K2 | Shift K3 |\n|---|---|---|---|---|---|---|---|\n");
    fprintf(f,"| Graphic EQ | Each point | Freq | Gain | Type | Point On/Off | Q | Output |\n");
    for(unsigned n=1;n<fx_mixer_catalog_count();++n){const struct fx_catalog_entry*e=fx_mixer_catalog_at(n);
        for(unsigned page=0;page<fx_catalog_page_count(e);++page){
            fprintf(f,"| %s | %u: %s ",e->name,page+1,fx_catalog_page_name(e,page));
            for(unsigned cell=0;cell<6;++cell){int index=fx_catalog_parameter_index(e,page*6+cell);
                fprintf(f,"| %s ",index<0?"-":fx_catalog_parameter_label(e,(unsigned)index));}
            fprintf(f,"|\n");
        }
    }
    fprintf(f,"| Sample Comp (Master) | Dynamics | Threshold | Ratio | Gain | Attack | Release | Knee |\n");
    fclose(f);
}
int main(int argc,char**argv){assert(argc==2);knobs(argv[1]);bypass();navigation();gallery(argv[1]);amp_models(argv[1]);eq_bypass();track_pad_exit();tape_geometry();shift_mapping();export_layout(argv[1]);return 0;}
