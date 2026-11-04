#include "mixer_view.h"
#include "fx_catalog.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define BLACK 0xff000000u
#define WHITE 0xffffffffu
#define GREY 0xff777777u
#define DARK 0xff252525u
#define AMBER 0xffffc247u
static void rect(const struct resample_painter*p,int x,int y,int w,int h,uint32_t c)
{
    if(w>0&&h>0)p->rect(p->user,x,y,w,h,c);
}
static void text(const struct resample_painter*p,const char*s,int x,int y,int w,int h,unsigned size,uint32_t c,int centre)
{
    p->text(p->user,s,x,y,w,h,size,c,centre);
}
static void border(const struct resample_painter*p,int x,int y,int w,int h,uint32_t c)
{
    rect(p,x,y,w,1,c);rect(p,x,y+h-1,w,1,c);rect(p,x,y,1,h,c);rect(p,x+w-1,y,1,h,c);
}
int mixer_view_gain_y(float db)
{
    if(!isfinite(db))return 177;
    if(db<-60)db=-60;
    if(db>6)db=6;
    return db>0?114-(int)lroundf(db*2):114+(int)lroundf(-db);
}
float mixer_view_gain_db(int y)
{
    if(y>=177)return -INFINITY;
    if(y<=102)return 6;
    if(y<=114)return (114-y)*.5f;
    return y>=174?-60.f:(float)(114-y);
}
static void level(char*out,size_t size,float db)
{
    if(db==0)db=0.f;
    if(!isfinite(db))snprintf(out,size,"OFF");else snprintf(out,size,"%+.1f",(double)db);
}
static void pan(char*out,size_t size,float position)
{
    int percent=(int)lroundf(position*100);
    if(!percent)snprintf(out,size,"C");
    else snprintf(out,size,"%c%d",percent<0?'L':'R',percent<0?-percent:percent);
}
static void short_name(char*out,const char*name,unsigned points)
{
    unsigned n=0,used=0;
    while(name[n]&&used<points){unsigned char c=(unsigned char)name[n];unsigned bytes=c<128?1:c<224?2:c<240?3:4;
        if(n+bytes>=MIXER_NAME_BYTES)break;
        memcpy(out+n,name+n,bytes);n+=bytes;++used;
    }
    out[n]=0;
}
static const struct mixer_mix*mix(const struct mixer_view_state*s,unsigned track,struct mixer_mix resolved[8])
{
    if(track==8)return &s->project->master.mix;
    mixer_scene_resolve(s->project,s->part_id,resolved);return resolved+track;
}
static uint32_t colour(const struct mixer_channel*c)
{
    unsigned rgb=c->colour;unsigned brightness=((rgb>>16)&255)+((rgb>>8)&255)+(rgb&255);
    return brightness<150?WHITE:0xff000000u|rgb;
}
static int single(const struct mixer_view_state*s)
{return s->master||(s->selected&&!(s->selected&(s->selected-1)));}
static void colour_label(char*out,size_t size,uint32_t rgb)
{
    const char*name=mixer_colour_name(rgb);
    if(name)snprintf(out,size,"%s",name);else snprintf(out,size,"#%06X",rgb);
}
static int available(const struct mixer_view_state*s,unsigned bus)
{
    return s->meter&&s->meter->epoch==s->epoch&&s->now_ms>=s->meter_ms&&s->now_ms-s->meter_ms<=250&&
        (s->meter->valid_mask&(1u<<bus));
}
static void meter(const struct mixer_view_state*s,unsigned bus,int x,int w,int peak_x,int peak_w,const struct resample_painter*p)
{
    rect(p,x,102,w,76,BLACK);rect(p,peak_x,182,peak_w,13,BLACK);
    if(w<10)rect(p,x-10,126,18,12,BLACK);
    if(!available(s,bus)){text(p,"N/A",w<10?x-10:x,126,w<10?18:w,12,8,GREY,1);return;}
    for(unsigned c=0;c<2;++c){float db=mixer_meter_dbfs(s->meter->peak[bus][c]);
        int h=isfinite(db)?(int)lroundf((fmaxf(-60,fminf(0,db))+60)*76/60):0;
        int bar=(w-1)/2;rect(p,x+(int)c*(bar+1),102,bar,76,DARK);
        rect(p,x+(int)c*(bar+1),178-h,bar,h,AMBER);
    }
    text(p,"P",peak_x,182,peak_w,13,10,(s->meter->clip_mask&(1u<<bus))?0xffff4747u:GREY,1);
}
void mixer_view_channel_strip(const struct mixer_view_state*s,unsigned track,int x,int width,const struct resample_painter*p)
{
    const struct mixer_channel*c=track<8?s->project->track+track:&s->project->master;
    struct mixer_mix resolved[8];const struct mixer_mix*m=mix(s,track,resolved);
    int compact=width==40,selected=track==8?s->master:!s->master&&!!(s->selected&(1u<<track));
    int primary=track==8?s->master:!s->master&&s->primary==track;
    rect(p,x,50,width,148,BLACK);rect(p,x+width-1,50,1,148,DARK);
    rect(p,x+2,50,width-4,2,0xff000000u|c->colour);
    if(selected)border(p,x+1,53,width-2,144,colour(c));
    char label[48],name[MIXER_NAME_BYTES];
    if(track==8)snprintf(label,sizeof(label),"MASTER");else snprintf(label,sizeof(label),"%s%c",primary?"*":"",'A'+track);
    text(p,label,x+2,54,width-4,12,compact?10:12,primary?AMBER:WHITE,1);
    short_name(name,c->name,compact?3:8);text(p,name,x+2,67,width-4,12,10,WHITE,1);
    level(label,sizeof(label),m->level_db);text(p,label,x+2,80,width-4,11,10,AMBER,1);
    pan(label,sizeof(label),m->pan);text(p,label,x+2,91,width-4,10,8,WHITE,1);
    int rail=x+(compact?12:24),thumb=mixer_view_gain_y(m->level_db);
    rect(p,rail,102,2,76,GREY);rect(p,rail-5,114,12,1,WHITE);
    text(p,"0",rail-10,108,8,10,8,GREY,1);
    rect(p,rail-5,thumb-2,12,5,selected?AMBER:WHITE);
    int meter_x=x+(compact?29:53),meter_w=compact?7:17;
    meter(s,track==8?9:track,meter_x,meter_w,x+width-12,10,p);
    if(track<8){text(p,"M",x+3,182,10,13,10,m->mute?AMBER:GREY,1);
        text(p,"S",x+(compact?14:25),182,10,13,10,m->solo?AMBER:GREY,1);}
}
static void chrome(const struct mixer_view_state*s,const struct resample_painter*p)
{
    rect(p,0,0,320,50,BLACK);const char*tab[]={"B1 MIX","B2 FX","B3 ROUTE"};
    for(unsigned i=0;i<3;++i){int x=i==2?213:(int)i*106,w=i==0?106:107;
        int active=s->page==(enum mixer_page)(i+1);
        text(p,tab[i],x,6,w,22,14,active?AMBER:WHITE,1);if(active)rect(p,x+4,28,w-8,2,AMBER);}
    char title[80],db[24];level(db,sizeof(db),s->project->master.mix.level_db);
    unsigned group=0;for(unsigned i=0;i<8;++i)if(s->selected&(1u<<i))++group;
    if(group>1)snprintf(title,sizeof(title),"GROUP %u  %c  M %s%s",group,'A'+s->primary,db,s->pending?" ...":"");
    else snprintf(title,sizeof(title),"%s  dB     M %s%s",s->master?"MASTER":"MIXER",db,s->pending?"  ...":"");
    text(p,title,4,33,312,15,11,WHITE,0);
    rect(p,0,198,320,42,BLACK);struct mixer_mix resolved[8];const struct mixer_mix*m=mix(s,s->master?8:s->primary,resolved);
    pan(title,sizeof(title),m->pan);
    text(p,"Pan",4,200,98,12,11,AMBER,1);text(p,title,4,215,98,18,14,WHITE,1);
    text(p,"Send",110,200,98,12,11,GREY,1);text(p,"Disabled",110,215,98,18,12,GREY,1);
    if(s->page==MIXER_MIX){
        colour_label(title,sizeof(title),s->master?s->project->master.colour:s->project->track[s->primary].colour);
        text(p,"Colour",217,200,99,12,11,single(s)?AMBER:GREY,1);
        text(p,single(s)?title:"One track",217,215,99,18,12,single(s)?WHITE:GREY,1);
    }else{
        text(p,"Select",217,200,99,12,11,AMBER,1);text(p,s->pickup?"PICKUP":s->master?"Master":"Track",217,215,99,18,14,WHITE,1);
    }
}
void mixer_view_overview(const struct mixer_view_state*s,const struct resample_painter*p)
{
    for(unsigned i=0;i<8;++i)mixer_view_channel_strip(s,i,(int)i*40,40,p);
}
static void row(const struct mixer_view_state*s,unsigned index,const char*label,const char*value,int disabled,const struct resample_painter*p)
{
    int y=57+(int)index*26;uint32_t ink=disabled?GREY:s->cursor==index?AMBER:WHITE;
    if(s->cursor==index&&!disabled)border(p,84,y-2,232,25,AMBER);
    text(p,label,90,y,124,20,12,ink,0);text(p,value,214,y,96,20,12,ink,1);
}
void mixer_view_context(const struct mixer_view_state*s,const struct resample_painter*p)
{
    rect(p,80,50,240,148,BLACK);rect(p,79,50,1,148,GREY);
    const struct mixer_channel*c=s->master?&s->project->master:s->project->track+s->primary;
    struct mixer_mix resolved[8];const struct mixer_mix*m=mix(s,s->master?8:s->primary,resolved);char value[40];
    if(s->page==MIXER_MIX){
        struct mixer_view_state visible=*s;visible.cursor=s->cursor>=s->scroll?s->cursor-s->scroll:6;
        for(unsigned i=0;i<5&&s->scroll+i<6;++i){
            unsigned index=s->scroll+i;
            if(index==0){pan(value,sizeof(value),m->pan);row(&visible,i,"Pan",value,0,p);}
            else if(index==1)row(&visible,i,"Send","Disabled",1,p);
            else if(index==2)row(&visible,i,"Mute",s->master?"--":m->mute?"On":"Off",s->master,p);
            else if(index==3)row(&visible,i,"Solo",s->master?"--":m->solo?"On":"Off",s->master,p);
            else if(index==4)row(&visible,i,"FX Bypass",c->fx_bypass?"On":"Off",0,p);
            else{
                colour_label(value,sizeof(value),c->colour);row(&visible,i,"Colour",single(s)?value:"One track",!single(s),p);
                if(single(s)){int y=57+(int)i*26;border(p,217,y+4,12,12,GREY);rect(p,218,y+5,10,10,0xff000000u|c->colour);}
            }
        }
        rect(p,317,57,2,129,DARK);rect(p,317,57+(s->scroll?22:0),2,107,AMBER);
        if(s->message&&*s->message)text(p,s->message,88,185,226,11,9,AMBER,0);
        else if(s->cursor==5)text(p,s->colour_edit?"Encoder: Colour   Press: Done":"K3: Colour   Press: Edit",88,185,226,11,9,single(s)?AMBER:GREY,0);
    }else if(s->page==MIXER_FX){
        for(unsigned i=0;i<5;++i){unsigned slot=s->scroll+i;if(slot>=5)break;
            char label[64];snprintf(label,sizeof(label),"%u. %s",slot+1,slot<c->fx.count?fx_catalog_stage_name(c->fx.stage+slot):"Empty");
            row(s,i,label,slot<c->fx.count?(c->fx_bypass||c->fx.stage[slot].bypass?"Bypass":"On"):"--",0,p);}
    }else if(s->page==MIXER_ROUTE){unsigned assigned=0,direct=0;
        for(unsigned pad=0;pad<128;++pad){if(!s->project->output[pad])++direct;else if(s->project->output[pad]==s->primary+1)++assigned;}
        snprintf(value,sizeof(value),"%u",s->master?direct:assigned);row(s,0,"Pad outputs",value,0,p);
        snprintf(value,sizeof(value),"Bank %c",'A'+s->bank);row(s,1,"Sound bank",value,0,p);
        row(s,2,"Output","Master",0,p);row(s,3,"PAD FX","Assign",0,p);row(s,4,"Banks A-H","128 pads",0,p);
    }else if(s->page==MIXER_ADVANCED){row(s,0,"Advanced FX","Disabled",1,p);row(s,1,"Sidechain","Disabled",1,p);
    }else if(s->page==MIXER_ASSIGN||s->page==MIXER_INSPECT){
        row(s,0,s->page==MIXER_ASSIGN?"Assign sounds":"Inspect sounds","PAD FX",0,p);
        snprintf(value,sizeof(value),"%c%02u",'A'+s->sound/16,s->sound%16+1);row(s,1,"Sound",value,0,p);
        unsigned output=s->project->output[s->sound<128?s->sound:0];
        if(output)snprintf(value,sizeof(value),"Track %c",'A'+output-1);else snprintf(value,sizeof(value),"Master");
        row(s,2,"Output",value,0,p);snprintf(value,sizeof(value),"%c",'A'+s->bank);row(s,3,"Bank",value,0,p);
        row(s,4,s->message&&*s->message?s->message:"LED routing",s->pending?"Pending":"",0,p);
        rect(p,0,198,320,42,BLACK);text(p,s->page==MIXER_INSPECT?"INSPECT: Read only":"Pads: Toggle Track output",4,201,312,15,12,AMBER,1);
        text(p,"Encoder: Sound    K3: Bank",4,220,312,14,11,WHITE,1);
    }else if(s->fx_catalog){unsigned first=s->cursor/5*5;
        for(unsigned i=0;i<5;++i){const struct fx_catalog_entry*e=fx_catalog_at(first+i);if(!e)break;
            struct mixer_view_state row_state=*s;row_state.cursor=s->cursor-first;row(&row_state,i,e->name,e->category,0,p);}
        rect(p,0,198,320,42,BLACK);text(p,"K1 / Encoder: Select effect",4,200,312,15,12,AMBER,1);
        text(p,"Encoder press: Add / Replace",4,219,312,15,11,WHITE,1);
    }else if(s->fx_slot<c->fx.count){const struct fx_stage*fx=c->fx.stage+s->fx_slot;
        text(p,fx_catalog_stage_name(fx),88,52,224,20,13,AMBER,0);
        const struct fx_catalog_entry*e=fx->kind==FX_STAGE_NATIVE?fx_catalog_find(fx->native_id):NULL;
        unsigned indices[32],n=0;if(e)for(unsigned i=0;i<e->count;++i)if(e->parameter[i]&&*e->parameter[i])indices[n++]=i;
        for(unsigned k=0;k<3;++k){const char*label="--";strcpy(value,"--");
            if(e){unsigned position=s->parameter_page*3+k;if(position<n){label=e->parameter[indices[position]];snprintf(value,sizeof(value),"%.1f%%",(double)fx->parameter[indices[position]]*100);}}
            else if(fx->eq.count){const struct fx_eq_band*b=fx->eq.band+(s->parameter_page<fx->eq.count?s->parameter_page:0);
                if(s->shifted){static const char*types[]={"Bell","Low Shelf","High Shelf","High Pass","Low Pass","Notch"};
                    label=k==0?"Type":k==1?"Band":"EQ Output";
                    if(k==0)snprintf(value,sizeof(value),"%s",b->type<6?types[b->type]:"--");
                    else if(k==1)snprintf(value,sizeof(value),"%s",b->enabled?"On":"Off");
                    else snprintf(value,sizeof(value),"%+.1f dB",fx->eq.output_db);
                }else{label=k==0?"Freq":k==1?"Gain":"Q";snprintf(value,sizeof(value),k==0?"%.0f Hz":k==1?"%+.1f dB":"%.2f",k==0?b->hz:k==1?b->gain_db:b->q);}}
            int y=80+(int)k*27; text(p,label,88,y,132,21,12,WHITE,0);text(p,value,220,y,94,21,12,AMBER,1);
            rect(p,(int)k*106,198,k==2?108:106,42,BLACK);text(p,label,(int)k*106+3,200,100,13,10,WHITE,1);text(p,value,(int)k*106+3,216,100,18,13,AMBER,1);
        }
        snprintf(value,sizeof(value),"%u",s->parameter_page+1);row(s,4,fx->kind==FX_STAGE_EQ?"EQ band":"Parameter page",value,0,p);
    }else row(s,0,"Effect","Empty",0,p);
}
void mixer_view_meters(const struct mixer_view_state*s,unsigned mask,const struct resample_painter*p)
{
    if(!s||!p||!p->rect||!p->text)return;
    if(s->page==MIXER_OVERVIEW){for(unsigned i=0;i<8;++i)if(mask&(1u<<i))meter(s,i,(int)i*40+29,7,(int)i*40+28,10,p);}
    else {unsigned track=s->master?8:s->primary;if(mask&(1u<<track))meter(s,track==8?9:track,53,17,68,10,p);}
}
void mixer_view_paint(const struct mixer_view_state*s,const struct resample_painter*p)
{
    if(!s||!s->project||!p||!p->rect||!p->text||s->primary>=8||s->bank>=8||s->page>MIXER_INSPECT)return;
    chrome(s,p);
    if(s->page==MIXER_OVERVIEW)mixer_view_overview(s,p);
    else {mixer_view_channel_strip(s,s->master?8:s->primary,0,80,p);mixer_view_context(s,p);}
}
