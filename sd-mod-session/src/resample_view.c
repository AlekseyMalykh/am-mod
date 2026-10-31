#include "resample_view.h"
#include "fx_catalog.h"
#include <stdio.h>
#define WHITE 0xffffffffu
#define GREY 0xffaaaaaau
#define AMBER 0xffffc247u
#define CYAN 0xff39c1d6u
#define RED 0xffff4040u
static void text(const struct resample_painter*p,const char*s,int x,int y,int w,int h,unsigned size,uint32_t colour,int centre){p->text(p->user,s,x,y,w,h,size,colour,centre);}
static void cell(const struct resample_painter*p,unsigned k,const char*name,const char*value){
    text(p,name,(int)k*106,202,106,15,12,GREY,1);text(p,value,(int)k*106,219,106,20,18,WHITE,1);
}
static void duration(char*out,size_t capacity,const char*label,uint64_t frames,double rate){
    double seconds=rate?(double)frames/rate:0;
    if(seconds>0&&seconds<.01)snprintf(out,capacity,"%s %.2f ms",label,seconds*1000);
    else snprintf(out,capacity,"%s %.2f s",label,seconds);
}
void resample_view_paint(const struct resample_flow*f,const struct resample_painter*p){
    if(!f||!p||!p->rect||!p->text||f->page==RS_CLOSED)return;
    p->rect(p->user,0,5,320,235,0xff000000u);
    for(unsigned k=0;k<3;++k)text(p,resample_flow_button_label(f,k),(int)k*106,7,106,24,15,k==0?RED:WHITE,1);
    text(p,f->page==RS_CHOOSE?"Resample":"Resample Sample",8,40,304,25,21,WHITE,0);
    char s[112];snprintf(s,sizeof(s),"%c%02u  %s",'A'+f->spec.source/16,f->spec.source%16+1,f->has_sample?f->sample:"Empty");
    if(f->page==RS_CHOOSE){
        text(p,"Selected sound",8,78,304,17,13,GREY,0);text(p,s,8,98,304,24,18,AMBER,0);
        text(p,"Sequence",8,144,304,17,13,GREY,0);text(p,f->sequence,8,164,304,24,18,WHITE,0);
        text(p,f->message[0]?f->message:"Choose Seq or Sample",8,214,304,20,14,f->message[0]?AMBER:GREY,0);return;
    }
    text(p,s,8,75,304,23,17,AMBER,0);
    if(f->page==RS_FX){
        for(unsigned k=0;k<FX_CHAIN_MAX;++k){
            char label[64];const struct fx_stage*e=k<f->spec.fx.count?&f->spec.fx.stage[k]:NULL;
            snprintf(label,sizeof(label),"%u  %s%s",k+1,e?fx_catalog_stage_name(e):"Empty",e&&e->bypass?"  Bypass":"");
            text(p,label,8,101+(int)k*20,304,20,16,k==f->fx_slot?AMBER:e?WHITE:GREY,0);
        }
        snprintf(s,sizeof(s),"FX %u/%u  Shift+B2/B3: move",f->spec.fx.count,FX_CHAIN_MAX);
        text(p,f->message[0]?f->message:s,8,211,304,22,13,f->message[0]?AMBER:GREY,0);return;
    }
    if(f->page==RS_SAMPLE){
        duration(s,sizeof(s),"Input",f->capture_total,f->rate);text(p,s,8,113,304,23,17,WHITE,0);
        snprintf(s,sizeof(s),"FX %u/%u",f->spec.fx.count,FX_CHAIN_MAX);text(p,s,8,143,304,23,17,WHITE,0);
        snprintf(s,sizeof(s),"%u",f->spec.repeats);cell(p,0,"Repeats",s);
        snprintf(s,sizeof(s),"%.2f s",f->spec.tail_ms/1000.);cell(p,1,"Tail",s);
        if(f->velocity_supported){snprintf(s,sizeof(s),"%u",f->spec.velocity);cell(p,2,"Velocity",s);}
    }else if(f->page==RS_READY||f->page==RS_SWITCHING||f->page==RS_CONFIRM){
        if(f->page==RS_CONFIRM){
            unsigned d=resample_flow_destination(f);snprintf(s,sizeof(s),"Replace %c%02u?",'A'+d/16,d%16+1);
            text(p,s,8,112,304,26,21,RED,0);text(p,"This pad is occupied",8,145,304,23,16,WHITE,0);
        }else{
            text(p,f->page==RS_SWITCHING?"Switching...":!f->compare_supported?"Result ready":f->compare_rendered?"Rendered in mix":"Original in mix",8,113,304,25,20,CYAN,0);
            duration(s,sizeof(s),"Result",f->total,f->rate);text(p,s,8,145,304,22,16,WHITE,0);
        }
        cell(p,0,"Destination",f->replace?"Replace":"To Pad");
        snprintf(s,sizeof(s),"%c",'A'+f->target_bank);cell(p,1,"Bank",s);
        snprintf(s,sizeof(s),"%02u",f->target_pad+1);cell(p,2,"Pad",s);
    }else{
        const char*label=f->page==RS_RENDERING?(f->captured<f->capture_total?"Reading sample...":"Processing..."):
            f->page==RS_PREPARING?"Preparing sample...":f->page==RS_CANCELLING?"Restoring original...":f->page==RS_APPLYING?"Applying...":"Resample failed";
        text(p,label,8,115,304,25,20,f->page==RS_ERROR?RED:WHITE,0);
        if(f->page==RS_RENDERING){
            uint64_t total=f->capture_total+f->total,done=f->captured+f->done;unsigned width=total?(unsigned)((double)done/total*304):0;if(width>304)width=304;
            p->rect(p->user,8,155,304,5,0xff333333u);p->rect(p->user,8,155,(int)width,5,CYAN);
            snprintf(s,sizeof(s),"%u%%",total?(unsigned)((double)done/total*100):0);text(p,s,8,165,304,22,17,WHITE,0);
        }
    }
    if(f->message[0])text(p,f->message,8,181,304,18,12,f->page==RS_ERROR?RED:AMBER,0);
}
