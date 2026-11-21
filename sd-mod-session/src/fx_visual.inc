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
static unsigned fxv_history_bypass;
static struct {uint32_t identity;uint64_t last_ms,fade_ms;unsigned head,count;
    float in[FXV_HIST],out[FXV_HIST],key[FXV_HIST],gr[FXV_HIST];} fxv_hist;
static void fxv_history_push(uint32_t identity,uint64_t now_ms,const struct mixer_fx_monitor*m)
{
    if(fxv_hist.identity!=identity||now_ms<fxv_hist.last_ms){
        memset(&fxv_hist,0,sizeof(fxv_hist));fxv_hist.identity=identity;fxv_hist.last_ms=fxv_hist.fade_ms=now_ms;
    }
    if(fxv_history_bypass&&now_ms>fxv_hist.fade_ms){
        uint64_t dt=now_ms-fxv_hist.fade_ms;float decay=expf(-(float)(dt>2000?2000:dt)/250.f);
        for(unsigned i=0;i<FXV_HIST;++i){fxv_hist.in[i]*=decay;fxv_hist.out[i]*=decay;fxv_hist.key[i]*=decay;fxv_hist.gr[i]*=decay;}
    }
    fxv_hist.fade_ms=now_ms;
    uint64_t elapsed=(now_ms-fxv_hist.last_ms)/40;if(!elapsed)return;
    fxv_hist.last_ms+=elapsed*40;
    unsigned steps=elapsed>FXV_HIST?FXV_HIST:(unsigned)elapsed;
    for(unsigned step=0;step<steps;++step){unsigned i=fxv_hist.head;int current=step+1==steps&&m&&!fxv_history_bypass;
        fxv_hist.in[i]=current?m->input_peak:0;fxv_hist.out[i]=current?m->output_peak:0;
        fxv_hist.key[i]=current?m->key_peak:0;fxv_hist.gr[i]=current?m->gain_reduction_db:0;
        fxv_hist.head=(i+1)%FXV_HIST;if(fxv_hist.count<FXV_HIST)++fxv_hist.count;
    }
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
    int gr_h=show_key?h/4:0;               /* sidechain: reduction gets its own strip below */
    int ph=h-gr_h;
    for(int c=0;c<cols;++c){
        int age=cols-1-c;if((unsigned)age>=n)continue;
        unsigned i=(fxv_hist.head+FXV_HIST-1-(unsigned)age)%FXV_HIST;
        int px=x+w-2-age*2;
        if(show_in){int hi=fxv_level_h(fxv_hist.in[i],ph);if(hi)rect(p,px,y+ph-hi,2,1,BLUE);}
        if(show_key){
            int hk=fxv_level_h(fxv_hist.key[i],ph);if(hk)rect(p,px,y+ph-hk,2,hk,0xff4a1414u); /* key: filled area */
            int ho=fxv_level_h(fxv_hist.out[i],ph);if(ho)rect(p,px,y+ph-ho,2,2,AMBER);       /* output: line only */
            int hg=(int)lroundf(gr_h*fminf(24.f,fmaxf(0.f,fxv_hist.gr[i]))/24.f);
            if(hg)rect(p,px,y+ph+1,2,hg,0xff9a6bffu);
        }else{
            int ho=fxv_level_h(fxv_hist.out[i],h);if(ho)rect(p,px,y+h-ho,2,ho,0xff5a4419u);
            if(ho)rect(p,px,y+h-ho,2,2,AMBER);
            int hg=(int)lroundf(h*fminf(24.f,fmaxf(0.f,fxv_hist.gr[i]))/24.f);
            if(hg)rect(p,px,y,2,hg,0xffff6a3cu);
        }
    }
    if(show_key){ /* Key outline: bright red line over the fill and the output. */
        int prev=-1;
        for(int c=0;c<cols;++c){
            int age=cols-1-c;if((unsigned)age>=n)continue;
            unsigned i=(fxv_hist.head+FXV_HIST-1-(unsigned)age)%FXV_HIST;
            int py=y+ph-fxv_level_h(fxv_hist.key[i],ph),px=x+w-2-age*2;
            if(prev<0)prev=py;
            int top=py<prev?py:prev;rect(p,px,top,2,abs(py-prev)+2,0xffff2a2au);prev=py;
        }
        rect(p,x,y+ph,w,1,GREY);
    }
    if(isfinite(threshold_db)){
        int ty=y+ph-(int)lroundf(ph*fmax(0.0,fmin(1.0,(threshold_db+60.0)/60.0)));
        for(int dx=0;dx<w;dx+=6)rect(p,x+dx,ty,4,1,WHITE);   /* dashed: same dB scale as the key */
    }
}
static int fxv_idle;
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
    if(ceiling_lin>0&&ceiling_lin<=1.0000001){
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
    char buf[64];
    if(source)snprintf(buf,sizeof(buf),"Key %c (red)  Out (amber)  Threshold (white)",'A'+(int)source-1);
    else snprintf(buf,sizeof(buf),"Key off  Out (amber)  Threshold (white)");
    fxv_title(p,buf,FXV_X+4,FXV_Y+1,290);
    fxv_strip(p,FXV_X+4,FXV_Y+14,FXV_W-8,FXV_H-34,0,1,threshold);
    fxv_gr_text(p,m,FXV_X+4,FXV_Y+FXV_H-18,120);
    snprintf(buf,sizeof(buf),"Threshold %.1f dB",threshold);
    text(p,buf,FXV_X+FXV_W-134,FXV_Y+FXV_H-18,130,12,10,WHITE,0);
}

