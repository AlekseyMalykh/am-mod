#include "fx_eq_editor.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WHITE 0xffffffffu
#define AMBER 0xffffc247u
#define CYAN 0xff39c1d6u
#define GREY 0xff777777u
#define RED 0xffff4040u
#define GX 34
#define GY 63
#define GW 276
#define GH 112
static int clamp(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
static double limit(const struct fx_eq_editor*e){return fmin(20000,e->rate*.45);}
static int x_at(const struct fx_eq_editor*e,double hz){return GX+clamp((int)lround(log(hz/20)/log(limit(e)/20)*GW),0,GW);}
static int y_at(double db){return GY+clamp((int)lround((18-db)*GH/36),0,GH);}
static const char*const types[]={"Bell","Low Shelf","High Shelf","HP 12dB","LP 12dB"};
static void text(const struct resample_painter*p,const char*s,int x,int y,int w,int h,unsigned size,uint32_t colour,int centre){p->text(p->user,s,x,y,w,h,size,colour,centre);}
int fx_eq_editor_open(struct fx_eq_editor*e,const struct fx_eq_config*c,double rate){
    if(!e||!fx_eq_valid(c,rate))return 0;
    struct fx_eq_config copy=*c;
    memset(e,0,sizeof(*e));e->config=copy;e->rate=rate;e->revision=1;return 1;
}
static void changed(struct fx_eq_editor*e){if(!++e->revision)++e->revision;e->message[0]=0;}
static void add_point(struct fx_eq_editor*e){
    if(e->config.count==FX_EQ_MAX_BANDS){snprintf(e->message,sizeof(e->message),"Maximum %u points",FX_EQ_MAX_BANDS);return;}
    double frequency=1000;
    if(e->config.count){
        double sorted[FX_EQ_MAX_BANDS+2]={20};unsigned n=e->config.count;
        for(unsigned i=0;i<n;++i)sorted[i+1]=e->config.band[i].hz;
        sorted[n+1]=limit(e);
        for(unsigned i=1;i<n+1;++i)for(unsigned j=i+1;j<n+1;++j)if(sorted[i]>sorted[j]){double t=sorted[i];sorted[i]=sorted[j];sorted[j]=t;}
        double gap=0;for(unsigned i=0;i<=n;++i)if(log(sorted[i+1]/sorted[i])>gap){gap=log(sorted[i+1]/sorted[i]);frequency=sqrt(sorted[i+1]*sorted[i]);}
    }
    e->selected=e->config.count++;
    e->config.band[e->selected]=(struct fx_eq_band){fmin(limit(e),frequency),0,1,FX_EQ_BELL,1};changed(e);
}
int fx_eq_editor_button(struct fx_eq_editor*e,unsigned button,int shift){
    (void)shift;
    if(button==0){if(e->points){e->points=0;return EQ_EDITOR_REDRAW;}return EQ_EDITOR_CLOSE;}
    if(button==3){e->points=0;return EQ_EDITOR_REDRAW;} /* Encoder click */
    if(button==1){if(!e->points&&e->config.count){e->points=1;return EQ_EDITOR_REDRAW;}
        unsigned before=e->config.count;add_point(e);return before!=e->config.count?EQ_EDITOR_CHANGED:EQ_EDITOR_REDRAW;}
    if(button==2){
        if(e->points){if(!e->config.count)return EQ_EDITOR_NONE;
            memmove(e->config.band+e->selected,e->config.band+e->selected+1,(e->config.count-e->selected-1)*sizeof(e->config.band[0]));
            memset(e->config.band+--e->config.count,0,sizeof(e->config.band[0]));
            e->selected=e->config.count?(unsigned)clamp((int)e->selected,0,(int)e->config.count-1):0;
        }else e->config.bypass=!e->config.bypass;
        changed(e);return EQ_EDITOR_CHANGED;
    }
    return EQ_EDITOR_NONE;
}
int fx_eq_editor_control(struct fx_eq_editor*e,unsigned cc,unsigned value,int shift){
    e->shifted=!!shift;
    if(value>127)return EQ_EDITOR_NONE;
    int delta=value>64?(int)value-128:(int)value;
    if(cc==0x64){if(e->config.count)e->selected=(unsigned)clamp((int)e->selected+delta,0,(int)e->config.count-1);return EQ_EDITOR_REDRAW;}
    if(cc<0x10||cc>0x12)return EQ_EDITOR_NONE;
    if(e->points){if(cc==0x10&&e->config.count)e->selected=value*e->config.count/128;return EQ_EDITOR_REDRAW;}
    if(shift&&cc==0x12){e->config.output_db=-24+36*value/127.;changed(e);return EQ_EDITOR_CHANGED;}
    if(!e->config.count){snprintf(e->message,sizeof(e->message),"B2 Add point");return EQ_EDITOR_REDRAW;}
    struct fx_eq_band*b=&e->config.band[e->selected];
    if(shift){if(cc==0x10)b->type=value*5/128;else b->enabled=value>=64;}
    else if(cc==0x10)b->hz=20*pow(limit(e)/20,value/127.);
    else if(cc==0x11){if(b->type>=FX_EQ_HIGH_PASS)return EQ_EDITOR_NONE;b->gain_db=-18+36*value/127.;}
    else b->q=.2*pow(80,value/127.);
    changed(e);return EQ_EDITOR_CHANGED;
}
void fx_eq_editor_paint(struct fx_eq_editor*e,const struct resample_painter*p){
    if(!e||!p||!p->rect||!p->text)return;
    p->rect(p->user,0,5,320,235,0xff000000u);
    text(p,"Back",0,7,106,22,15,RED,1);
    text(p,e->points||!e->config.count?"Add point":"Points",106,7,106,22,15,WHITE,1);
    text(p,e->points?"Delete":e->config.bypass?"Enable":"Bypass",212,7,106,22,15,e->points?RED:WHITE,1);
    char s[80];snprintf(s,sizeof(s),"Graphic EQ   %u/%u points%s",e->config.count,FX_EQ_MAX_BANDS,e->config.bypass?"  BYPASS":"");
    text(p,s,7,34,306,22,16,e->config.bypass?CYAN:WHITE,0);
    if(e->points){
        unsigned first=e->selected>=5?e->selected-4:0;
        for(unsigned row=0;row<5&&first+row<e->config.count;++row){unsigned i=first+row;const struct fx_eq_band*b=&e->config.band[i];
            snprintf(s,sizeof(s),"%u %s  %.0fHz %+.1fdB%s",i+1,types[b->type],b->hz,b->gain_db,b->enabled?"":" OFF");
            text(p,s,8,65+(int)row*23,304,23,16,i==e->selected?AMBER:WHITE,0);
        }
        if(!e->config.count)text(p,"No points. B2 adds a point.",8,100,304,25,17,WHITE,0);
        text(p,e->message[0]?e->message:"Encoder: select   Click: graph",8,208,304,24,14,GREY,0);return;
    }
    for(int db=-18;db<=18;db+=9){int y=y_at(db);p->rect(p->user,GX,y,GW+1,1,db?0xff222222u:0xff666666u);snprintf(s,sizeof(s),"%+d",db);text(p,s,0,y-6,30,12,10,GREY,1);}
    const unsigned ticks[]={100,1000,10000};
    for(unsigned i=0;i<3;++i)if(ticks[i]<limit(e)){int x=x_at(e,ticks[i]);p->rect(p->user,x,GY,1,GH+1,0xff222222u);snprintf(s,sizeof(s),ticks[i]>=1000?"%uk":"%u",ticks[i]>=1000?ticks[i]/1000:ticks[i]);text(p,s,x-13,GY+GH+2,26,12,10,GREY,1);}
    if(e->paint_revision!=e->revision){struct fx_eq_config cfg=e->config;cfg.bypass=0;
        for(unsigned x=0;x<=GW;++x)e->curve[x]=y_at(fx_eq_response_db(&cfg,e->rate,20*pow(limit(e)/20,x/(double)GW)));
        e->paint_revision=e->revision;
    }
    for(unsigned x=1;x<=GW;++x)if(!e->config.bypass||x%6<3){int a=e->curve[x-1],b=e->curve[x];p->rect(p->user,GX+(int)x,a<b?a:b,1,abs(a-b)+1,e->config.bypass?GREY:AMBER);}
    if(e->config.bypass)p->rect(p->user,GX,y_at(0),GW+1,1,CYAN);
    for(unsigned i=0;i<e->config.count;++i){const struct fx_eq_band*b=&e->config.band[i];int x=x_at(e,b->hz),y=y_at(b->type>=FX_EQ_HIGH_PASS?0:b->gain_db);
        uint32_t ink=!b->enabled?GREY:i==e->selected?AMBER:WHITE;
        p->rect(p->user,x-2,y-2,5,5,ink);snprintf(s,sizeof(s),"%u",i+1);text(p,s,x-7,y<GY+14?y+4:y-14,14,12,11,ink,1);
    }
    if(e->message[0])text(p,e->message,8,188,304,12,11,AMBER,0);
    else if(e->config.count){snprintf(s,sizeof(s),"Point %u  %s%s",e->selected+1,types[e->config.band[e->selected].type],e->config.band[e->selected].enabled?"":" OFF");text(p,s,8,188,304,13,11,AMBER,0);}
    else text(p,"B2 Add point",8,188,304,13,12,GREY,0);
    const char*normal_names[]={"Frequency","Gain","Q"};
    const char*shift_names[]={"Type","Point","Output"};
    const char*const*names=e->shifted?shift_names:normal_names;char values[3][32];
    if(e->config.count){const struct fx_eq_band*b=&e->config.band[e->selected];snprintf(values[0],32,"%.0f Hz",b->hz);snprintf(values[1],32,b->type>=FX_EQ_HIGH_PASS?"--":"%+.1f dB",b->gain_db);snprintf(values[2],32,"%.2f",b->q);}
    else for(unsigned k=0;k<3;++k)strcpy(values[k],"--");
    if(e->shifted){
        if(e->config.count){snprintf(values[0],32,"%s",types[e->config.band[e->selected].type]);snprintf(values[1],32,"%s",e->config.band[e->selected].enabled?"On":"Off");}
        snprintf(values[2],32,"%+.1f dB",e->config.output_db);
    }
    for(unsigned k=0;k<3;++k){text(p,names[k],(int)k*106,203,106,14,11,WHITE,1);text(p,values[k],(int)k*106,218,106,21,17,AMBER,1);}
}
