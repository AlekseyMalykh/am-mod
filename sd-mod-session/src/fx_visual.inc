/* Illustrated effect screens ("FX view: Illustrated").
 * Textual include for mixer_view.c: uses its rect/text helpers and the same
 * 320x240 rect/text painter, UI thread only. Everything is computed while the
 * screen is being painted; the audio side only supplies the monitor that the
 * open effect already requested. Return 0 to fall back to the Classic view. */
#define FXV_X 12
#define FXV_Y 74
#define FXV_W 296
#define FXV_H 94
#define FXV_DB_TOP 12.0
#define FXV_DB_SPAN 60.0

static double fxv_hz(float normalized){return 20.0*pow(1000.0,fmax(0.0,fmin(1.0,(double)normalized)));}
static int fxv_x_for_hz(double hz){
    return FXV_X+(int)lround((FXV_W-1)*fmax(0.0,fmin(1.0,log10(hz/20.0)/3.0)));
}
/* kind 0 High Pass, 1 Low Pass, 2 Band Pass. Display model only. */
static double fxv_filter_db(int kind,double hz,double cutoff,double resonance)
{
    double octaves=log2(hz/cutoff),m;
    if(kind==1)m=1.0/sqrt(1.0+pow(hz/cutoff,8.0));
    else if(kind==0)m=1.0/sqrt(1.0+pow(cutoff/hz,8.0));
    else{double x=octaves*(1.5+5.0*resonance);m=1.0/sqrt(1.0+x*x*x*x);}
    if(kind!=2)m*=1.0+3.0*resonance*exp(-pow(octaves*3.0,2.0));
    else m*=1.0+1.5*resonance;
    return 20.0*log10(fmax(m,1e-4));
}
static int fxv_db_y(double db)
{
    double v=(FXV_DB_TOP-fmax(-48.0,fmin(FXV_DB_TOP,db)))/FXV_DB_SPAN;
    return FXV_Y+8+(int)lround(v*(FXV_H-24));
}
static void fxv_filter(const struct resample_painter*p,const struct fx_stage*stage,int kind,
                       const struct mixer_fx_monitor*m,double rate)
{
    const float cutoff_v=kind==2?stage->parameter[4]:stage->parameter[12];
    const float res_v=kind==2?stage->parameter[5]:stage->parameter[8];
    const double cutoff=fxv_hz(cutoff_v),res=fmax(0.0,fmin(1.0,(double)res_v));
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    /* Frequency grid: 100 Hz, 1 kHz, 10 kHz, and the 0 dB line. */
    static const double marks[3]={100,1000,10000};
    for(unsigned i=0;i<3;++i)rect(p,fxv_x_for_hz(marks[i]),FXV_Y+2,1,FXV_H-20,DARK);
    rect(p,FXV_X,fxv_db_y(0),FXV_W,1,LINE);
    rect(p,FXV_X,fxv_db_y(-24),FXV_W,1,DARK);
    if(m){
        fx_spectrum(p,m->input_pcm,rate,FXV_X,FXV_Y+10,FXV_W,FXV_H-28,0xff2a4a5au,0);
        fx_spectrum(p,m->pcm,rate,FXV_X,FXV_Y+10,FXV_W,FXV_H-28,0xff3a2c10u,1);
    }
    int previous=fxv_db_y(fxv_filter_db(kind,20.0,cutoff,res));
    for(int x=0;x<FXV_W;x+=2){
        double hz=20.0*pow(1000.0,(double)x/(FXV_W-1));
        int y=fxv_db_y(fxv_filter_db(kind,hz,cutoff,res));
        int top=y<previous?y:previous;
        rect(p,FXV_X+x,top,2,abs(y-previous)+2,AMBER);
        previous=y;
    }
    int cx=fxv_x_for_hz(cutoff),cy=fxv_db_y(fxv_filter_db(kind,cutoff,cutoff,res));
    rect(p,cx,FXV_Y+2,1,FXV_H-20,WHITE);
    rect(p,cx-3,cy-3,7,7,WHITE);
    rect(p,cx-1,cy-1,3,3,AMBER);
    /* Thin level strips along the bottom edge: Input (blue) over Output (amber). */
    float peaks[2]={m?m->input_peak:0,m?m->output_peak:0};
    for(unsigned i=0;i<2;++i){
        int y=FXV_Y+FXV_H-6+(int)i*3;
        rect(p,FXV_X,y,FXV_W,2,DARK);
        int w=isfinite(mixer_meter_dbfs(peaks[i]))?(int)lroundf((FXV_W-1)*fmaxf(0,fminf(1,(mixer_meter_dbfs(peaks[i])+60)/60))):0;
        if(w)rect(p,FXV_X,y,w,2,i?AMBER:BLUE);
    }
    char label[48];
    if(cutoff>=1000.0)snprintf(label,sizeof(label),"%s  %.1f kHz  Res %.0f%%",kind==1?"Low Pass":kind==0?"High Pass":"Band Pass",cutoff/1000.0,res*100.0);
    else snprintf(label,sizeof(label),"%s  %.0f Hz  Res %.0f%%",kind==1?"Low Pass":kind==0?"High Pass":"Band Pass",cutoff,res*100.0);
    text(p,label,FXV_X+4,FXV_Y+1,FXV_W-8,12,10,GREY,0);
    text(p,"20",FXV_X+2,FXV_Y+FXV_H-17,30,11,9,GREY,0);
    text(p,"1k",fxv_x_for_hz(1000)-15,FXV_Y+FXV_H-17,30,11,9,GREY,1);
    text(p,"20k",FXV_X+FXV_W-34,FXV_Y+FXV_H-17,32,11,9,GREY,0);
}