/* ---- Delay and reverb: animated from the paint clock; parameters drive the picture. ---- */
static uint64_t fxv_now;
static uint32_t fxv_acc=0xffffc247u;
#define CYAN 0xff40c8e0u
static float fxv_p(const struct fx_stage*stage,unsigned i){return fmaxf(0.f,fminf(1.f,stage->parameter[i]));}
static void fxv_disc(const struct resample_painter*p,int cx,int cy,int r,uint32_t c)
{
    for(int y=-r;y<=r;++y){int w=(int)lround(sqrt((double)(r*r-y*y)));rect(p,cx-w,cy+y,2*w+1,1,c);}
}
static void fxv_delay(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    float fb=fxv_p(stage,3);int spacing=16+(int)lround(56*fxv_p(stage,0));
    double ratio=pow(2.,(fxv_p(stage,2)-.5)*2.);
    text(p,"STEREO ECHO",FXV_X+7,FXV_Y+1,110,9,8,GREY,0);
    text(p,fxv_p(stage,1)>=.5?"SYNC":"FREE",FXV_X+135,FXV_Y+1,42,9,8,GREY,1);
    for(int lane=0;lane<2;++lane){
        int base=FXV_Y+45+lane*37,start=FXV_X+26;uint32_t col=lane?CYAN:AMBER;
        int step=(int)lround(spacing*(lane?ratio:1));if(step<8)step=8;
        rect(p,start,base,FXV_W-33,1,LINE);
        text(p,lane?"R":"L",FXV_X+4,base-19,15,13,10,col,1);
        for(int x=start;x<FXV_X+FXV_W-8;x+=20)rect(p,x,base+1,1,3,DARK);
        for(int k=0;k<20;++k){int x=start+k*step;if(x>FXV_X+FXV_W-14)break;
            double amp=pow(fb,k);if(k&&amp<.005)break;
            int h=(int)lround(27*amp);if(h<1)h=1;
            rect(p,x,base-h,6,h,k?0xff354c50u:0xff82939fu);rect(p,x,base-h,6,2,k?col:WHITE);
            if(k&&k<6)rect(p,x+2,base-h-3,1,2,col);
        }
    }
    int span=FXV_W-35;int px=FXV_X+26+(int)((fxv_now/16)%(uint64_t)span);
    if(!fxv_idle)rect(p,px,FXV_Y+12,1,FXV_H-20,WHITE);
}
static void fxv_diff_delay(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,PANEL);
    int base=FXV_Y+FXV_H-22;float fb=fxv_p(stage,2),dif=fxv_p(stage,4);int spread=60+(int)lround(200*fxv_p(stage,1));
    rect(p,FXV_X+4,base,FXV_W-8,1,LINE);
    text(p,"EARLY",FXV_X+5,FXV_Y+1,50,9,7,GREY,0);
    for(int x=FXV_X+22;x<FXV_X+FXV_W-6;x+=16)rect(p,x,base+2,1,3,DARK);
    for(int i=0;i<spread;i+=4){double u=(double)i/spread;
        int yy=base-(int)lround(58*pow(1-u,2.4-fb*1.6));rect(p,FXV_X+22+i,yy,2,1,0xff7d9b92u);}
    rect(p,FXV_X+12,base-55,4,55,AMBER);
    unsigned seed=12345;int n=14+(int)lround(46*dif);
    int sweep=fxv_idle?spread:(int)((fxv_now/10)%(uint64_t)(spread+40));
    for(int i=0;i<n;++i){
        seed=seed*1664525u+1013904223u;double u=pow((seed>>8&0xffff)/65535.0,.8);
        int x=FXV_X+22+(int)lround(u*spread);if(x-FXV_X-22>sweep)continue;
        seed=seed*1664525u+1013904223u;double r=(seed>>8&0xffff)/65535.0;
        double amp=pow(1-u,2.4-fb*1.6)*(.35+.65*r);int h=(int)lround(58*amp);if(h<2)h=2;
        rect(p,x,base-h,3,h,amp>.5?AMBER:0xff7a5f22u);
    }
    if(!fxv_idle)rect(p,FXV_X+22+(sweep<spread?sweep:spread),FXV_Y+8,1,FXV_H-30,WHITE);
}
static void fxv_line(const struct resample_painter*p,double x0,double y0,double x1,double y1,int w,uint32_t c)
{
    double dx=x1-x0,dy=y1-y0;int n=(int)(fmax(fabs(dx),fabs(dy)))+1;
    for(int i=0;i<=n;++i){double t=(double)i/n;rect(p,(int)lround(x0+dx*t)-w/2,(int)lround(y0+dy*t)-w/2,w,w,c);}
}
static void fxv_disc_clip(const struct resample_painter*p,int cx,int cy,int r,int y0,int y1,uint32_t c)
{
    for(int y=-r;y<=r;++y){if(cy+y<y0||cy+y>=y1)continue;int w=(int)lround(sqrt((double)(r*r-y*y)));rect(p,cx-w,cy+y,2*w+1,1,c);}
}
/* External tangent between a reel pack (c1,r1) and a roller (c2,r2) on the lower side; returns the two touch points. */
static void fxv_tangent(double x1,double y1,double r1,double x2,double y2,double r2,double*ax,double*ay,double*bx,double*by)
{
    double dx=x2-x1,dy=y2-y1,l=sqrt(dx*dx+dy*dy),ux=dx/l,uy=dy/l,nx=-uy,ny=ux;
    if(ny<0){nx=-nx;ny=-ny;}
    double s=(r1-r2)/l,c=sqrt(1-s*s),mx=s*ux+c*nx,my=s*uy+c*ny;
    *ax=x1+r1*mx;*ay=y1+r1*my;*bx=x2+r2*mx;*by=y2+r2*my;
}
static void fxv_reel(const struct resample_painter*p,int cx,int cy,int pack,int hub,double ang,uint32_t tape)
{
    fxv_disc(p,cx,cy,pack,tape);fxv_disc(p,cx,cy,hub,0xffd9dce2u);fxv_disc(p,cx,cy,hub-3,0xff1c2026u);
    for(int k=0;k<6;++k){double a=ang+k*1.0472;
        rect(p,cx+(int)lround(cos(a)*(hub-2))-1,cy+(int)lround(sin(a)*(hub-2))-1,3,3,0xffd9dce2u);}
}
/* Small hardware details share geometry; no bitmap uploads or per-frame allocations. */
static void fxv_screw(const struct resample_painter*p,int x,int y,uint32_t rim)
{
    fxv_disc(p,x,y,2,0xff11151au);fxv_disc(p,x,y,1,rim);
    rect(p,x-1,y,3,1,0xff50565eu);
}
static void fxv_vu(const struct resample_painter*p,int x,int y,int w,int h,double value,const char*label)
{
    rect(p,x,y,w,h,0xff0c1014u);rect(p,x+1,y+1,w-2,h-2,0xff9a8b6eu);
    rect(p,x+2,y+2,w-4,h-4,0xffeee2bdu);rect(p,x+3,y+3,w-6,2,0xfffff0cdu);
    int cx=x+w/2,cy=y+h-4;double radius=h-9,rx=(w-9)*.5;
    for(int t=0;t<9;++t){double angle=-1.05+t*2.1/8;
        double xx=cx+sin(angle)*rx,yy=cy-cos(angle)*radius;
        fxv_line(p,xx,yy,xx+sin(angle)*2,yy-cos(angle)*2,1,t>=6?0xffa83826u:0xff62553du);
    }
    text(p,"-20",x+3,y+h-12,14,7,5,0xff62553du,0);
    text(p,"0",x+w-12,y+5,8,7,5,0xffa83826u,1);
    text(p,label,x+4,y+h-10,w-8,7,6,0xff62553du,1);
    double angle=-1.05+2.1*fmax(0.,fmin(1.,value));
    fxv_line(p,cx,cy,cx+sin(angle)*(rx-2),cy-cos(angle)*(radius-1),1,0xff201c18u);
    fxv_disc(p,cx,cy,2,0xff403829u);
    rect(p,x+3,y+h-3,w-6,1,0xffb6a47eu);
}
static double fxv_vu_value(float peak)
{
    if(fxv_idle||!isfinite(peak)||peak<=0)return 0;
    return fmax(0.,fmin(1.,(mixer_meter_dbfs(peak)+36.)/39.));
}
static void fxv_open_reel(const struct resample_painter*p,int cx,int cy,int pack,double angle)
{
    fxv_disc(p,cx+1,cy+1,30,0xff0b0e12u);
    fxv_disc(p,cx,cy,30,0xff5b626cu);fxv_disc(p,cx,cy,29,0xffd7dce2u);
    fxv_disc(p,cx,cy,27,0xff262c34u);fxv_disc(p,cx,cy,pack,0xff513723u);
    for(int r=12;r<pack;r+=3)for(int k=0;k<36;++k){double a=k*6.283185307/36;
        rect(p,cx+(int)lround(cos(a)*r),cy+(int)lround(sin(a)*r),1,1,0xff725037u);}
    for(int k=0;k<3;++k){double a=angle+k*2.0943951;
        fxv_line(p,cx+cos(a)*8,cy+sin(a)*8,cx+cos(a)*26,cy+sin(a)*26,5,0xffb5bdc8u);
        fxv_line(p,cx+cos(a)*10,cy+sin(a)*10,cx+cos(a)*25,cy+sin(a)*25,1,0xfff0f2f5u);
        double b=a+1.0472;fxv_disc(p,cx+(int)lround(cos(b)*22),cy+(int)lround(sin(b)*22),3,0xff10161cu);
    }
    fxv_disc(p,cx,cy,9,0xff747e8bu);fxv_disc(p,cx,cy,7,0xffe6e9edu);
    fxv_disc(p,cx,cy,5,0xff2a3039u);fxv_disc(p,cx,cy,2,0xffc1c9d2u);
    fxv_line(p,cx-3,cy-1,cx+3,cy+1,1,0xff0c1117u);
}
static double fxv_tape_phase(uint64_t ms,double time,double wow)
{
    /* Bounded wobble cannot reverse transport as elapsed time grows. */
    return -(ms*(.0022+.0045*time)+.03*wow*sin(ms*.01));
}
extern unsigned step_native_transport_state(void) __attribute__((weak));
static struct {uint32_t identity;uint64_t last_ms,phase_ms;double angle;int initialized,running;} fxv_delay_transport;
static void fxv_tape_delay(const struct resample_painter*p,const struct fx_stage*stage,const struct mixer_fx_monitor*m,uint32_t identity,uint64_t now_ms)
{
    const uint32_t TAPE=0xff5a3923u,STEEL=0xffa9b2bcu;
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,0xff5d3a24u);
    for(int k=0;k<5;++k){rect(p,FXV_X+2+k*3,FXV_Y+2,1,FXV_H-4,0xff805334u);
        rect(p,FXV_X+FXV_W-15+k*3,FXV_Y+2,1,FXV_H-4,0xff805334u);}
    rect(p,FXV_X+17,FXV_Y+1,FXV_W-34,FXV_H-2,0xff9ca5adu);
    rect(p,FXV_X+18,FXV_Y+2,FXV_W-36,FXV_H-4,0xff303940u);
    rect(p,FXV_X+19,FXV_Y+3,FXV_W-38,1,0xff697680u);
    for(int row=0;row<5;++row)rect(p,FXV_X+116,FXV_Y+15+row*2,64,1,0xff171e24u);
    text(p,"AM MOD",FXV_X+109,FXV_Y+3,78,9,8,0xffd9d3b8u,1);
    int lx=FXV_X+65,rx=FXV_X+231,cy=FXV_Y+35;
    if(!fxv_delay_transport.initialized||fxv_delay_transport.identity!=identity||now_ms<fxv_delay_transport.last_ms){
        memset(&fxv_delay_transport,0,sizeof fxv_delay_transport);fxv_delay_transport.initialized=1;fxv_delay_transport.identity=identity;fxv_delay_transport.last_ms=now_ms;
    }
    uint64_t elapsed=now_ms-fxv_delay_transport.last_ms;fxv_delay_transport.last_ms=now_ms;if(elapsed>100)elapsed=0;
    unsigned transport=step_native_transport_state?step_native_transport_state():0;
    fxv_delay_transport.running=!fxv_idle&&(((transport&3u)==3u)||(m&&m->input_peak>.0001f));
    if(fxv_delay_transport.running){uint64_t phase=fxv_delay_transport.phase_ms;
        fxv_delay_transport.angle+=fxv_tape_phase(phase+elapsed,fxv_p(stage,8),fxv_p(stage,6))-fxv_tape_phase(phase,fxv_p(stage,8),fxv_p(stage,6));
        fxv_delay_transport.phase_ms+=elapsed;
    }
    double angle=fxv_delay_transport.angle;
    fxv_vu(p,FXV_X+108,FXV_Y+28,38,29,fxv_vu_value(m?m->input_peak:0),"IN");
    fxv_vu(p,FXV_X+150,FXV_Y+28,38,29,fxv_vu_value(m?m->output_peak:0),"OUT");
    double ax,ay,bx,by;int py=FXV_Y+73,pl=FXV_X+99,pr=FXV_X+197;
    fxv_tangent(lx,cy,24,pl,py,4,&ax,&ay,&bx,&by);fxv_line(p,ax,ay,bx,by,2,TAPE);
    fxv_tangent(rx,cy,18,pr,py,4,&ax,&ay,&bx,&by);fxv_line(p,ax,ay,bx,by,2,TAPE);
    fxv_line(p,pl,py+4,pr,py+4,2,TAPE);
    fxv_open_reel(p,lx,cy,24,angle);fxv_open_reel(p,rx,cy,18,angle);
    for(int k=0;k<2;++k){int x=k?pr:pl;fxv_disc(p,x,py,5,0xff151b20u);fxv_disc(p,x,py,3,STEEL);fxv_disc(p,x,py,1,0xff303940u);}
    for(int k=0;k<4;++k){int x=FXV_X+116+k*17;
        rect(p,x,FXV_Y+64,12,12,0xff151b21u);rect(p,x+1,FXV_Y+64,10,9,STEEL);
        rect(p,x+2,FXV_Y+65,8,2,0xffe0e4e8u);rect(p,x+5,FXV_Y+66,1,8,0xff606972u);
        text(p,k==0?"E":"R",x,FXV_Y+80,12,7,6,0xffc4cbd1u,1);
    }
    rect(p,FXV_X+29,FXV_Y+78,52,10,0xff151b20u);
    text(p,"TAPE ECHO",FXV_X+30,FXV_Y+79,50,8,7,0xffc9c0a2u,1);
    for(int k=0;k<3;++k){int x=FXV_X+220+k*16;fxv_disc(p,x,FXV_Y+79,5,STEEL);
        fxv_line(p,x,FXV_Y+79,x-2,FXV_Y+75,1,0xff12181eu);}
    for(int k=0;k<4;++k)fxv_screw(p,FXV_X+(k&1?FXV_W-23:23),FXV_Y+(k&2?FXV_H-6:6),STEEL);
}
static void fxv_sample_delay(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,0xff12161au);
    rect(p,FXV_X+70,FXV_Y+4,156,13,0xff2f3a45u);
    text(p,"AM MOD  SAMPLE DELAY",FXV_X+72,FXV_Y+4,152,13,9,0xffd8dde4u,1);
    for(int lane=0;lane<2;++lane){
        int base=FXV_Y+(lane?76:52);uint32_t col=lane?CYAN:AMBER;
        float off=fxv_p(stage,lane);int step=6+(int)lround(26*off);
        rect(p,FXV_X+10,base,FXV_W-20,1,DARK);
        for(int x=FXV_X+10;x<FXV_X+FXV_W-10;x+=8)rect(p,x,base+1,1,3,LINE);
        rect(p,FXV_X+12,base-16,5,16,col);
        int n=0;
        for(int x=FXV_X+12+step;x<FXV_X+FXV_W-14&&n<8;x+=step,++n){int h=(int)lround(16*pow(.72,n+1));if(h<2)h=2;rect(p,x,base-h,5,h,col);}
        int sx=FXV_X+12+(int)((fxv_now/14+lane*70)%(uint64_t)(FXV_W-30));
        if(!fxv_idle)rect(p,sx,base-20,1,24,WHITE);
    }
}
static void fxv_shelf(const struct resample_painter*p,int x,int w,unsigned seed)
{
    const uint32_t WOOD=0xff7a4a26u,WOOD2=0xff5c3819u;
    rect(p,x,FXV_Y,w,FXV_H,WOOD2);
    for(int row=0;row<2;++row){
        int y=FXV_Y+3+row*45,h=42;
        rect(p,x+3,y,w-6,h,0xff1d1410u);
        int sx=x+4;
        while(sx<x+w-7){
            seed=seed*1664525u+1013904223u;
            int sw=2+(int)(seed>>20&1),sh=h-6-(int)(seed>>22&7);
            static const uint32_t col[6]={0xffc23a2eu,0xffd9c9a0u,0xff2f6f9fu,0xffe0a030u,0xff3a3f46u,0xff5a8a4au};
            uint32_t c=col[(seed>>12)%6];
            if((seed>>26&7)==0){sx+=3;continue;}  /* a gap now and then */
            rect(p,sx,y+h-sh,sw,sh,c);rect(p,sx,y+h-sh,sw,1,0xff000000u|((c>>1)&0x7f7f7fu));
            sx+=sw+1;
        }
        rect(p,x+3,y+h,w-6,2,WOOD);
    }
}
extern unsigned step_native_transport_state(void) __attribute__((weak));
static struct {uint32_t identity;uint64_t last_ms;double angle;int initialized,running;} fxv_vinyl_state;
static void fxv_vinyl(const struct resample_painter*p,const struct fx_stage*stage,uint32_t identity,uint64_t now_ms)
{
    if(!fxv_vinyl_state.initialized||fxv_vinyl_state.identity!=identity||now_ms<fxv_vinyl_state.last_ms){
        memset(&fxv_vinyl_state,0,sizeof(fxv_vinyl_state));fxv_vinyl_state.initialized=1;
        fxv_vinyl_state.identity=identity;fxv_vinyl_state.last_ms=now_ms;
    }
    uint64_t elapsed=now_ms-fxv_vinyl_state.last_ms;fxv_vinyl_state.last_ms=now_ms;
    if(elapsed>100)elapsed=100;
    unsigned transport=step_native_transport_state?step_native_transport_state():0;
    fxv_vinyl_state.running=!fxv_idle&&(transport&3u)==3u;
    if(fxv_vinyl_state.running)fxv_vinyl_state.angle=fmod(fxv_vinyl_state.angle+elapsed*.0035*(1+(fxv_p(stage,2)-.5)*.3),6.283185307);
    const int TW=188,x0=FXV_X+(FXV_W-TW)/2;
    fxv_shelf(p,FXV_X,x0-FXV_X-2,7u);fxv_shelf(p,x0+TW+2,FXV_X+FXV_W-(x0+TW+2),19u);
    rect(p,x0,FXV_Y,TW,FXV_H,0xffc3c7cdu);
    rect(p,x0,FXV_Y+FXV_H-2,TW,2,0xff8f949bu);
    for(int row=4;row<FXV_H-3;row+=4)rect(p,x0+3,FXV_Y+row,TW-6,1,0xffb8bec5u);
    rect(p,x0+2,FXV_Y+2,TW-4,1,0xffeef1f4u);
    int cx=x0+54,cy=FXV_Y+47;
    fxv_disc(p,cx,cy,44,0xff4a4f56u);fxv_disc(p,cx,cy,42,0xff101114u);
    for(int r=18;r<=40;r+=3)for(int a=0;a<48;++a){double t=a*.1309;rect(p,cx+(int)lround(cos(t)*r),cy+(int)lround(sin(t)*r),1,1,0xff23252au);}
    for(int k=0;k<60;++k){double a=k*6.283185307/60;
        rect(p,cx+(int)lround(cos(a)*43),cy+(int)lround(sin(a)*43),1,1,0xffd3d9dfu);}
    for(int r=23;r<41;r+=4)for(int k=0;k<14;++k){double a=-1.15+k*.055;
        rect(p,cx+(int)lround(cos(a)*r),cy+(int)lround(sin(a)*r),1,1,0xff444a52u);}
    double ang=fxv_vinyl_state.angle;
    for(int k=0;k<2;++k){double t=ang+k*3.1416;
        for(int r=20;r<=38;r+=2)rect(p,cx+(int)lround(cos(t)*r),cy+(int)lround(sin(t)*r),1,1,0xff5a5f68u);}
    fxv_disc(p,cx,cy,14,0xffc23a2eu);
    text(p,"AM MOD",cx-14,cy-9,28,9,7,0xfff0e8d0u,1);text(p,"33 RPM",cx-14,cy+1,28,9,6,0xfff0e8d0u,1);
    fxv_disc(p,cx,cy,2,0xffc3c7cdu);
    /* Tonearm as on a real deck: the pivot just outside the record's rim at the top right, the arm about 1.4 radii long,
     * the head resting on the grooves. */
    int px=cx+51,py=cy-34;
    rect(p,px-8,py-13,16,26,0xff2a2d33u);rect(p,px-5,py-10,10,20,0xff3f444cu);
    fxv_disc(p,px,py,5,0xffd9dce2u);
    double wob=sin(ang*(.004/.0035))*.02;
    double hx=cx+26,hy=cy+16+wob*20;
    fxv_line(p,px+1,py+2,hx+1,hy+2,3,0xff737b85u);
    fxv_line(p,px,py,hx,hy,2,0xffe4e7ecu);
    rect(p,px+6,py+14,10,2,0xff4c535cu);rect(p,px+12,py+9,2,8,0xff6c7580u);
    fxv_line(p,px,py,px+5,py-9,4,0xff3a3f46u);
    for(int k=0;k<5;++k)rect(p,(int)hx-5+k,(int)hy-1+k,6,3,0xff101114u);
    fxv_disc(p,x0+TW-34,FXV_Y+34,9,0xffe2e5eau);fxv_disc(p,x0+TW-34,FXV_Y+34,6,0xff9aa0a8u);
    fxv_disc(p,x0+TW-34,FXV_Y+64,9,0xffe2e5eau);fxv_disc(p,x0+TW-34,FXV_Y+64,6,0xff9aa0a8u);
    for(int k=0;k<2;++k){int nx=x0+TW-34,ny=FXV_Y+34+k*30;
        double a=-2.2+4.4*fxv_p(stage,k?0:2);
        fxv_line(p,nx,ny,nx+sin(a)*6,ny-cos(a)*6,1,0xff30363du);}
    text(p,"PITCH",x0+TW-52,FXV_Y+44,36,9,6,0xff4a4f56u,1);text(p,"TONE",x0+TW-52,FXV_Y+74,36,9,6,0xff4a4f56u,1);
    int sx=x0+TW-12,sy=FXV_Y+20;
    rect(p,sx-2,sy,5,45,0xff8f979fu);rect(p,sx,sy+2,1,41,0xff313940u);
    for(int k=0;k<7;++k)rect(p,sx-5,sy+3+k*6,2,1,0xff646e79u);
    int slider=sy+3+(int)lround(fxv_p(stage,2)*36);
    rect(p,sx-3,slider,7,4,0xffe7ebefu);rect(p,sx-2,slider+1,5,1,0xff333d48u);
    text(p,"33 / 45",x0+107,FXV_Y+FXV_H-11,46,8,6,0xff3a434cu,1);
    for(int k=0;k<4;++k)fxv_screw(p,x0+(k&1?TW-5:5),FXV_Y+(k&2?FXV_H-5:5),0xffe2e6eau);
    fxv_disc(p,x0+12,FXV_Y+FXV_H-14,7,0xff5a5f68u);fxv_disc(p,x0+12,FXV_Y+FXV_H-14,2,0xffd9dce2u);
    unsigned seed=(unsigned)(ang*200);int sparks=fxv_vinyl_state.running?(int)lround(6*fxv_p(stage,1)):0;
    for(int k=0;k<sparks;++k){seed=seed*1664525u+1013904223u;rect(p,hx-6+(int)(seed>>9&15),hy+6+(int)(seed>>13&7),2,2,AMBER);}
}
static struct {double ang,needle;uint64_t last_ms,hold_ms;} fxv_tape_state;
static void fxv_tape_emu(const struct resample_painter*p,const struct fx_stage*stage,const struct mixer_fx_monitor*m)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,0xff1d2126u);
    int bx=FXV_X+74,by=FXV_Y+1,bw=148,bh=92;double wow=fxv_p(stage,0);
    rect(p,bx,by,bw,bh,0xffe6dcc0u);rect(p,bx,by,bw,2,0xfffff4d8u);rect(p,bx,by+bh-2,bw,2,0xffb8ad90u);
    rect(p,bx+6,by+4,bw-12,22,0xffc23a2eu);
    text(p,"AM MOD  C-60",bx+10,by+5,100,12,9,0xfff6eedcu,0);
    text(p,"SIDE A",bx+10,by+16,40,9,7,0xfff6eedcu,0);text(p,"NR",bx+bw-30,by+16,20,9,7,0xfff6eedcu,0);
    int wx=bx+20,wy=by+30,ww=108,wh=32;
    rect(p,wx,wy,ww,wh,0xff0d1013u);
    float level=!fxv_idle&&m?m->output_peak:0.f;
    if(level>.004f)fxv_tape_state.hold_ms=fxv_now;
    int playing=!fxv_idle&&fxv_now-fxv_tape_state.hold_ms<350;
    uint64_t dt=fxv_now>fxv_tape_state.last_ms?fxv_now-fxv_tape_state.last_ms:0;if(dt>200)dt=0;
    fxv_tape_state.last_ms=fxv_now;
    if(playing)fxv_tape_state.ang-=dt*.0026*(1+.15*wow*sin(fxv_now*.012));
    double ang=fxv_tape_state.ang;
    double target=0;if(level>0){double db=mixer_meter_dbfs(level);target=fmax(0.0,fmin(1.0,(db+40.0)/40.0));}
    if(fxv_idle)fxv_tape_state.needle=0;
    else fxv_tape_state.needle+=(target-fxv_tape_state.needle)*(target>fxv_tape_state.needle?.5:.12);
    int lcx=wx+24,rcx=wx+ww-24,cy=wy+wh/2;
    fxv_disc_clip(p,lcx,cy,21,wy+1,wy+wh-1,0xff4a3524u);
    fxv_disc_clip(p,rcx,cy,11,wy+1,wy+wh-1,0xff4a3524u);
    fxv_reel(p,lcx,cy,8,8,ang,0xff4a3524u);fxv_reel(p,rcx,cy,8,8,ang,0xff4a3524u);

    for(int k=0;k<3;++k){double a=ang+k*2.0944;
        rect(p,lcx+(int)lround(cos(a)*4)-1,cy+(int)lround(sin(a)*4)-1,2,2,0xffd9dce2u);
        rect(p,rcx+(int)lround(cos(a)*4)-1,cy+(int)lround(sin(a)*4)-1,2,2,0xffd9dce2u);}
    rect(p,bx+30,by+bh-24,bw-60,18,0xff8e856e);
    rect(p,bx+40,by+bh-18,8,6,0xff23272d);rect(p,bx+bw-48,by+bh-18,8,6,0xff23272d);
    /* Moulded case, leader guides and the five lower shell apertures. */
    rect(p,bx+4,by+27,bw-8,1,0xffb7ad92u);rect(p,bx+5,by+28,1,34,0xfffff2d1u);
    rect(p,wx-2,wy-2,ww+4,1,0xff9e957du);rect(p,wx-2,wy-1,1,wh+2,0xffb3a98cu);
    rect(p,wx+1,wy+1,ww-2,1,0xff606a6cu);rect(p,wx+1,wy+wh-2,ww-2,1,0xff485050u);
    for(int k=0;k<9;++k)rect(p,bx+8+k*15,by+28,9,1,0xffa39a81u);
    for(int k=0;k<5;++k){int x=bx+38+k*18;
        fxv_disc(p,x,by+bh-10,k==2?4:3,0xff252b30u);fxv_disc(p,x,by+bh-10,1,0xffb8b29cu);}
    for(int k=0;k<4;++k)fxv_screw(p,bx+(k&1?bw-7:7),by+(k&2?bh-7:7),0xffa89f87u);
    text(p,"NORMAL BIAS  120 us",bx+31,by+63,88,8,6,0xff655d4bu,1);
    rect(p,bx+116,by+6,20,11,0xffeee5cdu);text(p,"60",bx+116,by+6,20,11,8,0xff9b3026u,1);
    fxv_vu(p,FXV_X+7,FXV_Y+20,57,43,fxv_tape_state.needle,"OUT");
    text(p,"LEVEL",FXV_X+8,FXV_Y+66,56,8,7,0xffaeb6bcu,1);
    /* transport at the right: PLAY is down while sound runs, STOP when it has stopped */
    const char*lab[2]={"PLAY","STOP"};
    for(int i=0;i<2;++i){
        int down=i==0?playing:!playing;int y=FXV_Y+14+i*34+(down?2:0);
        uint32_t body=down?(i==0?0xff3aa655u:0xffc23a2eu):0xff4a4f56u;
        rect(p,FXV_X+FXV_W-62,y,54,down?24:26,body);
        if(!down)rect(p,FXV_X+FXV_W-62,y+24,54,2,0xff2b2f34u);
        rect(p,FXV_X+FXV_W-62,y,54,2,down?body:0xff7a808aU);
        text(p,lab[i],FXV_X+FXV_W-62,y+7,54,10,9,0xfff0f0f0u,1);
    }
}
static void fxv_reverb(const struct resample_painter*p,const struct fx_stage*stage,unsigned time_i,unsigned size_i,unsigned mix_i)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    int cx=FXV_X+FXV_W/2,cy=FXV_Y+38;
    double time=fxv_p(stage,time_i),size=size_i==99?1.0:.4+.6*fxv_p(stage,size_i),mix=fxv_p(stage,mix_i);
    int rings=3+(int)lround(9*time);                  /* longer Time = more reflections */
    double cycle=1400+2600*time,pulse=fmod(fxv_now/cycle,1.0);
    for(int k=0;k<rings;++k){
        double u=(k+1.0)/rings,fade=1.0-.85*(double)k/rings;
        double hit=fxv_idle?0.0:fmax(0.0,1.0-fabs(u-pulse)*rings*.7);
        double v=fmin(1.0,fade*(.45+.55*mix)+hit*.5);
        int hw=(int)lround(135*u*size),hh=(int)lround(34*u*size);
        uint32_t c=0xff000000u|(((unsigned)lround(0x71*v+8*(1-v))&255)<<16)|(((unsigned)lround(0xc1*v+13*(1-v))&255)<<8)|((unsigned)lround(0xe8*v+18*(1-v))&255);
        rect(p,cx-hw,cy-hh,2*hw+1,1,c);rect(p,cx-hw,cy+hh,2*hw+1,1,c);
        rect(p,cx-hw,cy-hh,1,2*hh+1,c);rect(p,cx+hw,cy-hh,1,2*hh+1,c);
    }
    fxv_disc(p,cx,cy,4,AMBER);
    int bw=FXV_W-8;
    rect(p,FXV_X+4,FXV_Y+FXV_H-30,bw,3,DARK);rect(p,FXV_X+4,FXV_Y+FXV_H-30,(int)lround(bw*time),3,AMBER);
    rect(p,FXV_X+4,FXV_Y+FXV_H-26,bw,3,DARK);rect(p,FXV_X+4,FXV_Y+FXV_H-26,(int)lround(bw*mix),3,CYAN);
}
static void fxv_spring(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,0xff15181cu);
    rect(p,FXV_X+6,FXV_Y+4,FXV_W-12,FXV_H-10,0xff3a3f46u);rect(p,FXV_X+8,FXV_Y+6,FXV_W-16,FXV_H-14,0xff1c2025u);
    for(int k=0;k<4;++k)rect(p,FXV_X+(k&1?FXV_W-14:9)+0,FXV_Y+(k&2?FXV_H-12:7),3,3,0xff9aa0a8u);
    rect(p,FXV_X+14,FXV_Y+10,5,FXV_H-22,0xff6a4a30u);rect(p,FXV_X+FXV_W-19,FXV_Y+10,5,FXV_H-22,0xff6a4a30u);
    double time=fxv_p(stage,1);int width=FXV_W-52;
    int pulse=fxv_idle?-40:(int)((fxv_now/(6+(int)(14*time)))%(uint64_t)(width+40))-20;
    for(int row=0;row<2;++row){
        int cy=FXV_Y+28+row*32,prev=cy;
        rect(p,FXV_X+20,cy,7,1,0xff9ba4adu);rect(p,FXV_X+FXV_W-27,cy,7,1,0xff9ba4adu);
        for(unsigned side=0;side<2;++side){int xx=FXV_X+(side?FXV_W-24:20);
            fxv_disc(p,xx,cy,4,0xff697581u);fxv_disc(p,xx,cy,2,0xff151c23u);}
        for(int x=0;x<width;x+=2){
            double tri=fabs(fmod(x/14.0+row*.5,1.0)*2-1)*2-1;
            double gain=1.0+.9*exp(-pow((x-pulse)/22.0,2.0));
            int y=cy+(int)lround(8*gain*tri);
            rect(p,FXV_X+26+x,y<prev?y:prev,2,abs(y-prev)+2,0xffc2c8d0u);prev=y;
        }
    }
    text(p,"IN",FXV_X+10,FXV_Y+FXV_H-12,20,9,8,GREY,0);text(p,"OUT",FXV_X+FXV_W-34,FXV_Y+FXV_H-12,26,9,8,GREY,0);
}

