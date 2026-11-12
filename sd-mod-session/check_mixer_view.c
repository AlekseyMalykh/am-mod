#include "src/mixer_view.h"
#include "src/mixer_buses.h"
#include "src/fx_catalog.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
static FILE*trace;
static unsigned delete_probe,delete_buttons,add_point_ink;
static unsigned overview_keys,fader_focus,master_cap_x,master_cap_ink,knob_centres,knob_bars[3];
static unsigned dynamic,draws,na,thumbs,letters,mstr,tabs,eq_labels,fx_headers,amber_spectrum,send_height,send_x,send_width,threshold_line,threshold_label,input_label,output_label,key_label,catalog_hint;
static void r(void*u,int x,int y,int w,int h,uint32_t c){(void)u;
    assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240);++draws;
    if(dynamic)assert(y+h<=198);
    if(delete_probe&&delete_buttons&&y<19)assert(c!=0xff000000u);
    if(y==234&&x==53&&w==1&&h==4)++knob_centres;
    if(y==235&&h==2)++knob_bars[x/106];
    if(y==36&&w==6&&h==6){master_cap_x=(unsigned)x;master_cap_ink=c;}
    if(h==9&&(w==14||w==18))++thumbs;
    if(h==7&&(w==12||w==16)&&c==0xffffc247u)++fader_focus;
    if(c==0xe05a4419u)++amber_spectrum;
    if(c==0xffff4040u&&w==2){send_height=(unsigned)h;send_x=(unsigned)x;}
    if(c==0xffff4040u&&y+h==194&&h==5)send_width=(unsigned)w;
    if(c==0xffffffffu&&w==1&&y==92&&h==12)++threshold_line;
    if(trace)fprintf(trace,"R %d %d %d %d %08x\n",x,y,w,h,c);}
static void t(void*u,const char*s,int x,int y,int w,int h,unsigned size,uint32_t c,int centre){(void)u;
    assert(s&&x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240&&size>=8);++draws;
    if(delete_probe&&y==1){
        if(!strcmp(s,"Cancel")){assert(c==0xffffffffu);delete_buttons|=1;}
        if(!strcmp(s,"Delete")){assert(c==0xffff4040u);delete_buttons|=2;}
    }
    if(y<18){
        if(!strcmp(s,"Route"))overview_keys|=1;
        if(!strcmp(s,"Solo"))overview_keys|=2;
        if(!strcmp(s,"Rename"))overview_keys|=4;
        if(!strcmp(s,"Clear Solo"))overview_keys|=8;
    }
    if(y==200&&h==12){
        if(x==4&&!strcmp(s,"Freq"))eq_labels|=1;
        if(x==110&&!strcmp(s,"Gain"))eq_labels|=2;
        if(x==216&&!strcmp(s,"Type"))eq_labels|=4;
    }
    if(strstr(s,"Threshold ")||!strncmp(s,"Ceiling ",8))++threshold_label;
    if(y==135||y==100||y==109)input_label+=!strcmp(s,"Input");
    if(y==152||y==121||y==127)output_label+=!strcmp(s,"Output");
    if(y==91)key_label+=!strcmp(s,"Key");
    if(!strcmp(s,"Add effect")&&y>=199)++catalog_hint;
    if(!strcmp(s,"N/A"))++na;
    if(!strcmp(s,"FX List")||!strcmp(s,"+ EQ Point"))++fx_headers;
    if(!strcmp(s,"+ EQ Point"))add_point_ink=c;
    if(strlen(s)==1&&s[0]>='A'&&s[0]<='H'&&size>=18)++letters;
    if(!strcmp(s,"MSTR"))++mstr;
    if(!strncmp(s,"B1",2)&&strlen(s)>3&&s[2]==' '&&s[3]!=':')++tabs;
    if(y>=44&&y<198&&!strncmp(s,"Track ",6)&&s[6]>='A'&&s[6]<='H'&&!s[7])assert(0); /* no "Track A" labels */
    if(dynamic)assert(y+h<=198);
    if(trace)fprintf(trace,"T %d %d %d %d %u %08x %d %s\n",x,y,w,h,size,c,centre,s);}