/* ---- Dynamics: rolling level history, kept only while the screen is painted. ---- */
#define FXV_HIST 148u
static struct {uint32_t identity;uint64_t last_ms;unsigned head,count;
    float in[FXV_HIST],out[FXV_HIST],key[FXV_HIST],gr[FXV_HIST];} fxv_hist;
static void fxv_history_push(uint32_t identity,uint64_t now_ms,const struct mixer_fx_monitor*m)
{
    if(fxv_hist.identity!=identity||now_ms<fxv_hist.last_ms){
        memset(&fxv_hist,0,sizeof(fxv_hist));fxv_hist.identity=identity;fxv_hist.last_ms=now_ms;
    }
    if(!m||now_ms-fxv_hist.last_ms<40)return;
    fxv_hist.last_ms=now_ms;
    unsigned i=fxv_hist.head;
    fxv_hist.in[i]=m->input_peak;fxv_hist.out[i]=m->output_peak;
    fxv_hist.key[i]=m->key_peak;fxv_hist.gr[i]=m->gain_reduction_db;
    fxv_hist.head=(i+1)%FXV_HIST;if(fxv_hist.count<FXV_HIST)++fxv_hist.count;
}
static int fxv_level_h(float peak,int height)
{
    float db=mixer_meter_dbfs(peak);
    return isfinite(db)?(int)lroundf(height*fmaxf(0,fminf(1,(db+60)/60))):0;
}
/* Strip chart: output level as amber bars from the bottom, optional input line,
 * optional key (violet) and gain reduction hanging from the top in red-orange. */