/* ---- Drive, modulation, emulators. ---- */
static void fxv_knob(const struct resample_painter*p,int cx,int cy,int r,float v,uint32_t body)
{
    fxv_disc(p,cx+1,cy+1,r,0xff080b10u);fxv_disc(p,cx,cy,r,0xff69737du);
    if(r>3)fxv_disc(p,cx,cy,r-1,body);
    rect(p,cx-r+2,cy-r+2,2,1,0xfff2e4bdu);
    double a=(-135.0+270.0*fmax(0.f,fmin(1.f,v)))*3.14159265/180.0;
    for(int k=2;k<=r;k+=2)rect(p,cx+(int)lround(sin(a)*k)-1,cy-(int)lround(cos(a)*k)-1,2,2,BLACK);
}
static void fxv_amp(const struct resample_painter*p,const struct fx_stage*stage,const struct mixer_fx_monitor*m)
{
    unsigned voice=fx_amp_model_index(stage);
    static const uint32_t shell[8]={0xff352c23u,0xff141b23u,0xff3c2b20u,0xff121517u,0xff161b20u,0xff172329u,0xff47392bu,0xff161e25u};
    static const uint32_t panel[8]={0xff292721u,0xff171e27u,0xffa18348u,0xffb49b5cu,0xffa4aeb8u,0xff172a35u,0xffd3b877u,0xff25313bu};
    static const uint32_t cloth[8]={0xff574a32u,0xff89929cu,0xff554334u,0xff33373bu,0xff242b30u,0xff3f4d55u,0xff6d5942u,0xff283b48u};
    const int x=FXV_X+15,y=FXV_Y+2,w=FXV_W-30,h=FXV_H-7;
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,0xff0c1014u);
    rect(p,x,y,w,h,shell[voice]);rect(p,x+1,y+1,w-2,1,0xff62605au);
    for(int row=4;row<h-3;row+=4)for(int col=3;col<w-3;col+=8)
        rect(p,x+col,y+row,2,1,voice==2?0xff5a4230u:0xff252b30u);
    rect(p,x+7,y+4,w-14,26,panel[voice]);rect(p,x+8,y+5,w-16,1,voice==4?0xffe4e9edu:0xffc4b282u);
    if(voice==4)for(int row=0;row<4;++row)for(int col=0;col<27;++col){
        int xx=x+10+col*9,yy=y+8+row*6;fxv_line(p,xx,yy,xx+3,yy+2,1,0xff6e7b87u);fxv_line(p,xx,yy+2,xx+3,yy,1,0xffd3dbe2u);}
    static const unsigned index[5]={0,4,5,7,8};static const char*label[5]={"DRIVE","BASS","MID","TREB","OUT"};
    for(unsigned k=0;k<5;++k){int cx=x+27+(int)k*50;
        uint32_t knob=voice==1||voice==4||voice==5||voice==7?0xff343b44u:voice==2?0xffeee2b6u:0xffd7bd78u;
        fxv_knob(p,cx,y+14,6,fxv_p(stage,index[k]),knob);
        text(p,label[k],cx-17,y+22,34,7,6,(voice==1||voice==5||voice==7)?0xffe5e9edu:voice==4?0xff202b34u:0xff25241du,1);
    }
    int gx=x+8,gy=y+34,gw=w-16,gh=h-40;
    if(voice>=5)for(int k=0;k<14;++k)rect(p,gx+12+k*17,y+31,9,1,0xff83929eu);
    rect(p,gx,gy,gw,gh,cloth[voice]);
    unsigned cab=(unsigned)lroundf(fxv_p(stage,9)*10);
    int speakers=cab==6||cab==7||cab==9?4:cab==1||cab==4||cab==5?2:1;
    for(int k=0;k<speakers;++k){int cx=gx+(speakers==1?gw/2:(k%2?3:1)*gw/4);
        int cy=gy+(speakers==4?(k/2?3:1)*gh/4:gh/2),radius=speakers==4?gh/4-1:gh/2-2;
        fxv_disc(p,cx,cy,radius,voice==1?0xff626d78u:0xff20262cu);
        if(radius>3)fxv_disc(p,cx,cy,radius-3,voice==1?0xff747e88u:0xff2c3238u);
        fxv_disc(p,cx,cy,3,voice==1?0xff88919au:0xff191e23u);
    }
    if(voice==2){ /* Diagonal diamond cloth distinguishes the chime combo. */
        for(int start=-gh;start<gw+gh;start+=10){
            int ay=start<0?-start:0,ax=start+ay;
            int by=gh-1;if(start+by>=gw)by=gw-1-start;
            if(by>=ay)fxv_line(p,gx+ax,gy+ay,gx+start+by,gy+by,1,0xff9a7950u);
            ay=start>=gw?start-gw+1:0;by=start<gh?start:gh-1;
            if(by>=ay)fxv_line(p,gx+start-ay,gy+ay,gx+start-by,gy+by,1,0xff8b6a45u);
        }
    }else{
        for(int yy=gy;yy<gy+gh;yy+=3)rect(p,gx,yy,gw,1,voice==1?0xffafb7bfu:0xff4a4c47u);
        for(int xx=gx+2;xx<gx+gw;xx+=5)rect(p,xx,gy,1,gh,voice==1?0xff6a7580u:0xff272e33u);
    }
    int bw=(int)strlen(fx_amp_model_name(voice))*6+12;
    rect(p,gx+(gw-bw)/2,gy+gh/2-7,bw,14,voice==1?0xff252c34u:0xff12181du);
    text(p,fx_amp_model_name(voice),gx+(gw-bw)/2+2,gy+gh/2-6,bw-4,12,10,voice==1?0xffe8edf2u:0xffe2c989u,1);
    for(int k=0;k<4;++k)fxv_screw(p,x+(k&1?w-4:4),y+(k&2?h-4:5),0xffb7bdc3u);
    rect(p,x+13,y+h,17,3,0xff10151bu);rect(p,x+w-30,y+h,17,3,0xff10151bu);
    float peak=!fxv_idle&&m?m->output_peak:0;
    fxv_disc(p,x+w-15,y+16,3,peak>.01f?(voice==1||voice==7?0xff4ab8ffu:voice==5?0xffffb74au:0xffff5b40u):0xff482a28u);
}
static void fxv_clip_wave(const struct resample_painter*p,int x,int y,int w,int h,double drive,double hard)
{
    rect(p,x,y,w,h,WELL);
    int c=y+h/2,amp=h/2-2;rect(p,x,c,w,1,DARK);
    int pi=c,po=c;
    for(int cx=0;cx<w;cx+=2){
        double s=sin(cx*.11+fxv_now*.004)*(.6+.35*sin(fxv_now*.0013)),g=1+drive*10;
        double v=s*g,o=hard>.5?fmax(-1.0,fmin(1.0,v)):tanh(v);
        int yi=c-(int)lround(amp*s),yo=c-(int)lround(amp*o);
        rect(p,x+cx,yi<pi?yi:pi,2,abs(yi-pi)+1,0xff4a525au);
        rect(p,x+cx,yo<po?yo:po,2,abs(yo-po)+2,AMBER);pi=yi;po=yo;
    }
}
static void fxv_tube(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,PANEL);
    float drive=fxv_p(stage,0);
    for(int k=0;k<2;++k){
        int cx=FXV_X+30+k*52,top=FXV_Y+6,h=FXV_H-24;
        int glow=(int)lround(40+170*drive*(k?.85:1.0));
        uint32_t hot=0xff000000u|((unsigned)(120+glow/2)<<16)|((unsigned)(40+glow/3)<<8)|20u;
        fxv_disc_clip(p,cx,top+14,16,top,top+h,0xff262a30u);rect(p,cx-16,top+14,33,h-14,0xff262a30u);
        fxv_disc_clip(p,cx,top+14,13,top,top+h,0xff15181cu);rect(p,cx-13,top+14,27,h-14,0xff15181cu);
        rect(p,cx-8,top+10,17,h-26,hot);rect(p,cx-5,top+14,11,h-34,0xffffd890u);
        fxv_line(p,cx-11,top+10,cx-11,top+h-19,1,0xff8e969fu);
        for(int g=0;g<5;++g)rect(p,cx-7,top+18+g*5,15,1,0xff915d38u);
        rect(p,cx-16,top+h-12,33,10,0xff80858cu);rect(p,cx-16,top+h-12,33,2,0xffb0b4bau);
    }
    text(p,"AM MOD",FXV_X+8,FXV_Y+FXV_H-12,46,10,7,0xfff8e8d8u,0);
    fxv_clip_wave(p,FXV_X+112,FXV_Y+6,FXV_W-118,FXV_H-14,drive,0);
}
static void fxv_soft_clip(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,0xff12161au);
    /* transfer curve: input on x, output on y, the straight line is no clipping */
    int cx=FXV_X+6,cy=FXV_Y+16,cw=84,ch=70;double drive=fxv_p(stage,0);int hard=fxv_p(stage,1)>.5f;
    rect(p,cx,cy,cw,ch,PANEL);rect(p,cx+cw/2,cy,1,ch,DARK);rect(p,cx,cy+ch/2,cw,1,DARK);
    for(int i=0;i<cw;++i){double x=(i-cw/2)/(cw/2.0),g=1+drive*10,v=x*g,o=hard?fmax(-1.0,fmin(1.0,v)):tanh(v);
        int y=cy+ch/2-(int)lround(o*(ch/2-2));rect(p,cx+i,cy+ch/2-(int)lround(x*(ch/2-2)),1,1,0xff3a424aU);rect(p,cx+i,y,2,2,fxv_acc);}
    fxv_clip_wave(p,FXV_X+100,FXV_Y+16,FXV_W-106,70,drive,hard);
}
static void fxv_ensemble(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,0xff12161au);
    double rate=fxv_p(stage,0),depth=fxv_p(stage,1);
    static const int ys[4]={18,36,54,76};
    for(int v=0;v<4;++v){
        int cy=FXV_Y+ys[v],prev=cy;uint32_t c=v==3?AMBER:(v==0?fxv_acc:v==1?0xff9a66e0u:0xff7a50b8u);
        rect(p,FXV_X+24,cy-10,FXV_W-32,20,PANEL);
        text(p,v==3?"SUM":(v==0?"V1":v==1?"V2":"V3"),FXV_X+4,cy-5,20,10,8,c,0);
        for(int x=0;x<FXV_W-40;x+=2){
            double ph=x*.09+(v<3?v*1.4+sin(fxv_now*.0012*(.4+rate*3)+v)*depth*2.0:0)+fxv_now*.003;
            double val=v<3?sin(ph):(sin(x*.09+fxv_now*.003)+sin(x*.09+1.4+sin(fxv_now*.0012*(.4+rate*3)+1)*depth*2+fxv_now*.003)+sin(x*.09+2.8+sin(fxv_now*.0012*(.4+rate*3)+2)*depth*2+fxv_now*.003))/3;
            int y=cy-(int)lround(7*val);rect(p,FXV_X+28+x,y<prev?y:prev,2,abs(y-prev)+2,c);prev=y;
        }
    }
}
static void fxv_chorus(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,0xff12161au);
    int voices=2+(int)lround(4*fxv_p(stage,5));double rate=fxv_p(stage,0),depth=fxv_p(stage,1);
    int cx=FXV_X+50,cy=FXV_Y+46;
    for(int r=24;r<=40;r+=16)for(int a=0;a<48;++a){double t=a*.1309;rect(p,cx+(int)lround(cos(t)*r),cy+(int)lround(sin(t)*r),1,1,0xff2a323bU);}
    for(int v=0;v<voices&&v<6;++v){
        double a=v*6.2831853/voices+fxv_now*.0012*(.4+rate*3);
        int x=cx+(int)lround(cos(a)*38),y=cy+(int)lround(sin(a)*38);
        for(int k=0;k<=8;++k)rect(p,cx+(x-cx)*k/8,cy+(y-cy)*k/8,1,1,0xff4a3a70U);
        fxv_disc(p,x,y,4,v%2?0xffa070e8U:fxv_acc);
    }
    fxv_disc(p,cx,cy,3,AMBER);text(p,"LFO",cx-14,cy+44,28,9,8,GREY,1);
    for(int v=0;v<voices;++v){
        int ly=FXV_Y+10+(FXV_H-20)*(2*v+1)/(2*voices),prev=ly;uint32_t c=v%2?0xffa070e8U:fxv_acc;
        for(int x=0;x<FXV_W-120;x+=2){int y=ly-(int)lround(6*sin((x+sin(fxv_now*.0012*(.4+rate*3)+v*1.7)*depth*14)*.1+v*1.1+fxv_now*.003));
            rect(p,FXV_X+108+x,y<prev?y:prev,2,abs(y-prev)+2,c);prev=y;}
    }
}
static void fxv_phaser(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    for(int x=FXV_X+18;x<FXV_X+FXV_W-12;x+=32)rect(p,x,FXV_Y+11,1,42,DARK);
    rect(p,FXV_X+4,FXV_Y+54,FXV_W-8,1,LINE);
    double sw=.5+.4*sin(fxv_now*.0015*(.3+3*fxv_p(stage,0))),depth=.3+.7*fxv_p(stage,1),fb=fxv_p(stage,2);
    int prev=FXV_Y+14;
    for(int x=0;x<FXV_W-8;x+=2){
        double u=(double)x/(FXV_W-8),v=0;
        for(int k=0;k<3;++k){double c=(.12+sw*depth*(.28+k*.22));v+=exp(-pow((u-c)/(.025+.02*(1-fb)),2.0));}
        int y=FXV_Y+14+(int)lround(34*fmin(1.0,v));
        rect(p,FXV_X+4+x,y<prev?y:prev,2,abs(y-prev)+2,fxv_acc);prev=y;
    }
    for(int k=0;k<4;++k){int cx=FXV_X+30+k*70;
        fxv_disc(p,cx,FXV_Y+FXV_H-34,8,(fxv_now/200)%4==(uint64_t)k?fxv_acc:0xff50441eu);
        if(k<3){rect(p,cx+9,FXV_Y+FXV_H-35,52,2,GREY);
            rect(p,cx+52,FXV_Y+FXV_H-38,4,1,GREY);rect(p,cx+56,FXV_Y+FXV_H-37,4,1,GREY);}
        char num[2]={(char)('1'+k),0};text(p,num,cx-5,FXV_Y+FXV_H-39,10,9,7,BLACK,1);
    }
}
static void fxv_flanger(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    for(int x=FXV_X+12;x<FXV_X+FXV_W-10;x+=24)rect(p,x,FXV_Y+9,1,52,DARK);
    for(int row=0;row<3;++row)rect(p,FXV_X+4,FXV_Y+14+row*22,FXV_W-8,1,DARK);
    double d=10+9*sin(fxv_now*.0016*(.3+3*fxv_p(stage,0)))*(.4+.6*fxv_p(stage,1)),fb=fxv_p(stage,2);
    int prev=FXV_Y+14;
    for(int x=0;x<FXV_W-8;x+=2){
        double v=.5+.5*cos(x/d*3.14159)*(.3+.7*fb);
        int y=FXV_Y+14+(int)lround(44*(1-v));
        rect(p,FXV_X+4+x,y<prev?y:prev,2,abs(y-prev)+2,fxv_acc);prev=y;
    }
    /* sweep arrow under the comb: the notches slide up and down */
    int sx=FXV_X+60,sw2=FXV_W-120,sy=FXV_Y+FXV_H-12;
    rect(p,sx,sy,sw2,2,0xff4a3a70U);
    int mk=sx+(int)lround((.5+.5*sin(fxv_now*.0016*(.3+3*fxv_p(stage,0))))*(sw2-8));
    rect(p,mk,sy-4,8,10,AMBER);
    text(p,"SWEEP",FXV_X+8,sy-3,48,10,8,GREY,0);
}
static void fxv_auto_wah(const struct resample_painter*p,const struct fx_stage*stage,const struct mixer_fx_monitor*m)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,0xff12161au);
    double level=m?fmin(1.0,m->input_peak*1.5):.5,centre=.15+.7*fxv_p(stage,13)+.3*(level-.5)*fxv_p(stage,9),res=fxv_p(stage,8);
    centre=fmax(.08,fmin(.92,centre));
    /* the pedal rocks with the sweep: heel down = low centre, toe down = high */
    int px=FXV_X+46,py=FXV_Y+FXV_H-8;double tilt=(centre-.5)*.9;
    for(int row=0;row<64;++row){
        double t=(double)row/63;int w=(int)lround(22+22*t);
        int shift=(int)lround(tilt*(t-.5)*0)+(int)lround(tilt*(.5-t)*10);
        rect(p,px-w/2+shift,py-row-8,w,1,row%6<2?0xff1e2226U:0xff6a6e76U);
    }
    rect(p,px-24,py-6,48,6,0xff3a3f46U);fxv_disc(p,px,py-3,2,fxv_acc);
    int gx=FXV_X+96,gw=FXV_W-102,gy=FXV_Y+10,gh=FXV_H-24;
    rect(p,gx,gy,gw,gh,PANEL);
    for(int i=1;i<5;++i)rect(p,gx+gw*i/5,gy,1,gh,0xff1c222aU);
    int prev=gy+gh;
    for(int x=0;x<gw;x+=2){double u=(double)x/gw;
        double v=.12+(.5+.5*res)*exp(-pow((u-centre)/(.14-.09*res),2.0));
        int y=gy+gh-(int)lround((gh-6)*fmin(1.0,v));
        rect(p,gx+x,y,2,gy+gh-y,0xff2a2438U);rect(p,gx+x,y<prev?y:prev,2,abs(y-prev)+2,fxv_acc);prev=y;}
    rect(p,gx+(int)lround(centre*gw),gy,1,gh,WHITE);
}
static void fxv_auto_pan(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    double pos=sin(fxv_now*.0016*(.2+4*fxv_p(stage,8)));
    for(int side=0;side<2;++side){
        int cx=FXV_X+(side?FXV_W-44:44),cy=FXV_Y+40;double lev=side?(1+pos)/2:(1-pos)/2;
        rect(p,cx-22,FXV_Y+8,44,64,0xff2c3036u);
        fxv_disc(p,cx,cy,14,0xff121416u);
        uint32_t c=0xff000000u|((unsigned)lround(0x30+0xc0*lev)<<16)|((unsigned)lround(0x28+0x9a*lev)<<8)|(unsigned)lround(0x10+0x37*lev);
        fxv_disc(p,cx,cy,8,c);
        text(p,side?"R":"L",cx-6,FXV_Y+9,12,11,10,fxv_acc,1);
    }
    int ox=FXV_X+FXV_W/2+(int)lround(pos*80);
    fxv_disc(p,ox,FXV_Y+40,7,fxv_acc);
    rect(p,FXV_X+70,FXV_Y+FXV_H-28,FXV_W-140,2,GREY);
}

