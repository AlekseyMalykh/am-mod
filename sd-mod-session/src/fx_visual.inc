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
    if(source)snprintf(buf,sizeof(buf),"Key Track %c (red)  /  output (amber)  /  threshold (white)",'A'+(int)source-1);
    else snprintf(buf,sizeof(buf),"Key off  /  output (amber)  /  threshold (white)");
    fxv_title(p,buf,FXV_X+4,FXV_Y+1,290);
    fxv_strip(p,FXV_X+4,FXV_Y+14,FXV_W-8,FXV_H-34,0,1,threshold);
    fxv_gr_text(p,m,FXV_X+4,FXV_Y+FXV_H-18,120);
    snprintf(buf,sizeof(buf),"Threshold %.1f dB",threshold);
    text(p,buf,FXV_X+FXV_W-134,FXV_Y+FXV_H-18,130,12,10,WHITE,0);
}

/* ---- Delay and reverb: animated from the paint clock; parameters drive the picture. ---- */
static uint64_t fxv_now;
#define CYAN 0xff40c8e0u
static float fxv_p(const struct fx_stage*stage,unsigned i){return fmaxf(0.f,fminf(1.f,stage->parameter[i]));}
static void fxv_disc(const struct resample_painter*p,int cx,int cy,int r,uint32_t c)
{
    for(int y=-r;y<=r;++y){int w=(int)lround(sqrt((double)(r*r-y*y)));rect(p,cx-w,cy+y,2*w+1,1,c);}
}
static void fxv_params_text(const struct resample_painter*p,const char*s,int right)
{
    text(p,s,right?FXV_X+FXV_W-200:FXV_X+4,FXV_Y+FXV_H-18,right?196:290,12,10,WHITE,0);
}
static void fxv_delay(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,PANEL);
    float fb=fxv_p(stage,3);int spacing=16+(int)lround(56*fxv_p(stage,0));
    for(int lane=0;lane<2;++lane){
        int base=FXV_Y+14+lane*36+32;uint32_t col=lane?CYAN:AMBER;
        rect(p,FXV_X+4,base,FXV_W-8,1,DARK);
        for(int k=0;k<9;++k){
            int x=FXV_X+10+k*spacing+lane*spacing/2;if(x>FXV_X+FXV_W-14)break;
            double amp=pow(fb,k);int h=(int)lround(30*amp);if(h<1)h=1;
            rect(p,x,base-h,7,h,k?0xff5a4419u:col);rect(p,x,base-h,7,2,col);
        }
    }
    int span=FXV_W-20;int px=FXV_X+10+(int)((fxv_now/16)%(uint64_t)span);
    if(!fxv_idle)rect(p,px,FXV_Y+10,1,FXV_H-30,WHITE);
    char b[48];snprintf(b,sizeof(b),"Time %.0f%%  Feedback %.0f%%  Mix %.0f%%",fxv_p(stage,0)*100,fb*100,fxv_p(stage,9)*100);
    fxv_params_text(p,b,0);
}
static void fxv_diff_delay(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,PANEL);
    int base=FXV_Y+FXV_H-22;float fb=fxv_p(stage,2),dif=fxv_p(stage,4);int spread=60+(int)lround(200*fxv_p(stage,1));
    rect(p,FXV_X+4,base,FXV_W-8,1,DARK);
    rect(p,FXV_X+12,base-62,4,62,AMBER);
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
    char b[48];snprintf(b,sizeof(b),"Delay %.0f%%  Diffusion %.0f%%  Feedback %.0f%%",fxv_p(stage,1)*100,dif*100,fb*100);
    fxv_params_text(p,b,0);
}
static void fxv_tape_delay(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,0xff1c2026u);
    rect(p,FXV_X+40,FXV_Y+4,FXV_W-80,FXV_H-24,0xff2b3037u);
    rect(p,FXV_X+52,FXV_Y+8,FXV_W-104,14,0xffe8dcb8u);
    text(p,"AM MOD  TAPE ECHO",FXV_X+56,FXV_Y+9,FXV_W-112,12,9,0xff2b2b2bu,0);
    int cy=FXV_Y+46;
    double wow=fxv_p(stage,6),speed=.0022+.0045*fxv_p(stage,8);
    for(int side=0;side<2;++side){
        int cx=FXV_X+(side?FXV_W-92:92);
        fxv_disc(p,cx,cy,22,0xff0c0e10u);fxv_disc(p,cx,cy,17,0xff3a2c1eu);fxv_disc(p,cx,cy,9,0xffc8ccd4u);fxv_disc(p,cx,cy,3,0xff1c2026u);
        double ang=fxv_now*speed*(1+.12*wow*sin(fxv_now*.01))+side;
        for(int k=0;k<3;++k){double a=ang+k*2.0943951;
            for(int rr=3;rr<=16;rr+=3)rect(p,cx+(int)lround(cos(a)*rr)-1,cy+(int)lround(sin(a)*rr)-1,3,3,0xffc8ccd4u);}
    }
    rect(p,FXV_X+92,cy+22,FXV_W-184,2,0xff6a4a30u);
    for(int i=0;i<3;++i)rect(p,FXV_X+FXV_W/2-22+i*16,cy+16,10,14,0xff80858cu);
    char b[48];snprintf(b,sizeof(b),"Time %.0f%%  Feedback %.0f%%  Tone %.0f%%",fxv_p(stage,8)*100,fxv_p(stage,4)*100,fxv_p(stage,14)*100);
    fxv_params_text(p,b,0);
}
static void fxv_sample_delay(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    int shift[2]={(int)lround(40*fxv_p(stage,0)),(int)lround(40*fxv_p(stage,1))};
    for(int lane=0;lane<2;++lane){
        int cy=FXV_Y+24+lane*30;uint32_t col=lane?CYAN:AMBER;int prev=cy;
        rect(p,FXV_X+4,cy,FXV_W-8,1,DARK);
        for(int x=0;x<FXV_W-60;x+=2){
            double ph=(x-shift[lane])*.09+fxv_now*.003;
            int y=cy-(int)lround(11*sin(ph));
            rect(p,FXV_X+20+x,y<prev?y:prev,2,abs(y-prev)+2,col);prev=y;
        }
        text(p,lane?"R":"L",FXV_X+6,cy-6,12,12,10,col,0);
    }
    rect(p,FXV_X+20+shift[0],FXV_Y+60,abs(shift[1]-shift[0])+1,1,WHITE);
    char b[48];snprintf(b,sizeof(b),"Left %.0f%%  Right %.0f%%%s",fxv_p(stage,0)*100,fxv_p(stage,1)*100,fxv_p(stage,2)>.5f?"  Linked":"");
    fxv_params_text(p,b,0);
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
    char b[48];snprintf(b,sizeof(b),"Time %.0f%%  Mix %.0f%%",time*100,mix*100);
    fxv_params_text(p,b,0);
}
static void fxv_spring(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,0xff15181cu);
    rect(p,FXV_X+8,FXV_Y+8,6,58,0xff6a4a30u);rect(p,FXV_X+FXV_W-14,FXV_Y+8,6,58,0xff6a4a30u);
    double time=fxv_p(stage,1);int width=FXV_W-44;
    int pulse=(int)((fxv_now/(6+(int)(14*time)))%(uint64_t)(width+40))-20;
    for(int row=0;row<2;++row){
        int cy=FXV_Y+24+row*30,prev=cy;
        for(int x=0;x<width;x+=2){
            double tri=fabs(fmod(x/14.0+row*.5,1.0)*2-1)*2-1;
            double gain=1.0+.9*exp(-pow((x-pulse)/22.0,2.0));
            int y=cy+(int)lround(7*gain*tri);
            rect(p,FXV_X+22+x,y<prev?y:prev,2,abs(y-prev)+2,0xffc2c8d0u);prev=y;
        }
    }
    text(p,"IN",FXV_X+6,FXV_Y+FXV_H-34,24,11,9,GREY,0);text(p,"OUT",FXV_X+FXV_W-30,FXV_Y+FXV_H-34,26,11,9,GREY,0);
    char b[48];snprintf(b,sizeof(b),"Pre %.0f%%  Time %.0f%%  Mix %.0f%%",fxv_p(stage,0)*100,time*100,fxv_p(stage,5)*100);
    fxv_params_text(p,b,0);
}