static void fxv_strip(const struct resample_painter*p,int x,int y,int w,int h,int show_in,int show_key,float threshold_db)
{
    rect(p,x,y,w,h,WELL);
    for(int g=1;g<4;++g)rect(p,x,y+g*h/4,w,1,DARK);
    unsigned n=fxv_hist.count;
    int cols=w/2;if((unsigned)cols>FXV_HIST)cols=(int)FXV_HIST;
    for(int c=0;c<cols;++c){
        int age=cols-1-c;if((unsigned)age>=n)continue;
        unsigned i=(fxv_hist.head+FXV_HIST-1-(unsigned)age)%FXV_HIST;
        int px=x+w-2-age*2;
        if(show_in){int hi=fxv_level_h(fxv_hist.in[i],h);if(hi)rect(p,px,y+h-hi,2,1,BLUE);}
        if(show_key){int hk=fxv_level_h(fxv_hist.key[i],h);if(hk)rect(p,px,y+h-hk,2,2,KEY);}
        int ho=fxv_level_h(fxv_hist.out[i],h);if(ho)rect(p,px,y+h-ho,2,ho,0xff5a4419u);
        if(ho)rect(p,px,y+h-ho,2,2,AMBER);
        int hg=(int)lroundf(h*fminf(24.f,fmaxf(0.f,fxv_hist.gr[i]))/24.f);
        if(hg)rect(p,px,y,2,hg,0xffff6a3cu);
    }
    if(isfinite(threshold_db)){
        int ty=y+h-(int)lroundf(h*fmax(0.0,fmin(1.0,(threshold_db+60.0)/60.0)));
        rect(p,x,ty,w,1,WHITE);
    }
}
static void fxv_gr_text(const struct resample_painter*p,const struct mixer_fx_monitor*m,int x,int y,int w)
{
    char buf[24];
    snprintf(buf,sizeof(buf),m&&m->gain_reduction_db>0?"GR -%.1f dB":"GR 0.0 dB",m?m->gain_reduction_db:0.f);
    text(p,buf,x,y,w,12,10,0xffff9a7cu,0);
}
static void fxv_title(const struct resample_painter*p,const char*s,int x,int y,int w){text(p,s,x,y,w,12,10,GREY,0);}