/* ---- Am MOD's own effects (fx_own.c). Parameters are normalized 0..1 as in the DSP. ---- */
/* OptoCat: the Sample compressor's cat (same sprites and motion as Shift+Pad 5), scaled into the art area,
 * with the real-unit transfer curve and the level history next to it. */
static struct comp_animation fxv_cat;static int fxv_cat_ready;
static uint32_t fxv_cat_frame[320*164];
#define FXV_CAT_W 99
#define FXV_CAT_H 90
static void fxv_cat_scene(const struct resample_painter*p,const struct comp_config*c,float input_db,float reduction,int x0,int y0)
{
    if(!fxv_cat_ready){compressor_animation_init(&fxv_cat);fxv_cat_ready=1;}
    fxv_cat.reduction=fmaxf(0.f,reduction);
    compressor_animation_frame(&fxv_cat,fxv_cat_frame,(double)fxv_now/1000.0,input_db,c,0);
    rect(p,x0,y0,FXV_CAT_W,FXV_CAT_H,BLACK);
    /* Source window 164x150 at (92,12) -> 99x90, box-averaged, 6 grey levels, runs merged per row. */
    const int sx0=92,sy0=12;const double scale=164.0/FXV_CAT_W;
    for(int y=0;y<FXV_CAT_H;++y){
        int ya=sy0+(int)(y*scale),yb=sy0+(int)((y+1)*scale);if(yb<=ya)yb=ya+1;if(yb>164)yb=164;
        int run_start=0,run_level=0;
        for(int x=0;x<=FXV_CAT_W;++x){
            int level=0;
            if(x<FXV_CAT_W){
                int xa=sx0+(int)(x*scale),xb=sx0+(int)((x+1)*scale);if(xb<=xa)xb=xa+1;if(xb>320)xb=320;
                unsigned sum=0,n=0;
                for(int yy=ya;yy<yb;++yy)for(int xx=xa;xx<xb;++xx){sum+=fxv_cat_frame[yy*320+xx]&255;++n;}
                unsigned v=n?sum/n:0;
                level=v<70?0:v<105?1:v<140?2:v<180?3:v<220?4:5;
            }
            if(level!=run_level||x==FXV_CAT_W){
                if(run_level){
                    static const uint32_t shade[6]={0,0xff40454au,0xff6a7075u,0xff9aa0a6u,0xffc8ccd2u,0xfff2f4f6u};
                    rect(p,x0+run_start,y0+y,x-run_start,1,shade[run_level]);
                }
                run_start=x;run_level=level;
            }
        }
    }
}
static void fxv_opto_draw(const struct resample_painter*p,const struct comp_config*c,float input_db,float reduction,
                          const struct mixer_fx_monitor*m)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,PANEL);
    fxv_cat_scene(p,c,input_db,reduction,FXV_X+3,FXV_Y+2);
    const int gx=FXV_X+108,gy=FXV_Y+14,gs=64;
    rect(p,gx,gy,gs,gs,WELL);
    rect(p,gx,gy+gs/2,gs,1,DARK);rect(p,gx+gs/2,gy,1,gs,DARK);
    double slope=1.0-1.0/c->ratio,half=c->knee*.5;
    if(c->knee>0){ /* knee band: the region where the curve bends, wider as Knee grows */
        int k0=(int)lround((fmax(-60.0,c->threshold-half)+60.0)*(gs-1)/60.0),k1=(int)lround((fmin(0.0,c->threshold+half)+60.0)*(gs-1)/60.0);
        if(k1>=k0)rect(p,gx+k0,gy,k1-k0+1,gs,0xff2b2a20u);
    }
    for(int x=0;x<gs;++x){ /* 1:1 reference diagonal, so Ratio shows as the bend away from it */
        int y=gy+gs-1-x;if(y>=gy)rect(p,gx+x,y,1,1,0xff3a4048u);
    }
    for(int x=0;x<gs;++x){
        double in=-60.0+60.0*x/(gs-1),over=in-c->threshold,gr;
        gr=over<=-half?0:over>=half?slope*over:c->knee>0?slope*(over+half)*(over+half)/(2*c->knee):0;
        double out=in-gr+c->gain;
        if(out<-60)out=-60;
        if(out>0)out=0;
        rect(p,gx+x,gy+gs-1-(int)lround((out+60.0)*(gs-1)/60.0),1,2,AMBER);
    }
    rect(p,gx+(int)lround((c->threshold+60.0)*(gs-1)/60.0),gy,1,gs,LINE);
    if(m){
        float di=mixer_meter_dbfs(m->input_peak),dout=mixer_meter_dbfs(m->output_peak);
        if(isfinite(di)&&isfinite(dout)){
            int dx=gx+(int)lround((fmax(-60.f,di)+60.f)*(gs-1)/60.f),dy=gy+gs-1-(int)lround((fmax(-60.f,dout)+60.f)*(gs-1)/60.f);
            rect(p,dx-2,dy-2,5,5,WHITE);
        }
    }
    fxv_title(p,"Transfer",gx,FXV_Y+1,64);
    fxv_title(p,"Level history",FXV_X+180,FXV_Y+1,112);
    fxv_strip(p,FXV_X+180,FXV_Y+14,FXV_W-184,FXV_H-34,1,0,NAN);
    char buf[48];
    snprintf(buf,sizeof(buf),reduction>0?"GR -%.1f dB":"GR 0.0 dB",(double)reduction);
    text(p,buf,gx,FXV_Y+FXV_H-18,70,12,10,0xffff9a7cu,0);
    snprintf(buf,sizeof(buf),"%.0f dB  %.1f:1  K%.0f%s",(double)c->threshold,(double)c->ratio,(double)c->knee,c->warm?" W":"");
    text(p,buf,FXV_X+180,FXV_Y+FXV_H-18,FXV_W-184,12,10,WHITE,0);
}
static void fxv_opto(const struct resample_painter*p,const struct fx_stage*stage,const struct mixer_fx_monitor*m)
{
    struct comp_config c;fx_own_opto_config(stage->parameter,&c);c.bypass=stage->bypass;
    float di=m?mixer_meter_dbfs(m->input_peak):-60.f;if(!isfinite(di))di=-60.f;
    fxv_opto_draw(p,&c,di,m?m->gain_reduction_db:0.f,m);
}
static void fxv_shimmer(const struct resample_painter*p,const struct fx_stage*stage,const struct mixer_fx_monitor*m)
{
    double decay=fxv_p(stage,1),amount=fxv_p(stage,2),size=.45+.55*fxv_p(stage,4),damp=fxv_p(stage,5);
    int pitch=fx_own_shimmer_semitones(fxv_p(stage,3));
    int cx=FXV_X+FXV_W/2,vanish=FXV_Y+17,base=FXV_Y+FXV_H-15;
    for(int row=0;row<FXV_H;row+=2){unsigned glow=(unsigned)(12+16*(double)row/FXV_H);
        rect(p,FXV_X,FXV_Y+row,FXV_W,2,0xff000000u|(glow/2<<16)|(glow/2<<8)|(glow+9));}
    /* Receding hall reflections; room Size changes the spread and decay the layers. */
    for(int layer=5;layer>0;--layer){
        double sc=(.12+layer*.15)*size;int hw=(int)(132*sc),hh=(int)(41*sc),cy=vanish+hh+5;
        uint32_t ink=0xff000000u|((unsigned)(22+layer*4)<<16)|((unsigned)(38+layer*8)<<8)|(unsigned)(62+layer*13);
        fxv_line(p,cx-hw,base,cx-hw,cy,1,ink);fxv_line(p,cx+hw,base,cx+hw,cy,1,ink);
        for(int k=0;k<24;++k){double a=3.14159265*k/23;
            int xx=cx+(int)lround(cos(a)*hw),yy=cy-(int)lround(sin(a)*hh);
            rect(p,xx,yy,2,2,ink);
        }
        fxv_line(p,cx-hw,base,cx+hw,base,1,ink);
    }
    for(int k=-3;k<=3;++k)fxv_line(p,cx,vanish+15,cx+k*45,base,1,0xff24333eu);
    int ribbons=3+(int)lround(4*amount);
    double phase=fxv_now/(2400.+4800.*decay);
    for(int k=0;k<ribbons;++k){int prevx=cx,prevy=base;
        for(int t=0;t<=36;++t){double u=t/36.,flow=phase*(pitch<0?-1:1);
            int xx=cx+(int)lround(sin(u*5+k*1.9-flow*6.2831853)*(12+75*u)*size);
            int yy=base-(int)lround(u*(FXV_H-25));
            uint32_t halo=k%2?0xff27344fu:0xff20454fu;
            uint32_t ink=k%2?0xff8c90c4u:0xff69bdc5u;
            fxv_line(p,prevx,prevy,xx,yy,3,halo);
            fxv_line(p,prevx,prevy,xx,yy,1,ink);prevx=xx;prevy=yy;
        }
    }
    /* Deterministic particles and their fading multi-octave tails. */
    unsigned rnd=2463534242u;int count=9+(int)lround(22*amount);
    for(int i=0;i<count;++i){rnd=rnd*1664525u+1013904223u;
        double seed=(rnd&1023)/1024.,u=fmod(phase*(.6+seed*.8)+seed,1.);
        if(pitch<0)u=1-u;
        int xx=cx+(int)lround((seed-.5)*230*size+sin(u*6+seed*20)*10);
        int yy=base-(int)lround(u*(FXV_H-29));
        for(int tail=3;tail>0;--tail){int ty=yy+(pitch<0?-1:1)*tail*2;
            if(ty>FXV_Y+5&&ty<base)rect(p,xx,ty,1,2,tail==1?0xff5b8fa8u:0xff30445eu);}
        uint32_t ink=damp>.6?0xffd4af84u:u>.55?0xffdbd8ffu:0xff8ce2e2u;
        rect(p,xx,yy,2,2,ink);
        if(i%4==0){rect(p,xx-1,yy,4,1,ink);rect(p,xx,yy-1,1,4,ink);}
    }
    double peak=!fxv_idle&&m?fmin(1.,m->output_peak):0;
    fxv_disc(p,cx,base,6+(int)(peak*3),0xff344b6bu);fxv_disc(p,cx,base,3,0xffb4e9e6u);
    char label[24];snprintf(label,sizeof label,"PITCH %+d",pitch);
    text(p,label,FXV_X+5,FXV_Y+FXV_H-10,72,9,7,0xffa5bdcdu,0);
    text(p,"SHIMMER TAIL",FXV_X+FXV_W-89,FXV_Y+FXV_H-10,84,9,7,0xffa5bdcdu,1);
}
static void fxv_pitch(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,0xff12161au);
    int st=fx_own_pitch_semitones(stage->parameter[0]);double cents=(fxv_p(stage,1)-.5)*100.0;
    double ratio=pow(2.0,(st+cents/100.0)/12.0);
    for(int lane=0;lane<2;++lane){
        int cy=FXV_Y+16+lane*24;uint32_t c=lane?fxv_acc:0xff7a828aU;int prev=cy;
        rect(p,FXV_X+4,cy,FXV_W-8,1,DARK);
        double f=lane?ratio:1.0;
        for(int x=0;x<FXV_W-60;x+=2){
            int y=cy-(int)lround(9*sin(x*.06*f+fxv_now*.003*f));
            rect(p,FXV_X+40+x,y<prev?y:prev,2,abs(y-prev)+2,c);prev=y;
        }
        text(p,lane?"OUT":"IN",FXV_X+6,cy-6,30,12,10,c,0);
    }
    /* keyboard: the grey marker is the played note, the coloured one is where it comes out */
    int kx=FXV_X+8,ky=FXV_Y+FXV_H-34,kw=11;
    for(int i=0;i<24;++i){int blk=(i%12==1||i%12==3||i%12==6||i%12==8||i%12==10);
        rect(p,kx+i*kw,ky,kw-1,28,blk?0xff24262au:0xffd6d8deU);}
    int a=5,b=5+st;if(b<0)b=0;if(b>23)b=23;
    rect(p,kx+a*kw,ky-6,kw-1,4,0xff9aa0a8U);rect(p,kx+b*kw,ky-6,kw-1,4,fxv_acc);
}
static void fxv_reverse(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,PANEL);
    double time=.08+1.42*fxv_p(stage,0),fb=.85*fxv_p(stage,1);
    int span=FXV_W-24,base=FXV_Y+FXV_H-24;
    int hits=1+(int)lround(fb*3);
    for(int e=0;e<hits;++e){
        double amp=pow(fb+.15,e);
        for(int x=0;x<span/ (hits>1?2:1);x+=3){
            double u=(double)x/(span/(hits>1?2:1));
            int h=(int)lround(52*amp*u*u);if(h<1)h=1;
            rect(p,FXV_X+10+e*(span/(hits+1))+x,base-h,2,h,e?0xff7a5f22u:AMBER);
        }
        rect(p,FXV_X+10+e*(span/(hits+1))+span/(hits>1?2:1),base-(int)lround(52*amp),2,(int)lround(52*amp),WHITE);
    }
    rect(p,FXV_X+10,base,span,1,DARK);
    int px=FXV_X+10+(int)((fxv_now/(int)(8+time*10))%(uint64_t)span);
    if(!fxv_idle)rect(p,px,FXV_Y+8,1,FXV_H-34,GREY);
    text(p,"<",FXV_X+FXV_W-30,FXV_Y+2,20,16,14,AMBER,0);
}
static void fxv_tremolo(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    double hz=.5*pow(28.0,fxv_p(stage,0)),depth=fxv_p(stage,1),shape=fxv_p(stage,2);
    int cy=FXV_Y+44,prevt=cy,prevw=cy;
    double phase=fxv_now*.001*hz*6.2831853;
    for(int x=0;x<FXV_W-16;x+=2){
        double t=phase-x*.035;double lfo=sin(t),sq=tanh(lfo*(1+14*shape))/tanh(1+14*shape);
        double u=.5+.5*(lfo*(1-shape)+sq*shape),g=1-depth*(1-u);
        int top=cy-(int)lround(34*g);
        rect(p,FXV_X+8+x,top<prevt?top:prevt,2,abs(top-prevt)+2,fxv_acc);
        int bot=cy+(int)lround(34*g);rect(p,FXV_X+8+x,bot<prevw?bot:prevw,2,abs(bot-prevw)+2,0xff7a5f22u);
        int w=cy-(int)lround(34*g*sin(x*.55));
        rect(p,FXV_X+8+x,w,2,2,0xff4a525au);
        prevt=top;prevw=bot;
    }
}
static void fxv_swell(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    double attack=.02*pow(100.0,fxv_p(stage,0));
    int base=FXV_Y+FXV_H-26,hgt=52,w=FXV_W-24;
    int prevo=base,previ=base;
    double note=.12,span=3.0;
    for(int x=0;x<w;x+=2){
        double t=(double)x/w*span;
        double in=t>=note&&t<span-.4?1.0:0.0,out=t<note?0.0:fmin(1.0,(t-note)/attack);if(t>=span-.4)out=0;
        int yi=base-(int)lround(hgt*in),yo=base-(int)lround(hgt*out);
        rect(p,FXV_X+12+x,yi<previ?yi:previ,2,abs(yi-previ)+2,0xff5a626aU);
        rect(p,FXV_X+12+x,yo<prevo?yo:prevo,2,abs(yo-prevo)+2,fxv_acc);previ=yi;prevo=yo;
    }
    int px=FXV_X+12+(int)((fxv_now/14)%(uint64_t)w);
    if(!fxv_idle)rect(p,px,FXV_Y+8,1,FXV_H-34,WHITE);
}
static void fxv_ring(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    double fc=20.0*pow(250.0,fxv_p(stage,0));double car=fmin(.9,.06+fc/2200.0);
    static const char*names[3]={"IN","CARRIER","OUT"};
    for(int lane=0;lane<3;++lane){
        int cy=FXV_Y+16+lane*26;uint32_t c=lane==0?0xff7a828aU:lane==1?CYAN:fxv_acc;int prev=cy;
        rect(p,FXV_X+4,cy,FXV_W-8,1,DARK);
        for(int x=0;x<FXV_W-70;x+=2){
            double a=sin(x*.07+fxv_now*.003),b=sin(x*car+fxv_now*.004);
            double v=lane==0?a:lane==1?b:a*b;
            int y=cy-(int)lround(9*v);rect(p,FXV_X+60+x,y<prev?y:prev,2,abs(y-prev)+2,c);prev=y;
        }
        text(p,names[lane],FXV_X+4,cy-6,54,12,9,c,0);
    }
}
static void fxv_crush(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    int bits=2+(int)lround(14*fxv_p(stage,0));int factor=(int)lround(pow(32.0,fxv_p(stage,1)));if(factor<1)factor=1;
    double levels=pow(2.0,bits-1);
    for(int l=-(int)levels;levels<=16&&l<=(int)levels;++l)rect(p,FXV_X+8,FXV_Y+40-(int)lround(34*l/levels),FXV_W-16,1,0xff1a222aU);
    int cy=FXV_Y+40,amp=34,prevo=cy,previ=cy;int hold=0;double held=0;
    for(int x=0;x<FXV_W-16;x+=2){
        double s=sin(x*.06+fxv_now*.003)*.9;
        if(hold==0)held=floor(s*levels+.5)/levels;
        hold=(hold+1)%(factor>8?factor/2:factor);
        int yi=cy-(int)lround(amp*s),yo=cy-(int)lround(amp*held);
        rect(p,FXV_X+8+x,yi<previ?yi:previ,2,abs(yi-previ)+1,0xff4a525au);
        rect(p,FXV_X+8+x,yo<prevo?yo:prevo,2,abs(yo-prevo)+2,fxv_acc);
        previ=yi;prevo=yo;
    }
}
/* ---- Henry Skillz effects (fx_hs.c): simple live pictures, credited in the corner. ---- */
static void fxv_hs_credit(const struct resample_painter*p)
{
    text(p,"HENRY SKILLZ",FXV_X+FXV_W-76,FXV_Y+FXV_H-12,72,10,7,0xff6a7480U,0);
}
/* Sine through the effect's clip: drive pushes it into the rails, the accent trace is the output. */
static void fxv_hs_drive(const struct resample_painter*p,const struct fx_stage*stage,double drive,double mix)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    int cy=FXV_Y+FXV_H/2-4,amp=FXV_H/2-12;
    rect(p,FXV_X+8,cy,FXV_W-16,1,0xff1a222aU);
    double gain=1.0+drive*7.0;int prevo=cy,previ=cy;(void)stage;
    for(int x=0;x<FXV_W-16;x+=2){
        double s=sin(x*.07+fxv_now*.002)*.85,o=s*gain;o=o/(1.0+fabs(o));
        o=s*(1-mix)+o*mix*(1.0+.15*drive);if(o>1)o=1;if(o<-1)o=-1;
        int yi=cy-(int)lround(amp*s),yo=cy-(int)lround(amp*o);
        rect(p,FXV_X+8+x,yi<previ?yi:previ,2,abs(yi-previ)+1,0xff4a525aU);
        rect(p,FXV_X+8+x,yo<prevo?yo:prevo,2,abs(yo-prevo)+2,fxv_acc);
        previ=yi;prevo=yo;
    }
    fxv_hs_credit(p);
}
static void fxv_hs_saturator(const struct resample_painter*p,const struct fx_stage*stage)
{fxv_hs_drive(p,stage,fxv_p(stage,0),fxv_p(stage,2));}
static double fxv_heat_clip(int circuit,double x,double g)
{
    switch(circuit){
    case 4:case 5:{double o=x*g;return o>1?1:o<-1?-1:o;}
    case 6:return tanh(g*x+.3)-tanh(.3);
    case 7:return tanh(2.0*tanh(g*x));
    default:return tanh(g*x);}
}
static double fxv_heat_filter_db(int type,double hz,double fc,double res)
{
    double r=hz/fc,q=.7+res*7.0,m=1.0;
    switch(type){
    case 1:m=1.0/sqrt((1-r*r)*(1-r*r)+(r/q)*(r/q));break;
    case 2:m=1.0/sqrt(1+r*r);break;
    case 3:{double d=q*(r-1.0/r);m=1.0/sqrt(1+d*d);break;}
    case 4:m=r/sqrt(1+r*r);break;
    case 5:m=r*r/sqrt((1-r*r)*(1-r*r)+(r/q)*(r/q));break;
    case 6:{double d=q*(r-1.0/r);m=fabs(d)/sqrt(1+d*d);break;}
    case 7:{double d=q*(r-1.0/r);m=1.0+(1.0+3.0*res)/(1.0+d*d);m*=.5;break;}
    default:break;}
    return 20.0*log10(fmax(m,1e-3));
}
static double fxv_heat_wave(int w,double t)
{
    t-=floor(t);
    switch(w){
    case 0:return t<.5?4*t-1:3-4*t;
    case 1:return sin(6.2831853*t);
    case 2:return t<.5?1:-1;
    case 3:return 1-2*t;
    case 4:return 2*exp(-4*t)-1;
    case 5:return 2*t-1;
    default:{double h=sin(t*43.7)*9573.1;return 2*(h-floor(h))-1;}}
}
/* Faceplate-style panel: circuit name, its clipper curve, the filter response and the LFO shape. */
static void fxv_hs_heat(const struct resample_painter*p,const struct fx_stage*stage)
{
    static const char*const circuit[8]={"CLEAN BOOST","SATURATION","ENHANCEMENT","MID DRIVE","ROUGH CRUNCH","CLASSIC DIST","ROUND FUZZ","HIGH GAIN"};
    static const char*const ftype[8]={"FILTER OFF","LP 12 dB","LP 6 dB","BAND PASS","HP 6 dB","HP 12 dB","NOTCH","PEAK"};
    static const char*const wave[7]={"TRI","SINE","SQR","SAW","EXP","RAMP","RND"};
    int c=(int)lround(fxv_p(stage,0)*7.0),ft=(int)lround(fxv_p(stage,3)*7.0),wv=(int)lround(fxv_p(stage,14)*6.0);
    double drive=fxv_p(stage,1),fc=fxv_hz(fxv_p(stage,4)),res=fxv_p(stage,5);
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    rect(p,FXV_X,FXV_Y,FXV_W,16,0xff10181fU);
    text(p,circuit[c],FXV_X+8,FXV_Y+1,130,14,12,fxv_acc,0);
    for(int i=0;i<8;++i)rect(p,FXV_X+FXV_W-76+i*8,FXV_Y+5,5,5,i==c?fxv_acc:0xff2a323aU);
    /* left: clipper curve with a live input sine */
    int lx=FXV_X+8,lw=132,ly=FXV_Y+20,lh=52,cy=ly+lh/2;
    rect(p,lx,cy,lw,1,0xff1a222aU);rect(p,lx+lw/2,ly,1,lh,0xff1a222aU);
    double g=1.0+drive*9.0;int prev=cy;
    for(int x=0;x<lw;x+=2){
        double s=((double)x/(lw-1))*2-1,o=fxv_heat_clip(c,s,g);
        int y=cy-(int)lround((lh/2-2)*o);
        rect(p,lx+x,y<prev?y:prev,2,abs(y-prev)+2,fxv_acc);prev=y;
    }
    double ph=fxv_now*.0015;rect(p,lx+(int)lround((lw-3)*(.5+.5*sin(ph))),ly,3,lh,0xff3a4650U);
    /* right: filter response */
    int rx=FXV_X+150,rw=138,ry=FXV_Y+20,rh=52;
    text(p,ftype[ft],rx,FXV_Y+1,rw,14,10,0xffa0a8b0U,0);
    rect(p,rx,ry+rh-1,rw,1,0xff1a222aU);
    for(int x=0;x<rw;++x){
        double hz=20.0*pow(1000.0,(double)x/(rw-1)),db=fxv_heat_filter_db(ft,hz,fc,res);
        int h=(int)lround((db+36.0)/48.0*(rh-2));if(h<1)h=1;if(h>rh-2)h=rh-2;
        rect(p,rx+x,ry+rh-1-h,1,h,0xff1a2830U);rect(p,rx+x,ry+rh-1-h,1,2,fxv_acc);
    }
    /* bottom: LFO shape and Dirt/EQ readout */
    int by=FXV_Y+76;
    text(p,wave[wv],FXV_X+8,by,30,14,10,0xffa0a8b0U,0);
    for(int x=0;x<60;++x){int y=by+7-(int)lround(5*fxv_heat_wave(wv,x/60.0*2));rect(p,FXV_X+40+x,y,1,2,0xff4a8a98U);}
    rect(p,FXV_X+40+(int)((fxv_now/40)%60),by,1,14,0xff3a4650U);
    char line[40];snprintf(line,sizeof(line),"DIRT %d  MIX %d%%",(int)lround(fxv_p(stage,6)*127.0),(int)lround(fxv_p(stage,16)*100.0));
    text(p,line,FXV_X+120,by,120,14,10,0xffa0a8b0U,0);
    fxv_hs_credit(p);
}
/* Crossover line on a log axis; the bar left of it is the bass level, Mono Bass shades it. */
static void fxv_hs_bass(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    int x0=FXV_X+8,w=FXV_W-16,cx=x0+(int)lround(w*fxv_p(stage,0)),base=FXV_Y+FXV_H-16,top=FXV_Y+10;
    int level=(int)lround((base-top)*(.25+.75*fxv_p(stage,1)));
    rect(p,x0,base,w,1,0xff1a222aU);
    rect(p,x0,base-level,cx-x0,level,fxv_p(stage,2)>=.5?0xff5a6a78U:fxv_acc);
    rect(p,cx,top,1,base-top,0xff4a525aU);
    rect(p,cx+1,base-(base-top)/3,w-(cx-x0)-1,(base-top)/3,0xff2a323aU);
    fxv_hs_credit(p);
}
static int fx_visual_dispatch_inner(const struct resample_painter*p,const struct fx_catalog_entry*e,
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
    case 0x6146446c:fxv_delay(p,stage);return 1;        /* Delay */
    case 0x61464444:fxv_diff_delay(p,stage);return 1;   /* Diff Delay */
    case 0x41435658:fxv_tape_delay(p,stage,m,identity,now_ms);return 1;   /* Tape Delay */
    case 0x534d444c:fxv_sample_delay(p,stage);return 1; /* Sample Delay */
    case 0x61465276:fxv_reverb(p,stage,4,3,8);return 1; /* Reverb */
    case 0x61465370:fxv_spring(p,stage);return 1;       /* Spring Reverb */
    case 0x61464153:fxv_amp(p,stage,m);return 1;        /* Amp Sim */
    case 0x61465444:fxv_tube(p,stage);return 1;         /* Tube Drive */
    case 0x6153436c:fxv_soft_clip(p,stage);return 1;    /* Soft Clipper */
    case 0x6146456e:fxv_ensemble(p,stage);return 1;     /* Ensemble */
    case 0x61464d43:fxv_chorus(p,stage);return 1;       /* Multi Chorus */
    case 0x61465073:fxv_phaser(p,stage);return 1;       /* Phaser */
    case 0x6146466c:fxv_flanger(p,stage);return 1;      /* Flanger */
    case 0x41435679:fxv_auto_wah(p,stage,m);return 1;   /* Auto Wah */
    case 0x41435671:fxv_auto_pan(p,stage);return 1;     /* Auto Pan */
    case 0x56456678:fxv_vinyl(p,stage,identity,now_ms);return 1; /* Vinyl */
    case 0x54456678:fxv_tape_emu(p,stage,m);return 1;     /* Tape */
    case FX_OWN_OPTO:fxv_history_push(identity,now_ms,m);fxv_opto(p,stage,m);return 1;
    case FX_OWN_SHIMMER:fxv_shimmer(p,stage,m);return 1;
    case FX_OWN_PITCH:fxv_pitch(p,stage);return 1;
    case FX_OWN_REVERSE:fxv_reverse(p,stage);return 1;
    case FX_OWN_TREMOLO:fxv_tremolo(p,stage);return 1;
    case FX_OWN_SWELL:fxv_swell(p,stage);return 1;
    case FX_OWN_RING:fxv_ring(p,stage);return 1;
    case FX_OWN_CRUSH:fxv_crush(p,stage);return 1;
    case FX_OWN_HS_SATURATOR:fxv_hs_saturator(p,stage);return 1;
    case FX_OWN_HS_BASS:fxv_hs_bass(p,stage);return 1;
    case FX_OWN_HS_HEAT:fxv_hs_heat(p,stage);return 1;
    default:return 0;
    }
}