/* ---- Drive, modulation, emulators. ---- */
static void fxv_knob(const struct resample_painter*p,int cx,int cy,int r,float v,uint32_t body)
{
    fxv_disc(p,cx,cy,r,body);
    double a=(-135.0+270.0*fmax(0.f,fmin(1.f,v)))*3.14159265/180.0;
    for(int k=2;k<=r;k+=2)rect(p,cx+(int)lround(sin(a)*k)-1,cy-(int)lround(cos(a)*k)-1,2,2,BLACK);
}
static void fxv_amp(const struct resample_painter*p,const struct fx_stage*stage,const struct mixer_fx_monitor*m)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,0xff101012u);
    rect(p,FXV_X+16,FXV_Y+2,FXV_W-32,FXV_H-22,0xff2a2622u);
    rect(p,FXV_X+20,FXV_Y+5,FXV_W-40,24,0xff1c1c1eu);
    static const unsigned idx[5]={0,4,5,7,8};static const char*lab[5]={"DRIVE","BASS","MID","TREB","OUT"};
    for(unsigned k=0;k<5;++k){
        int cx=FXV_X+44+(int)k*52;
        fxv_knob(p,cx,FXV_Y+16,8,fxv_p(stage,idx[k]),0xffd8d0b0u);
        text(p,lab[k],cx-18,FXV_Y+23,36,8,7,0xffc8a450u,1);
    }
    for(int y=FXV_Y+34;y<FXV_Y+FXV_H-24;y+=4)rect(p,FXV_X+20,y,FXV_W-40,2,(y/4)%2?0xff5a4a2au:0xff40341cu);
    rect(p,FXV_X+FXV_W/2-24,FXV_Y+48,48,14,0xff101010u);
    text(p,"AM-30",FXV_X+FXV_W/2-24,FXV_Y+49,48,12,10,0xffe8d8a0u,1);
    float lamp=m?fminf(1.f,m->output_peak*1.6f):0.f;
    fxv_disc(p,FXV_X+FXV_W-30,FXV_Y+17,4,lamp>.1f?0xffff3030u:0xff401010u);
    char b[64];snprintf(b,sizeof(b),"Drive %.0f%%  Treble %.0f%%  Cab %s",fxv_p(stage,0)*100,fxv_p(stage,7)*100,
                        fx_catalog_choice_name(fx_catalog_find(0x61464153u),9,stage->parameter[9]));
    fxv_params_text(p,b,0);
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
    int px=FXV_X+6,py=FXV_Y+4;
    rect(p,px,py,76,FXV_H-24,0xffd84a22u);rect(p,px+30,py-2,16,3,0xff80858cu);
    static const unsigned idx[4]={0,1,2,3};
    for(unsigned k=0;k<4;++k)fxv_knob(p,px+19+(int)(k%2)*38,py+14+(int)(k/2)*26,8,fxv_p(stage,idx[k]),0xff161616u);
    fxv_disc(p,px+38,py+FXV_H-24-14,7,0xffb0b4bau);
    text(p,"TUBE DRIVE",px,py+FXV_H-24-9,76,8,7,0xfff8e8d8u,1);
    fxv_clip_wave(p,FXV_X+90,FXV_Y+6,FXV_W-96,FXV_H-30,fxv_p(stage,0),0);
    char b[48];snprintf(b,sizeof(b),"Drive %.0f%%  Sat %.0f%%  Out %.0f%%",fxv_p(stage,0)*100,fxv_p(stage,2)*100,fxv_p(stage,3)*100);
    fxv_params_text(p,b,0);
}
static void fxv_soft_clip(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,PANEL);
    fxv_title(p,"Input (grey) and clipped output (amber)",FXV_X+4,FXV_Y+1,280);
    fxv_clip_wave(p,FXV_X+4,FXV_Y+14,FXV_W-8,FXV_H-34,fxv_p(stage,0),fxv_p(stage,1)>.5f);
    char b[48];snprintf(b,sizeof(b),"Drive %.0f%%  Shape %.0f%%  Mix %.0f%%",fxv_p(stage,0)*100,fxv_p(stage,1)*100,fxv_p(stage,3)*100);
    fxv_params_text(p,b,0);
}
static void fxv_lanes(const struct resample_painter*p,int lanes,double rate,double depth,uint32_t c0,uint32_t c1)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    for(int k=0;k<lanes;++k){
        int cy=FXV_Y+14+(FXV_H-42)*(2*k+1)/(2*lanes),prev=cy;
        uint32_t c=k%2?c1:c0;
        double wob=sin(fxv_now*.0012*(.4+rate*3)+k*1.7)*depth*14;
        for(int x=0;x<FXV_W-16;x+=2){
            int y=cy-(int)lround(7*sin((x+wob)*.07+k*.5+fxv_now*.003));
            rect(p,FXV_X+8+x,y<prev?y:prev,2,abs(y-prev)+2,c);prev=y;
        }
    }
}
static void fxv_ensemble(const struct resample_painter*p,const struct fx_stage*stage)
{
    fxv_lanes(p,6,fxv_p(stage,0),fxv_p(stage,1),AMBER,0xffc07040u);
    char b[48];snprintf(b,sizeof(b),"Rate %.0f%%  Depth %.0f%%  Mix %.0f%%",fxv_p(stage,0)*100,fxv_p(stage,1)*100,fxv_p(stage,5)*100);
    fxv_params_text(p,b,0);
}
static void fxv_chorus(const struct resample_painter*p,const struct fx_stage*stage)
{
    int voices=2+(int)lround(4*fxv_p(stage,5));
    fxv_lanes(p,voices,fxv_p(stage,0),fxv_p(stage,1),AMBER,0xffa070e8u);
    char b[48];snprintf(b,sizeof(b),"Rate %.0f%%  Depth %.0f%%  Voices %d",fxv_p(stage,0)*100,fxv_p(stage,1)*100,voices);
    fxv_params_text(p,b,0);
}
static void fxv_phaser(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    double sw=.5+.4*sin(fxv_now*.0015*(.3+3*fxv_p(stage,0))),depth=.3+.7*fxv_p(stage,1),fb=fxv_p(stage,2);
    int prev=FXV_Y+14;
    for(int x=0;x<FXV_W-8;x+=2){
        double u=(double)x/(FXV_W-8),v=0;
        for(int k=0;k<3;++k){double c=(.12+sw*depth*(.28+k*.22));v+=exp(-pow((u-c)/(.025+.02*(1-fb)),2.0));}
        int y=FXV_Y+14+(int)lround(34*fmin(1.0,v));
        rect(p,FXV_X+4+x,y<prev?y:prev,2,abs(y-prev)+2,AMBER);prev=y;
    }
    for(int k=0;k<4;++k){int cx=FXV_X+30+k*70;
        fxv_disc(p,cx,FXV_Y+FXV_H-34,8,(fxv_now/200)%4==(uint64_t)k?AMBER:0xff50441eu);
        if(k<3)rect(p,cx+9,FXV_Y+FXV_H-35,52,2,GREY);}
    char b[48];snprintf(b,sizeof(b),"Rate %.0f%%  Depth %.0f%%  Feedback %.0f%%",fxv_p(stage,0)*100,fxv_p(stage,1)*100,fb*100);
    fxv_params_text(p,b,0);
}
static void fxv_flanger(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    double d=10+9*sin(fxv_now*.0016*(.3+3*fxv_p(stage,0)))*(.4+.6*fxv_p(stage,1)),fb=fxv_p(stage,2);
    int prev=FXV_Y+14;
    for(int x=0;x<FXV_W-8;x+=2){
        double v=.5+.5*cos(x/d*3.14159)*(.3+.7*fb);
        int y=FXV_Y+14+(int)lround(44*(1-v));
        rect(p,FXV_X+4+x,y<prev?y:prev,2,abs(y-prev)+2,AMBER);prev=y;
    }
    char b[48];snprintf(b,sizeof(b),"Rate %.0f%%  Depth %.0f%%  Feedback %.0f%%",fxv_p(stage,0)*100,fxv_p(stage,1)*100,fb*100);
    fxv_params_text(p,b,0);
}
static void fxv_auto_wah(const struct resample_painter*p,const struct fx_stage*stage,const struct mixer_fx_monitor*m)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    double level=m?fmin(1.0,m->input_peak*1.5):.5,centre=.15+.7*fxv_p(stage,13)+.3*(level-.5)*fxv_p(stage,9),res=fxv_p(stage,8);
    centre=fmax(.08,fmin(.92,centre));
    int prev=FXV_Y+60;
    for(int x=0;x<FXV_W-8;x+=2){
        double u=(double)x/(FXV_W-8);
        double v=.12+(.5+.5*res)*exp(-pow((u-centre)/(.14-.09*res),2.0));
        int y=FXV_Y+60-(int)lround(46*fmin(1.0,v));
        rect(p,FXV_X+4+x,y<prev?y:prev,2,abs(y-prev)+2,AMBER);prev=y;
    }
    /* pedal rocker follows the sweep */
    int px=FXV_X+FXV_W/2,tilt=(int)lround((centre-.5)*34);
    rect(p,px-40,FXV_Y+FXV_H-30,80,6,0xff30343au);
    for(int k=-38;k<=38;k+=2)rect(p,px+k,FXV_Y+FXV_H-36-(int)lround(k*tilt/60.0),2,5,0xffb0b4bcu);
    char b[48];snprintf(b,sizeof(b),"Sens %.0f%%  Res %.0f%%  Center %.0f%%",fxv_p(stage,9)*100,res*100,fxv_p(stage,13)*100);
    fxv_params_text(p,b,0);
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
        text(p,side?"R":"L",cx-6,FXV_Y+9,12,11,10,AMBER,1);
    }
    int ox=FXV_X+FXV_W/2+(int)lround(pos*80);
    fxv_disc(p,ox,FXV_Y+40,7,AMBER);
    rect(p,FXV_X+70,FXV_Y+FXV_H-28,FXV_W-140,2,GREY);
    char b[48];snprintf(b,sizeof(b),"Rate %.0f%%  Mix %.0f%%",fxv_p(stage,8)*100,fxv_p(stage,12)*100);
    fxv_params_text(p,b,0);
}
static void fxv_vinyl(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,0xff261a10u);
    int cx=FXV_X+100,cy=FXV_Y+37;
    fxv_disc(p,cx,cy,34,0xff0a0a0cu);
    for(int r=14;r<=32;r+=6){for(int a=0;a<24;++a){double t=a*.2618;rect(p,cx+(int)lround(cos(t)*r),cy+(int)lround(sin(t)*r),1,1,0xff1e1e22u);}}
    fxv_disc(p,cx,cy,11,0xffb83828u);fxv_disc(p,cx,cy,2,BLACK);
    double ang=fxv_now*.0035*(1+(fxv_p(stage,2)-.5)*.3);
    for(int k=0;k<3;++k){double t=ang+k*2.0944;rect(p,cx+(int)lround(cos(t)*27)-1,cy+(int)lround(sin(t)*27)-1,3,3,0xff4a4a54u);}
    int ax=FXV_X+250,ay=FXV_Y+8;
    for(int k=0;k<=36;k+=2){int x=ax-(int)lround(k*1.4),y=ay+(int)lround(k*1.9);rect(p,x,y,3,3,0xffb0b4bcu);}
    fxv_disc(p,ax,ay,6,0xff70747au);
    unsigned seed=(unsigned)(fxv_now/90);int sparks=(int)lround(6*fxv_p(stage,1));
    for(int k=0;k<sparks;++k){seed=seed*1664525u+1013904223u;rect(p,FXV_X+192+(int)(seed>>9&15),FXV_Y+72+(int)(seed>>13&7),2,2,AMBER);}
    char b[48];snprintf(b,sizeof(b),"Tone %.0f%%  Crackle %.0f%%  Pitch %.0f%%",fxv_p(stage,0)*100,fxv_p(stage,1)*100,fxv_p(stage,2)*100);
    fxv_params_text(p,b,0);
}
static void fxv_tape_emu(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,0xff22252au);
    int cy=FXV_Y+38;double wow=fxv_p(stage,0);
    for(int side=0;side<2;++side){
        int cx=FXV_X+(side?FXV_W-76:76);
        fxv_disc(p,cx,cy,30,0xffc0c4ccu);fxv_disc(p,cx,cy,24,0xff202226u);
        double ang=fxv_now*.0026*(1+.15*wow*sin(fxv_now*.012))+side;
        for(int k=0;k<3;++k){double t=ang+k*2.0944;for(int rr=4;rr<=22;rr+=3)rect(p,cx+(int)lround(cos(t)*rr)-1,cy+(int)lround(sin(t)*rr)-1,3,3,0xffc0c4ccu);}
        fxv_disc(p,cx,cy,5,0xffc0c4ccu);
    }
    rect(p,FXV_X+76,cy+32,FXV_W-152,2,0xff6a4a30u);
    /* Two VU meters in the gap between the reels; the needles swing around a pivot at the bottom. */
    for(int k=0;k<2;++k){
        int x=FXV_X+112+k*42,top=FXV_Y+8,pivot_x=x+17,pivot_y=top+23;
        rect(p,x,top,34,26,0xffe8e0c0u);
        for(int t=-2;t<=2;++t){double a=t*.38;rect(p,pivot_x+(int)lround(sin(a)*19),pivot_y-(int)lround(cos(a)*19),1,2,0xff6a5a40u);}
        double a=(-.8+1.6*(.5+.5*sin(fxv_now*.004+k*1.3)))*(.6+.4*(1-wow));
        for(int r=0;r<=18;++r)rect(p,pivot_x+(int)lround(sin(a)*r),pivot_y-(int)lround(cos(a)*r),1,1,k?0xffb02a1eu:BLACK);
        rect(p,pivot_x-1,pivot_y-1,3,3,BLACK);
    }
    char b[48];snprintf(b,sizeof(b),"Wow %.0f%%  Noise %.0f%%  Pitch %.0f%%",wow*100,fxv_p(stage,1)*100,fxv_p(stage,2)*100);
    fxv_params_text(p,b,0);
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
    snprintf(buf,sizeof(buf),"%.0f dB  %.1f:1%s",(double)c->threshold,(double)c->ratio,c->warm?"  WARM":"");
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
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    int cx=FXV_X+FXV_W/2,cy=FXV_Y+52;
    double decay=fxv_p(stage,1),amount=fxv_p(stage,2),size=.4+.6*fxv_p(stage,4);
    double cycle=3000+4500*decay;
    for(int k=0;k<4;++k){
        double age=fmod(fxv_now/cycle+k/4.0,1.0),sc=(.08+.92*age)*size;
        int hw=(int)lround(120*sc),hh=(int)lround(40*sc);
        uint32_t c=0xff000000u|(((unsigned)lround(0x40*(1-age)+8*age)&255)<<16)|(((unsigned)lround(0x90*(1-age)+13*age)&255)<<8)|((unsigned)lround(0xd0*(1-age)+18*age)&255);
        rect(p,cx-hw,cy-hh,2*hw+1,1,c);rect(p,cx-hw,cy+hh,2*hw+1,1,c);rect(p,cx-hw,cy-hh,1,2*hh+1,c);rect(p,cx+hw,cy-hh,1,2*hh+1,c);
    }
    /* Sparkles climb and brighten: each one is the tail an octave higher. */
    int count=6+(int)lround(26*amount),seed=(int)(m&&m->output_peak>.05f?3:1);
    unsigned rnd=2463534242u;
    for(int i=0;i<count;++i){
        rnd=rnd*1664525u+1013904223u;int x=FXV_X+8+(int)((rnd>>8)%(FXV_W-16));
        rnd=rnd*1664525u+1013904223u;double phase=(double)((rnd>>8)&1023)/1024.0;
        double up=fmod(fxv_now/(2600.0+1800.0*decay)*seed+phase,1.0);
        int y=FXV_Y+FXV_H-30-(int)lround(up*(FXV_H-44));
        uint32_t c=up<.5?AMBER:up<.8?0xffffe08au:0xffffffffu;
        if(up>.9)continue;
        rect(p,x,y,2,2,c);if(up>.4)rect(p,x-1,y+2,4,1,c);
    }
    fxv_disc(p,cx,cy+22,3,AMBER);
    int st=fx_own_shimmer_semitones(stage->parameter[3]);
    char b[64];snprintf(b,sizeof(b),"%+d st  Shimmer %.0f%%  Decay %.0f%%  Mix %.0f%%",st,amount*100,decay*100,fxv_p(stage,0)*100);
    fxv_params_text(p,b,0);
}
static void fxv_pitch(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    int st=fx_own_pitch_semitones(stage->parameter[0]);double cents=(fxv_p(stage,1)-.5)*100.0;
    double ratio=pow(2.0,(st+cents/100.0)/12.0);
    for(int lane=0;lane<2;++lane){
        int cy=FXV_Y+26+lane*34;uint32_t c=lane?AMBER:0xff7a828aU;int prev=cy;
        rect(p,FXV_X+4,cy,FXV_W-8,1,DARK);
        double f=lane?ratio:1.0;
        for(int x=0;x<FXV_W-60;x+=2){
            int y=cy-(int)lround(11*sin(x*.06*f+fxv_now*.003*f));
            rect(p,FXV_X+40+x,y<prev?y:prev,2,abs(y-prev)+2,c);prev=y;
        }
        text(p,lane?"OUT":"IN",FXV_X+6,cy-6,30,12,10,c,0);
    }
    char b[64];snprintf(b,sizeof(b),"%+d st  Detune %+.0f ct  Mix %.0f%%",st,cents,fxv_p(stage,2)*100);
    fxv_params_text(p,b,0);
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
    char b[64];snprintf(b,sizeof(b),"Time %.0f ms  Feedback %.0f%%  Mix %.0f%%",time*1000,fb*100/.85,fxv_p(stage,2)*100);
    fxv_params_text(p,b,0);
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
        rect(p,FXV_X+8+x,top<prevt?top:prevt,2,abs(top-prevt)+2,AMBER);
        int bot=cy+(int)lround(34*g);rect(p,FXV_X+8+x,bot<prevw?bot:prevw,2,abs(bot-prevw)+2,0xff7a5f22u);
        int w=cy-(int)lround(34*g*sin(x*.55));
        rect(p,FXV_X+8+x,w,2,2,0xff4a525au);
        prevt=top;prevw=bot;
    }
    char b[64];snprintf(b,sizeof(b),"Rate %.1f Hz  Depth %.0f%%  Shape %.0f%%",hz,depth*100,shape*100);
    fxv_params_text(p,b,0);
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
        rect(p,FXV_X+12+x,yo<prevo?yo:prevo,2,abs(yo-prevo)+2,AMBER);previ=yi;prevo=yo;
    }
    int px=FXV_X+12+(int)((fxv_now/14)%(uint64_t)w);
    if(!fxv_idle)rect(p,px,FXV_Y+8,1,FXV_H-34,WHITE);
    char b[64];snprintf(b,sizeof(b),"Attack %.0f ms  Sensitivity %.0f%%",attack*1000,fxv_p(stage,1)*100);
    fxv_params_text(p,b,0);
}
static void fxv_ring(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    double fc=20.0*pow(250.0,fxv_p(stage,0));double car=fmin(.9,.06+fc/2200.0);
    static const char*names[3]={"IN","CARRIER","OUT"};
    for(int lane=0;lane<3;++lane){
        int cy=FXV_Y+16+lane*26;uint32_t c=lane==0?0xff7a828aU:lane==1?CYAN:AMBER;int prev=cy;
        rect(p,FXV_X+4,cy,FXV_W-8,1,DARK);
        for(int x=0;x<FXV_W-70;x+=2){
            double a=sin(x*.07+fxv_now*.003),b=sin(x*car+fxv_now*.004);
            double v=lane==0?a:lane==1?b:a*b;
            int y=cy-(int)lround(9*v);rect(p,FXV_X+60+x,y<prev?y:prev,2,abs(y-prev)+2,c);prev=y;
        }
        text(p,names[lane],FXV_X+4,cy-6,54,12,9,c,0);
    }
    char b[64];snprintf(b,sizeof(b),"Frequency %.0f Hz  Mix %.0f%%",fc,fxv_p(stage,1)*100);
    fxv_params_text(p,b,0);
}
static void fxv_crush(const struct resample_painter*p,const struct fx_stage*stage)
{
    rect(p,FXV_X,FXV_Y,FXV_W,FXV_H,WELL);
    int bits=2+(int)lround(14*fxv_p(stage,0));int factor=(int)lround(pow(32.0,fxv_p(stage,1)));if(factor<1)factor=1;
    double levels=pow(2.0,bits-1);
    int cy=FXV_Y+40,amp=34,prevo=cy,previ=cy;int hold=0;double held=0;
    for(int x=0;x<FXV_W-16;x+=2){
        double s=sin(x*.06+fxv_now*.003)*.9;
        if(hold==0)held=floor(s*levels+.5)/levels;
        hold=(hold+1)%(factor>8?factor/2:factor);
        int yi=cy-(int)lround(amp*s),yo=cy-(int)lround(amp*held);
        rect(p,FXV_X+8+x,yi<previ?yi:previ,2,abs(yi-previ)+1,0xff4a525au);
        rect(p,FXV_X+8+x,yo<prevo?yo:prevo,2,abs(yo-prevo)+2,AMBER);
        previ=yi;prevo=yo;
    }
    char b[64];snprintf(b,sizeof(b),"%d-bit  Rate 1/%d  Mix %.0f%%",bits,factor,fxv_p(stage,2)*100);
    fxv_params_text(p,b,0);
}
static int fx_visual_dispatch(const struct resample_painter*p,const struct fx_catalog_entry*e,
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
    case 0x41435658:fxv_tape_delay(p,stage);return 1;   /* Tape Delay */
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
    case 0x56456678:fxv_vinyl(p,stage);return 1;        /* Vinyl */
    case 0x54456678:fxv_tape_emu(p,stage);return 1;     /* Tape */
    case FX_OWN_OPTO:fxv_opto(p,stage,m);return 1;
    case FX_OWN_SHIMMER:fxv_shimmer(p,stage,m);return 1;
    case FX_OWN_PITCH:fxv_pitch(p,stage);return 1;
    case FX_OWN_REVERSE:fxv_reverse(p,stage);return 1;
    case FX_OWN_TREMOLO:fxv_tremolo(p,stage);return 1;
    case FX_OWN_SWELL:fxv_swell(p,stage);return 1;
    case FX_OWN_RING:fxv_ring(p,stage);return 1;
    case FX_OWN_CRUSH:fxv_crush(p,stage);return 1;
    default:return 0;
    }
}

