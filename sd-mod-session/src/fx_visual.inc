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
static int fx_visual_paint(const struct resample_painter*p,const struct fx_catalog_entry*e,
                           const struct fx_stage*stage,const struct mixer_fx_monitor*m,double rate)
{
    if(!e||!stage||stage->kind!=FX_STAGE_NATIVE)return 0;
    switch(e->id){
    case 0x41435663:fxv_filter(p,stage,0,m,rate);return 1; /* HP Filter */
    case 0x41435662:fxv_filter(p,stage,1,m,rate);return 1; /* LP Filter */
    case 0x61465346:fxv_filter(p,stage,2,m,rate);return 1; /* BP Filter */
    default:return 0;
    }
}