/* Small stylised AM MOD plate in the category colour, top right of the picture. */
static int fx_visual_dispatch(const struct resample_painter*p,const struct fx_catalog_entry*e,
                           const struct fx_stage*stage,const struct mixer_fx_monitor*m,double rate,
                           uint32_t identity,uint64_t now_ms)
{
    const uint32_t DYN=0xffffc247u,SPC=0xff00e0b0u,DST=0xffff6048u,MOD=0xffb078ffu,PIT=0xff40c8e8u,LO=0xffff8c32u;
    const char*label=0;uint32_t col=0;
    if(e&&stage&&stage->kind==FX_STAGE_NATIVE)switch(e->id){
    case FX_OWN_SWELL:label="SLOW ATTACK";col=DYN;break;
    case 0x6146446c:label="DELAY";col=SPC;break;case 0x61464444:label="DIFF DELAY";col=SPC;break;case 0x61465276:label="REVERB";col=SPC;break;
    case 0x61465370:label="SPRING";col=SPC;break;case FX_OWN_SHIMMER:label="SHIMMER";col=SPC;break;case FX_OWN_REVERSE:label="REVERSE";col=SPC;break;
    case 0x61464153:label="AMP SIM";col=DST;break;case 0x6153436c:label="SOFT CLIP";col=DST;break;
    case 0x6146456e:label="ENSEMBLE";col=MOD;break;case 0x61464d43:label="CHORUS";col=MOD;break;case 0x61465073:label="PHASER";col=MOD;break;
    case 0x6146466c:label="FLANGER";col=MOD;break;case 0x41435679:label="AUTO WAH";col=MOD;break;case 0x41435671:label="AUTO PAN";col=MOD;break;
    case FX_OWN_TREMOLO:label="TREMOLO";col=MOD;break;case FX_OWN_PITCH:label="PITCH";col=PIT;break;
    case FX_OWN_RING:label="RING MOD";col=LO;break;case FX_OWN_CRUSH:label="BIT CRUSH";col=LO;break;
    default:break;}
    fxv_acc=col?col:0xffffc247u;
    int ok=fx_visual_dispatch_inner(p,e,stage,m,rate,identity,now_ms);
    if(!ok||!label)return ok;
    int w=(int)strlen(label)*6+30,py=(e->id==0x41435671||e->id==0x61464153)?FXV_Y+FXV_H-13:FXV_Y+2;
    rect(p,FXV_X+FXV_W-w-3,py,w,11,col);
    text(p,"AM MOD",FXV_X+FXV_W-w-1,py,30,11,7,0xff101418u,0);
    text(p,label,FXV_X+FXV_W-w+29,py,w-31,11,8,0xff101418u,0);
    return ok;
}

