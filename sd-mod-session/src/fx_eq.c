#include "fx_eq.h"
#include <math.h>
#include <string.h>

#define PI 3.14159265358979323846264338327950288
static const struct fx_eq_coeff identity={1,0,0,0,0};
static int rate_valid(double rate) {return isfinite(rate) && rate>=8000 && rate<=192000;}
void fx_eq_default(struct fx_eq_config *c)
{
    if(!c)return;
    memset(c,0,sizeof(*c));c->count=4;
    const double hz[]={100,500,2000,8000};
    for(unsigned i=0;i<c->count;++i)c->band[i]=(struct fx_eq_band){hz[i],0,1,FX_EQ_BELL,1};
}
int fx_eq_valid(const struct fx_eq_config *c,double rate)
{
    if(!c || !rate_valid(rate) || c->count>FX_EQ_MAX_BANDS || c->bypass>1 ||
       !isfinite(c->output_db) || c->output_db < -24 || c->output_db>12)return 0;
    for(unsigned i=0;i<c->count;++i){const struct fx_eq_band *b=&c->band[i];
        if(!isfinite(b->hz)||!isfinite(b->gain_db)||!isfinite(b->q)||b->hz<20 ||
           b->hz>20000 || b->hz>rate*.45 || b->gain_db < -18 || b->gain_db>18 ||
           b->q<.2 || b->q>16 || b->type>FX_EQ_NOTCH || b->enabled>1)return 0;
    }
    return 1;
}
int fx_eq_coefficients(const struct fx_eq_band *b,double rate,struct fx_eq_coeff *out)
{
    if(!b || !out)return 0;
    struct fx_eq_config c={.count=1,.band={*b}};
    if(!fx_eq_valid(&c,rate))return 0;
    if(!b->enabled){*out=identity;return 1;}
    const double w=2*PI*b->hz/rate,cs=cos(w),alpha=sin(w)/(2*b->q),A=pow(10,b->gain_db/40);
    double b0,b1,b2,a0,a1,a2;
    switch(b->type){
    case FX_EQ_BELL:
        b0=1+alpha*A;b1=-2*cs;b2=1-alpha*A;a0=1+alpha/A;a1=-2*cs;a2=1-alpha/A;break;
    case FX_EQ_LOW_SHELF:{double t=2*sqrt(A)*alpha;
        b0=A*((A+1)-(A-1)*cs+t);b1=2*A*((A-1)-(A+1)*cs);b2=A*((A+1)-(A-1)*cs-t);
        a0=(A+1)+(A-1)*cs+t;a1=-2*((A-1)+(A+1)*cs);a2=(A+1)+(A-1)*cs-t;break;}
    case FX_EQ_HIGH_SHELF:{double t=2*sqrt(A)*alpha;
        b0=A*((A+1)+(A-1)*cs+t);b1=-2*A*((A-1)+(A+1)*cs);b2=A*((A+1)+(A-1)*cs-t);
        a0=(A+1)-(A-1)*cs+t;a1=2*((A-1)-(A+1)*cs);a2=(A+1)-(A-1)*cs-t;break;}
    case FX_EQ_HIGH_PASS:
        b0=(1+cs)/2;b1=-(1+cs);b2=b0;a0=1+alpha;a1=-2*cs;a2=1-alpha;break;
    case FX_EQ_LOW_PASS:
        b0=(1-cs)/2;b1=1-cs;b2=b0;a0=1+alpha;a1=-2*cs;a2=1-alpha;break;
    case FX_EQ_NOTCH:
        b0=1;b1=-2*cs;b2=1;a0=1+alpha;a1=-2*cs;a2=1-alpha;break;
    default:return 0;
    }
    *out=(struct fx_eq_coeff){b0/a0,b1/a0,b2/a0,a1/a0,a2/a0};return 1;
}
static void targets(struct fx_eq *eq,const struct fx_eq_config *c)
{
    for(unsigned i=0;i<FX_EQ_MAX_BANDS;++i){eq->target[i]=identity;
        if(i<c->count)fx_eq_coefficients(&c->band[i],eq->rate,&eq->target[i]);}
    eq->mix_target=c->bypass?0:1;eq->output_target=pow(10,c->output_db/20);
}
int fx_eq_init(struct fx_eq *eq,double rate,const struct fx_eq_config *c)
{
    if(!eq || !fx_eq_valid(c,rate))return 0;
    memset(eq,0,sizeof(*eq));eq->rate=rate;eq->ramp=(unsigned)(rate*.005);
    targets(eq,c);memcpy(eq->coeff,eq->target,sizeof(eq->coeff));eq->mix=eq->mix_target;eq->output=eq->output_target;return 1;
}
int fx_eq_update(struct fx_eq *eq,const struct fx_eq_config *c)
{
    if(!eq || !fx_eq_valid(c,eq->rate))return 0;
    targets(eq,c);eq->remaining=eq->ramp;return 1;
}
static double clean(double v) {return fabs(v)<1e-25?0:v;}
void fx_eq_process(struct fx_eq *eq,float *stereo,size_t frames)
{
    if(!eq || !stereo)return;
    for(size_t f=0;f<frames;++f){
        if(eq->remaining){
            const double step=1./eq->remaining;
            for(unsigned i=0;i<FX_EQ_MAX_BANDS;++i){
                struct fx_eq_coeff *c=&eq->coeff[i],*t=&eq->target[i];
                c->b0+=(t->b0-c->b0)*step;c->b1+=(t->b1-c->b1)*step;c->b2+=(t->b2-c->b2)*step;
                c->a1+=(t->a1-c->a1)*step;c->a2+=(t->a2-c->a2)*step;
            }
            eq->mix+=(eq->mix_target-eq->mix)*step;eq->output+=(eq->output_target-eq->output)*step;--eq->remaining;
        }
        for(unsigned channel=0;channel<2;++channel){
            const float dry=stereo[f*2+channel];double y=dry;
            for(unsigned i=0;i<FX_EQ_MAX_BANDS;++i){
                const struct fx_eq_coeff *c=&eq->coeff[i];struct fx_eq_history *h=&eq->history[i];
                const double value=c->b0*y+h->z1[channel];
                h->z1[channel]=clean(c->b1*y-c->a1*value+h->z2[channel]);
                h->z2[channel]=clean(c->b2*y-c->a2*value);y=value;
            }
            /* Exact unity bypass after the ramp, even with Output Gain set. */
            stereo[f*2+channel]=eq->mix==0?dry:(float)(dry+eq->mix*(y*eq->output-dry));
        }
    }
}
double fx_eq_response_db(const struct fx_eq_config *c,double rate,double hz)
{
    if(!fx_eq_valid(c,rate)||!isfinite(hz)||hz<0||hz>rate/2)return NAN;
    if(c->bypass)return 0;
    double result=c->output_db,w=2*PI*hz/rate;
    for(unsigned i=0;i<c->count;++i){struct fx_eq_coeff b;fx_eq_coefficients(&c->band[i],rate,&b);
        const double nr=b.b0+b.b1*cos(w)+b.b2*cos(2*w),ni=-b.b1*sin(w)-b.b2*sin(2*w);
        const double dr=1+b.a1*cos(w)+b.a2*cos(2*w),di=-b.a1*sin(w)-b.a2*sin(2*w);
        const double power=(nr*nr+ni*ni)/(dr*dr+di*di);result+=10*log10(fmax(power,1e-30));
    }
    return result;
}