/* Bus Compressor: transfer curve with a live dot, history to the right. */
static void fxv_bus_comp(const struct resample_painter*p,const struct fx_stage*stage,const struct mixer_fx_monitor*m)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,PANEL);
    const int gx=FXV_X+6,gy=FXV_Y+14,gs=72;
    rect(p,gx,gy,gs,gs,WELL);
    rect(p,gx,gy+gs/2,gs,1,DARK);rect(p,gx+gs/2,gy,1,gs,DARK);
    double thr=-36.0+36.0*fmax(0.0,fmin(1.0,(double)stage->parameter[0]));      /* display model */
    double ratio=1.5+18.5*pow(fmax(0.0,fmin(1.0,(double)stage->parameter[13])),1.5);
    for(int x=0;x<gs;++x){
        double in=-60.0+60.0*x/(gs-1),out=in<=thr?in:thr+(in-thr)/ratio;
        int y=gy+gs-1-(int)lround((fmax(-60.0,out)+60.0)*(gs-1)/60.0);
        rect(p,gx+x,y,1,2,AMBER);
    }
    rect(p,gx+(int)lround((thr+60.0)*(gs-1)/60.0),gy,1,gs,LINE);
    if(m){
        float di=mixer_meter_dbfs(m->input_peak),dout=mixer_meter_dbfs(m->output_peak);
        if(isfinite(di)&&isfinite(dout)){
            int dx=gx+(int)lround((fmax(-60.f,di)+60.f)*(gs-1)/60.f),dy=gy+gs-1-(int)lround((fmax(-60.f,dout)+60.f)*(gs-1)/60.f);
            rect(p,dx-2,dy-2,5,5,WHITE);
        }
    }
    fxv_title(p,"Transfer",gx,FXV_Y+1,72);
    fxv_title(p,"Level history  (red: reduction)",FXV_X+92,FXV_Y+1,200);
    fxv_strip(p,FXV_X+92,FXV_Y+14,FXV_W-98,FXV_H-34,1,0,NAN);
    fxv_gr_text(p,m,FXV_X+92,FXV_Y+FXV_H-18,120);
    char buf[32];snprintf(buf,sizeof(buf),"%.1f:1",ratio);
    text(p,buf,FXV_X+FXV_W-78,FXV_Y+FXV_H-18,72,12,10,WHITE,0);
}
/* Limiter: waveform clipped at the ceiling. */
static void fxv_waveform(const struct resample_painter*p,const struct mixer_fx_monitor*m,int x,int y,int w,int h,double ceiling_lin)
{
    rect(p,x,y,w,h,WELL);
    int c=y+h/2,amp=h/2-2;
    rect(p,x,c,w,1,DARK);
    if(ceiling_lin>0&&ceiling_lin<1){
        int d=(int)lround(amp*ceiling_lin);
        rect(p,x,c-d,w,1,SEND);rect(p,x,c+d,w,1,SEND);
    }
    if(!m)return;
    int prev_in=c,prev_out=c;
    for(int cx=0;cx<w;cx+=2){
        unsigned i=(unsigned)cx*MIXER_FX_MONITOR_FRAMES/(unsigned)w;
        int yi=c-(int)lroundf(amp*fmaxf(-1,fminf(1,m->input_pcm[i])));
        int yo=c-(int)lroundf(amp*fmaxf(-1,fminf(1,m->pcm[i])));
        int ti=yi<prev_in?yi:prev_in,to=yo<prev_out?yo:prev_out;
        rect(p,x+cx,ti,1,abs(yi-prev_in)+1,0xff386477u);
        rect(p,x+cx,to,2,abs(yo-prev_out)+2,AMBER);
        prev_in=yi;prev_out=yo;
    }
}
static void fxv_limiter(const struct resample_painter*p,const struct fx_stage*stage,const struct mixer_fx_monitor*m)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,PANEL);
    double ceiling_db=-12.0+12.0*fmax(0.0,fmin(1.0,(double)stage->parameter[1]));  /* display model */
    fxv_title(p,"Waveform  /  red = ceiling",FXV_X+4,FXV_Y+1,230);
    fxv_waveform(p,m,FXV_X+4,FXV_Y+14,FXV_W-8,FXV_H-34,pow(10.0,ceiling_db/20.0));
    fxv_gr_text(p,m,FXV_X+4,FXV_Y+FXV_H-18,120);
    char buf[24];snprintf(buf,sizeof(buf),"Ceiling %.1f dB",ceiling_db);
    text(p,buf,FXV_X+FXV_W-134,FXV_Y+FXV_H-18,130,12,10,WHITE,0);
}
/* Pumper: the pumping envelope as history, plus depth. */
static void fxv_pumper(const struct resample_painter*p,const struct fx_stage*stage,const struct mixer_fx_monitor*m)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,PANEL);
    fxv_title(p,"Pumping  (amber level, red depth)",FXV_X+4,FXV_Y+1,230);
    fxv_strip(p,FXV_X+4,FXV_Y+14,FXV_W-8,FXV_H-34,1,0,NAN);
    fxv_gr_text(p,m,FXV_X+4,FXV_Y+FXV_H-18,120);
    char buf[32];snprintf(buf,sizeof(buf),"Depth %.0f%%  Speed %.0f%%",stage->parameter[0]*100.f,stage->parameter[6]*100.f);
    text(p,buf,FXV_X+FXV_W-170,FXV_Y+FXV_H-18,166,12,10,WHITE,0);
}
/* Transient: before/after envelope drawn from the Attack and Sustain settings. */
static void fxv_transient(const struct resample_painter*p,const struct fx_stage*stage,const struct mixer_fx_monitor*m)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,PANEL);
    const int gx=FXV_X+6,gy=FXV_Y+14,gw=150,gh=FXV_H-34;
    rect(p,gx,gy,gw,gh,WELL);
    double att=2.0*(stage->parameter[0]-.5),sus=2.0*(stage->parameter[2]-.5);
    int prev0=gy+gh,prev1=gy+gh;
    for(int x=0;x<gw;x+=2){
        double u=(double)x/gw,base=u<.08?u/.08:exp(-(u-.08)*3.2);
        double shaped=base*(u<.2?1.0+.7*att:1.0+.6*sus);
        shaped=fmax(0.0,fmin(1.0,shaped));
        int y0=gy+gh-1-(int)lround(base*(gh-2)),y1=gy+gh-1-(int)lround(shaped*(gh-2));
        rect(p,gx+x,y0<prev0?y0:prev0,2,abs(y0-prev0)+1,0xff606a73u);
        rect(p,gx+x,y1<prev1?y1:prev1,2,abs(y1-prev1)+2,AMBER);
        prev0=y0;prev1=y1;
    }
    fxv_title(p,"Envelope",gx,FXV_Y+1,150);
    fxv_title(p,"Level history",FXV_X+166,FXV_Y+1,130);
    fxv_strip(p,FXV_X+166,FXV_Y+14,FXV_W-172,FXV_H-34,1,0,NAN);
    char buf[40];snprintf(buf,sizeof(buf),"Attack %+.0f%%   Sustain %+.0f%%",att*100.0,sus*100.0);
    text(p,buf,FXV_X+4,FXV_Y+FXV_H-18,200,12,10,WHITE,0);
    (void)m;
}
/* Noise Gate: waveform with threshold window and OPEN / CLOSED state. */
static void fxv_gate(const struct resample_painter*p,const struct fx_stage*stage,const struct mixer_fx_monitor*m)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,PANEL);
    double thr_db=-60.0+60.0*fmax(0.0,fmin(1.0,(double)stage->parameter[0]));      /* display model */
    fxv_title(p,"Waveform  /  red = threshold",FXV_X+4,FXV_Y+1,250);
    fxv_waveform(p,m,FXV_X+4,FXV_Y+14,FXV_W-8,FXV_H-34,pow(10.0,thr_db/20.0));
    int open=m&&m->input_peak>0&&m->output_peak>m->input_peak*.25f;
    text(p,open?"OPEN":"CLOSED",FXV_X+4,FXV_Y+FXV_H-18,80,12,11,open?0xff6fe07au:SEND,0);
    char buf[24];snprintf(buf,sizeof(buf),"Threshold %.0f dB",thr_db);
    text(p,buf,FXV_X+FXV_W-134,FXV_Y+FXV_H-18,130,12,10,WHITE,0);
}
/* Sidechain Compressor: key (violet) against output with the reduction above. */
static void fxv_sidechain(const struct resample_painter*p,const struct fx_stage*stage,const struct mixer_fx_monitor*m)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,PANEL);
    unsigned source=(unsigned)fx_sidechain_value(0,stage->parameter[0]);
    float threshold=fx_sidechain_value(1,stage->parameter[1]);
    char buf[48];
    if(source)snprintf(buf,sizeof(buf),"Key Track %c (violet)  /  output (amber)",'A'+(int)source-1);
    else snprintf(buf,sizeof(buf),"Key off  /  output (amber)");
    fxv_title(p,buf,FXV_X+4,FXV_Y+1,290);
    fxv_strip(p,FXV_X+4,FXV_Y+14,FXV_W-8,FXV_H-34,0,1,threshold);
    fxv_gr_text(p,m,FXV_X+4,FXV_Y+FXV_H-18,120);
    snprintf(buf,sizeof(buf),"Threshold %.1f dB",threshold);
    text(p,buf,FXV_X+FXV_W-134,FXV_Y+FXV_H-18,130,12,10,WHITE,0);
}
static int fx_visual_paint(const struct resample_painter*p,const struct fx_catalog_entry*e,
                           const struct fx_stage*stage,const struct mixer_fx_monitor*m,double rate,
                           uint32_t identity,uint64_t now_ms)
{
    if(!e||!stage)return 0;
    if(stage->kind==FX_STAGE_SIDECHAIN){fxv_history_push(identity,now_ms,m);fxv_sidechain(p,stage,m);return 1;}
    if(stage->kind!=FX_STAGE_NATIVE)return 0;
    switch(e->id){
    case 0x41435663:fxv_filter(p,stage,0,m,rate);return 1; /* HP Filter */
    case 0x41435662:fxv_filter(p,stage,1,m,rate);return 1; /* LP Filter */
    case 0x61465346:fxv_filter(p,stage,2,m,rate);return 1; /* BP Filter */
    case 0x4143564a:fxv_history_push(identity,now_ms,m);fxv_bus_comp(p,stage,m);return 1;  /* Bus Compressor */
    case 0x61464c6d:fxv_history_push(identity,now_ms,m);fxv_limiter(p,stage,m);return 1;   /* Limiter */
    case 0x61465075:fxv_history_push(identity,now_ms,m);fxv_pumper(p,stage,m);return 1;    /* Pumper */
    case 0x61465472:fxv_history_push(identity,now_ms,m);fxv_transient(p,stage,m);return 1; /* Transient */
    case 0x61464e47:fxv_history_push(identity,now_ms,m);fxv_gate(p,stage,m);return 1;      /* Noise Gate */
    default:return 0;
    }
}