/* Only Bypass disables animation. Silence advances the real UI timeline. */
static uint32_t fxv_dimmed(uint32_t c)
{
    unsigned r=c>>16&255,g=c>>8&255,b=c&255,mx=r>g?(r>b?r:b):(g>b?g:b);
    unsigned grey=(54*r+183*g+19*b)/256;if(mx>=0x50)grey=grey*45/100;
    return (c&0xff000000u)|grey*0x010101u;
}
static void fxv_dim_rect(void*user,int x,int y,int w,int h,uint32_t c)
{const struct resample_painter*b=user;b->rect(b->user,x,y,w,h,fxv_dimmed(c));}
static void fxv_dim_text(void*user,const char*s,int x,int y,int w,int h,unsigned size,uint32_t c,int centre)
{const struct resample_painter*b=user;b->text(b->user,s,x,y,w,h,size,fxv_dimmed(c),centre);}
static int fx_visual_paint(const struct resample_painter*p,const struct fx_catalog_entry*e,
                           const struct fx_stage*stage,const struct mixer_fx_monitor*m,double rate,
                           uint32_t identity,uint64_t now_ms)
{
    if(!e||!stage)return 0;
    fxv_idle=stage->bypass;fxv_history_bypass=stage->bypass;
    if(!fxv_idle){fxv_now=now_ms;int result=fx_visual_dispatch(p,e,stage,m,rate,identity,now_ms);fxv_history_bypass=0;return result;}
    struct resample_painter dim={(void*)p,fxv_dim_rect,fxv_dim_text};
    fxv_now=0;
    struct mixer_fx_monitor silent;memset(&silent,0,sizeof silent);
    int r=fx_visual_dispatch(&dim,e,stage,&silent,rate,identity,now_ms);
    fxv_idle=0;fxv_history_bypass=0;return r;
}