/* With no signal (or a bypassed effect) the picture stops moving and is dimmed; it still follows the parameters. */
static uint32_t fxv_dimmed(uint32_t c)
{
    unsigned r=c>>16&255,g=c>>8&255,b=c&255,mx=r>g?(r>b?r:b):(g>b?g:b);
    if(mx<0x50)return c;
    return (c&0xff000000u)|((r*45/100)<<16)|((g*45/100)<<8)|(b*45/100);
}
static void fxv_dim_rect(void*user,int x,int y,int w,int h,uint32_t c)
{const struct resample_painter*b=user;b->rect(b->user,x,y,w,h,fxv_dimmed(c));}
static void fxv_dim_text(void*user,const char*s,int x,int y,int w,int h,unsigned size,uint32_t c,int centre)
{const struct resample_painter*b=user;b->text(b->user,s,x,y,w,h,size,c,centre);}
static int fx_visual_paint(const struct resample_painter*p,const struct fx_catalog_entry*e,
                           const struct fx_stage*stage,const struct mixer_fx_monitor*m,double rate,
                           uint32_t identity,uint64_t now_ms)
{
    static uint32_t last_identity;static uint64_t last_active;
    if(!e||!stage)return 0;
    if(identity!=last_identity){last_identity=identity;last_active=0;}
    if(m&&(m->input_peak>.0005f||m->output_peak>.0005f))last_active=now_ms+1;
    fxv_idle=stage->bypass||!last_active||now_ms<last_active-1||now_ms-(last_active-1)>600;
    if(!fxv_idle){fxv_now=now_ms;return fx_visual_dispatch(p,e,stage,m,rate,identity,now_ms);}
    struct resample_painter dim={(void*)p,fxv_dim_rect,fxv_dim_text};
    fxv_now=0;
    int r=fx_visual_dispatch(&dim,e,stage,m,rate,identity,now_ms);
    fxv_idle=0;return r;
}