static void paint_trace(const char*directory,const char*name,const struct mixer_view_state*s,
                        const struct resample_painter*p)
{
    char path[1024];snprintf(path,sizeof(path),"%s/%s.trace",directory,name);
    trace=fopen(path,"w");assert(trace);
    mixer_view_paint(s,p);fclose(trace);trace=NULL;
}
int main(int argc,char**argv)
{
    assert(argc==2);struct mixer_project project;mixer_defaults(&project);
    const float pans[]={-1,-.5f,0,.25f,.5f,.75f,1,-.25f};
    for(unsigned i=0;i<8;++i){project.output[i]=(unsigned char)(i+1);project.track[i].mix.level_db=-(float)i*3;project.track[i].mix.pan=pans[i];}
    project.track[7].mix.level_db=-INFINITY;project.track[1].mix.mute=1;
    struct mixer_meters*m=mixer_meters_create(48000);enum mixer_bus_result error;struct mixer_buses*b=mixer_buses_prepare(&project,0,48000,16,NULL,&error);assert(b&&m);
    float pcm[16];assert(mixer_buses_begin(b,16)==MIXER_BUS_OK);
    for(unsigned i=0;i<8;++i){for(unsigned n=0;n<16;++n)pcm[n]=(i+1)*.025f;
        assert(mixer_buses_accumulate(b,i,pcm,pcm,2,0)==MIXER_BUS_OK);}
    assert(mixer_buses_finish(b,m,7)==MIXER_BUS_OK);struct mixer_meter_snapshot snapshot;assert(mixer_meters_take(m,7,&snapshot));
    struct mixer_view_state s={.selected=5,.primary=2,.project=&project,.meter=&snapshot,.epoch=7,.meter_ms=1000,.now_ms=1000};
    struct resample_painter painter={NULL,r,t};char path[1024];
    /* Mixer page: 2x4 Track cells, A-H letters, no tab labels, MSTR header. */
    s.page=MIXER_OVERVIEW;snprintf(path,sizeof(path),"%s/overview.trace",argv[1]);trace=fopen(path,"w");assert(trace);
    thumbs=letters=mstr=tabs=na=0;mixer_view_paint(&s,&painter);fclose(trace);trace=NULL;
    assert(thumbs==8&&letters==8&&mstr==1&&!tabs&&na==0);
    assert(fader_focus==2);
    unsigned unity_cap=master_cap_x;
    assert(unity_cap>245&&master_cap_ink==0xff787d84u);
    s.master=1;project.master.mix.level_db=-INFINITY;
    mixer_view_paint(&s,&painter);assert(master_cap_x==246&&master_cap_ink==0xffffc247u);
    project.master.mix.level_db=6;
    mixer_view_paint(&s,&painter);assert(master_cap_x>unity_cap);
    project.master.mix.level_db=0;s.master=0;
    fader_focus=0;knob_centres=0;
    memset(knob_bars,0,sizeof(knob_bars));
    mixer_view_paint(&s,&painter);
    assert(overview_keys==3&&knob_centres==1&&knob_bars[0]==1&&knob_bars[1]==1&&knob_bars[2]==12);
    project.track[s.primary].mix.pan=-.5f;
    s.shifted=1;overview_keys=0;knob_bars[0]=knob_bars[1]=0;
    mixer_view_paint(&s,&painter);assert(overview_keys==12&&knob_bars[0]==2&&knob_bars[1]==9);
    s.shifted=0;s.cursor=PARAM_PAN;fader_focus=0;
    mixer_view_paint(&s,&painter);assert(!fader_focus);s.cursor=PARAM_LEVEL;
    project.send_to[0]=2;project.track[0].mix.send=1;
    snapshot.return_mask=2;snapshot.return_peak[1]=.5f;
    mixer_view_meters(&s,1,&painter);assert(!send_height);
    mixer_view_meters(&s,2,&painter);unsigned full=send_height;assert(full&&send_x==102);
    snapshot.return_peak[1]=.1f;send_height=0;
    mixer_view_meters(&s,2,&painter);assert(send_height>0&&send_height<full);
    s.page=MIXER_ASSIGN;s.primary=1;
    mixer_view_meters(&s,2,&painter);assert(send_height&&send_x==32);
    s.page=MIXER_KNOB_FX;
    mixer_view_meters(&s,2,&painter);assert(send_height&&send_x==32);
    s.page=MIXER_OVERVIEW;s.primary=2;
    snapshot.return_mask=0;send_height=0;
    mixer_view_meters(&s,2,&painter);assert(!send_height);
    /* PAD FX / KNOB FX: left area with the big name; Master as MSTR. */
    const enum mixer_page pages[]={MIXER_ASSIGN,MIXER_INSPECT,MIXER_KNOB_FX};const char*files[]={"assign","inspect","knob-fx"};
    for(unsigned i=0;i<3;++i){s.page=pages[i];snprintf(path,sizeof(path),"%s/%s.trace",argv[1],files[i]);trace=fopen(path,"w");assert(trace);
        thumbs=letters=0;mixer_view_paint(&s,&painter);fclose(trace);trace=NULL;assert(thumbs==1&&letters>=1);}
    s.page=MIXER_KNOB_FX;s.fx_editing=1;s.fx_parameters=1;s.rate=48000;
    assert(fx_catalog_stage(0,s.rate,&project.track[s.primary].fx.stage[0]));
    project.track[s.primary].fx.count=1;
    struct mixer_fx_monitor monitor={.input_peak=.8f,.output_peak=.5f,.key_peak=.4f,.gain_reduction_db=10};
    for(unsigned i=0;i<MIXER_FX_MONITOR_FRAMES;++i){
        monitor.pcm[i]=.5f*sinf(i*.13f);
        monitor.input_pcm[i]=.8f*sinf(i*.13f);
    }
    s.fx_signal=&monitor;
    snapshot.return_mask=1u<<s.primary;snapshot.return_peak[s.primary]=.5f;
    mixer_view_paint(&s,&painter);assert(eq_labels==7&&amber_spectrum&&send_width);
    full=send_width;snapshot.return_peak[s.primary]=.1f;send_width=0;
    mixer_view_meters(&s,1u<<s.primary,&painter);assert(send_width&&send_width<full);
    snapshot.return_mask=1u<<1;send_width=0;
    mixer_view_paint(&s,&painter);assert(!send_width);
    snapshot.return_mask=1u<<s.primary;
    for(unsigned i=1;i<fx_mixer_catalog_count();++i){
        struct fx_stage*stage=&project.track[s.primary].fx.stage[0];
        assert(fx_mixer_catalog_stage(i,s.rate,stage));
        const struct fx_catalog_entry*entry=fx_mixer_catalog_at(i);
        unsigned pages=(fx_catalog_parameter_count(entry)+5)/6;
        for(unsigned page=0;page<pages;++page){
            s.parameter_page=page;
            for(unsigned shift=0;shift<2;++shift){
                s.shifted=shift;
                send_width=input_label=output_label=key_label=threshold_line=threshold_label=0;
                mixer_view_paint(&s,&painter);
                assert(send_width&&input_label==1&&output_label==1);
                if(stage->kind==FX_STAGE_SIDECHAIN)assert(key_label&&threshold_line&&threshold_label);
            }
        }
    }
    snapshot.return_mask=0;
    fx_headers=0;s.page=MIXER_OVERVIEW;s.shifted=1;
    mixer_view_paint(&s,&painter);assert(!fx_headers); /* Stale editor flags cannot leak into overview. */
    s.page=MIXER_KNOB_FX;s.fx_parameters=0;s.fx_catalog=1;
    mixer_view_paint(&s,&painter);assert(fx_headers==1&&catalog_hint);
    s.fx_catalog=0;
    s.shifted=0;s.fx_signal=NULL;
    s.page=MIXER_ASSIGN;s.master=1;mstr=0;mixer_view_paint(&s,&painter);assert(mstr>=2);s.master=0;
    s.page=MIXER_OVERVIEW;dynamic=1;draws=0;mixer_view_meters(&s,1,&painter);assert(draws>0&&draws<15);dynamic=0;
    /* Capture actual painter output for visual review with populated tracks. */
    s.primary=2;s.selected=1u<<2;s.bank=0;s.sound=1;s.present[0]=0x01110001015fu;
    project.output[1]=project.output[4]=project.output[6]=3;
    strcpy(project.track[2].name,"Bass");
    project.track[2].fx.count=3;
    assert(fx_catalog_stage(0,s.rate,&project.track[2].fx.stage[0]));
    assert(fx_mixer_catalog_stage(fx_catalog_count(),s.rate,&project.track[2].fx.stage[1]));
    assert(fx_catalog_stage(5,s.rate,&project.track[2].fx.stage[2]));
    project.track[2].fx.stage[1].parameter[0]=.15f;
    struct fx_eq_config*eq=&project.track[2].fx.stage[0].eq;
    eq->count=3;eq->band[0].gain_db=7;eq->band[1].gain_db=-4;eq->band[2].gain_db=3;
    snapshot.return_mask=1u<<2;snapshot.return_peak[2]=.15f;
    paint_trace(argv[1],"mixer",&s,&painter);
    s.selected|=1u<<0;paint_trace(argv[1],"group",&s,&painter);s.selected=1u<<2;
    s.master=1;paint_trace(argv[1],"master",&s,&painter);s.master=0;
    s.page=MIXER_ASSIGN;paint_trace(argv[1],"assign",&s,&painter);
    s.page=MIXER_INSPECT;paint_trace(argv[1],"inspect",&s,&painter);
    project.send_to[0]=3;project.track[0].mix.send=.35f;
    project.send_to[1]=3;project.track[1].mix.send=.7f;
    s.route_tab=MIXER_ROUTE_SENDS;paint_trace(argv[1],"routing-sends",&s,&painter);
    s.route_tab=MIXER_ROUTE_KEYS;paint_trace(argv[1],"routing-keys",&s,&painter);
    s.master=1;paint_trace(argv[1],"routing-master",&s,&painter);s.master=0;
    s.route_tab=MIXER_ROUTE_PADS;
    s.page=MIXER_OVERVIEW;s.name_edit="Bass";
    paint_trace(argv[1],"rename",&s,&painter);s.name_edit=NULL;
    s.page=MIXER_KNOB_FX;s.fx_editing=1;s.fx_slot=1;
    paint_trace(argv[1],"chain",&s,&painter);
    s.fx_delete=MIXER_DELETE_EFFECT;delete_probe=1;paint_trace(argv[1],"delete",&s,&painter);
    assert(delete_buttons==3);s.fx_delete=delete_probe=0;
    s.fx_catalog=1;s.fx_slot=3;s.cursor=4;paint_trace(argv[1],"catalog",&s,&painter);s.fx_catalog=0;
    s.fx_parameters=1;s.parameter_page=0;s.fx_signal=&monitor;
    s.fx_slot=0;paint_trace(argv[1],"eq",&s,&painter);
    s.fx_delete=MIXER_DELETE_POINT;delete_probe=1;delete_buttons=0;
    paint_trace(argv[1],"eq-delete",&s,&painter);assert(delete_buttons==3);
    s.fx_delete=delete_probe=0;
    eq->count=0;paint_trace(argv[1],"eq-empty",&s,&painter);eq->count=3;
    s.shifted=1;paint_trace(argv[1],"eq-shift",&s,&painter);
    assert(add_point_ink==0xffffffffu);
    eq->count=MIXER_EQ_POINTS;eq->band[3]=(struct fx_eq_band){8000,0,1,FX_EQ_BELL,1};
    paint_trace(argv[1],"eq-full",&s,&painter);assert(add_point_ink==0xff9ba5b0u);
    eq->count=3;s.shifted=0;
    s.fx_slot=1;paint_trace(argv[1],"sidechain",&s,&painter);
    s.fx_slot=2;paint_trace(argv[1],"reverb",&s,&painter);
    assert(fx_catalog_stage(7,s.rate,&project.track[2].fx.stage[2]));
    paint_trace(argv[1],"filter",&s,&painter);
    s.page=MIXER_OVERVIEW;s.fx_parameters=0;s.fx_signal=NULL;
    s.now_ms=1251;na=0;mixer_view_paint(&s,&painter);assert(na==9);
    s.now_ms=1000;s.epoch=8;na=0;mixer_view_paint(&s,&painter);assert(na==9);
    assert(mixer_view_gain_y(0)==114&&mixer_view_gain_y(6)==102&&mixer_view_gain_y(-60)==174&&mixer_view_gain_y(-INFINITY)==177);
    assert(mixer_view_gain_db(114)==0&&mixer_view_gain_db(102)==6&&isinf(mixer_view_gain_db(177)));
    mixer_buses_destroy(b);mixer_meters_destroy(m);puts("PASS Mixer page 2x4 Track cells with A-H, MSTR header meter, no tab labels, PAD FX/KNOB FX left area, bounds, stale/epoch invalidation and meter-only repaint");return 0;
}
