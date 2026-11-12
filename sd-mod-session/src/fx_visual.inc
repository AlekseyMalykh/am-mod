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
    rect(p,px,FXV_Y+10,1,FXV_H-30,WHITE);
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
    int sweep=(int)((fxv_now/10)%(uint64_t)(spread+40));
    for(int i=0;i<n;++i){
        seed=seed*1664525u+1013904223u;double u=pow((seed>>8&0xffff)/65535.0,.8);
        int x=FXV_X+22+(int)lround(u*spread);if(x-FXV_X-22>sweep)continue;
        seed=seed*1664525u+1013904223u;double r=(seed>>8&0xffff)/65535.0;
        double amp=pow(1-u,2.4-fb*1.6)*(.35+.65*r);int h=(int)lround(58*amp);if(h<2)h=2;
        rect(p,x,base-h,3,h,amp>.5?AMBER:0xff7a5f22u);
    }
    rect(p,FXV_X+22+(sweep<spread?sweep:spread),FXV_Y+8,1,FXV_H-30,WHITE);
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
    int cx=FXV_X+FXV_W/2,cy=FXV_Y+40;
    double time=fxv_p(stage,time_i),size=size_i==99?1.0:.4+.6*fxv_p(stage,size_i);
    double cycle=3200+4000*time;
    for(int k=0;k<6;++k){
        double age=fmod(fxv_now/cycle+k/6.0,1.0),sc=(.06+.94*age)*size;
        int hw=(int)lround(130*sc),hh=(int)lround(34*sc);
        uint32_t c=0xff000000u|(((unsigned)lround(0x71*(1-age)+8*age)&255)<<16)|(((unsigned)lround(0xc1*(1-age)+13*age)&255)<<8)|((unsigned)lround(0xe8*(1-age)+18*age)&255);
        rect(p,cx-hw,cy-hh,2*hw+1,1,c);rect(p,cx-hw,cy+hh,2*hw+1,1,c);
        rect(p,cx-hw,cy-hh,1,2*hh+1,c);rect(p,cx+hw,cy-hh,1,2*hh+1,c);
    }
    fxv_disc(p,cx,cy,4,AMBER);
    rect(p,FXV_X+4,FXV_Y+FXV_H-26,FXV_W-8,4,DARK);
    rect(p,FXV_X+4,FXV_Y+FXV_H-26,(int)lround((FXV_W-8)*time),4,AMBER);
    char b[48];snprintf(b,sizeof(b),"Time %.0f%%  Mix %.0f%%",time*100,fxv_p(stage,mix_i)*100);
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
static int fx_visual_paint(const struct resample_painter*p,const struct fx_catalog_entry*e,
                           const struct fx_stage*stage,const struct mixer_fx_monitor*m,double rate,
                           uint32_t identity,uint64_t now_ms)
{
    if(!e||!stage)return 0;
    fxv_now=now_ms;
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
    default:return 0;
    }
}
