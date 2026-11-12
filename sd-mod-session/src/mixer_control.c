#include "mixer_control.h"
#include "fx_catalog.h"
#include "phone_input.h"
#include <time.h>
#define MIXER_HISTORY 16u
#include <math.h>
#include <stdlib.h>
#include <string.h>
struct mixer_control {
    struct mixer_control_ops ops;
    struct mixer_project model,desired,history[MIXER_HISTORY];
    unsigned history_start,history_count,history_pos;
    int history_request,history_step;
    struct phone_input name;
    unsigned naming;
    char name_preview[80];
    struct mixer_view_state view,back;
    struct mixer_meter_snapshot meter;
    struct mixer_fx_monitor fx_signal;
    uint64_t fx_signal_ms,name_since;
    unsigned name_target;
    uint64_t revision,epoch,ticket,submitted,present[2];
    double rate;
    unsigned active,shift,owned_buttons,owned_pads,held_tracks,temporary,dirty,exit_requested,fx_editing;
    unsigned pickup,last_fader_valid,last_fader;
    float anchors[8];
    unsigned catalog,parameter_page,fx_copied;
    struct fx_stage fx_clipboard;
    const char*message;
};
static unsigned history_index(const struct mixer_control*c,unsigned pos)
{return (c->history_start+pos)%MIXER_HISTORY;}
static void history_record(struct mixer_control*c,const struct mixer_project*p)
{
    c->history_count=c->history_pos+1;
    if(c->history_count==MIXER_HISTORY){c->history_start=(c->history_start+1)%MIXER_HISTORY;--c->history_count;}
    c->history_pos=c->history_count++;
    c->history[history_index(c,c->history_pos)]=*p;
}
static uint64_t name_time(void)
{
    struct timespec now={0};
    timespec_get(&now,TIME_UTC);
    return (uint64_t)now.tv_sec*1000+(unsigned long)now.tv_nsec/1000000;
}
static unsigned count(unsigned mask){unsigned n=0;while(mask){n+=mask&1;mask>>=1;}return n;}
static struct mixer_channel*selected(struct mixer_control*c){return c->view.master?&c->desired.master:c->desired.track+c->view.primary;}
static void pickup(struct mixer_control*c){c->pickup=0;c->last_fader_valid=0;}
static void changed(struct mixer_control*c){c->dirty=1;c->message="";}
static int one(const struct mixer_control*c){return c->view.master||count(c->view.selected)==1;}
static const struct mixer_comp_ops*comp_ops;
void mixer_control_set_comp_ops(const struct mixer_comp_ops*ops){comp_ops=ops;}
static int comp_visible(const struct mixer_control*c){return c->view.master&&comp_ops&&comp_ops->state;}
static int comp_pinned(const struct mixer_control*c){return comp_visible(c)&&c->view.fx_slot==MIXER_COMP_SLOT;}
static int comp_available(void){struct mixer_sample_comp k;memset(&k,0,sizeof(k));return comp_ops&&comp_ops->state&&comp_ops->state(&k)&&k.available;}
static void open_fx(struct mixer_control*);
static void clear_fx_page(struct mixer_control*c)
{
    c->catalog=c->view.fx_parameters=c->view.fx_delete=0;c->fx_signal_ms=0;
}
struct mixer_control*mixer_control_create(const struct mixer_control_ops*ops,double rate)
{
    if(!ops||!ops->submit||!isfinite(rate)||rate<8000||rate>192000)return NULL;
    struct mixer_control*c=calloc(1,sizeof(*c));if(!c)return NULL;
    c->ops=*ops;c->rate=rate;c->ticket=1;c->view.selected=1;c->message="";
    mixer_defaults(&c->model);c->desired=c->model;return c;
}
void mixer_control_destroy(struct mixer_control*c){free(c);}
int mixer_control_confirm(struct mixer_control*c,const struct mixer_project*p,uint64_t revision,uint64_t epoch)
{
    if(!c||!epoch||!mixer_project_valid(p)||(epoch==c->epoch&&revision<c->revision))return 0;
    int replacement=epoch!=c->epoch;
    if(replacement){mixer_control_close(c);c->epoch=epoch;c->submitted=c->dirty=0;}
    if(!replacement&&c->submitted&&revision<c->submitted)return 0;
    if(!replacement&&revision==c->revision)return 1;
    int own_ack=c->submitted&&revision==c->submitted;
    if(!own_ack){
        c->history_start=c->history_pos=0;c->history_count=1;
        c->history[0]=*p;c->history_step=c->history_request=0;c->naming=0;
    }else if(c->history_step){
        c->history_pos=(unsigned)((int)c->history_pos+c->history_step);c->history_step=0;
    }else if(memcmp(&c->model,p,sizeof(*p)))history_record(c,p);
    c->model=*p;c->revision=revision;c->submitted=0;
    if(!own_ack||!c->dirty)c->desired=*p;
    if(!own_ack){c->dirty=0;c->view.colour_edit=0;pickup(c);if(c->view.page==MIXER_KNOB_FX)open_fx(c);}
    return 1;
}
int mixer_control_open(struct mixer_control*c,int ready,int blocked)
{
    if(!c||!ready||blocked||!c->epoch)return 0;
    clear_fx_page(c);c->name_since=0;
    c->view.route_tab=c->view.route_focus=0;
    c->active=1;c->view.page=MIXER_OVERVIEW;c->temporary=0;c->held_tracks=0;
    c->view.cursor=c->view.scroll=c->view.colour_edit=c->view.fx_delete=0;c->message="";++c->ticket;pickup(c);return 1;
}
int mixer_control_reject(struct mixer_control*c,uint64_t revision,const char *reason)
{
    if(!c||!revision||revision!=c->submitted)return 0;
    c->submitted=c->dirty=0;c->history_step=c->history_request=0;c->desired=c->model;c->message=reason?reason:"Update failed";
    if(c->view.page==MIXER_KNOB_FX)open_fx(c);
    pickup(c);return 1;
}
void mixer_control_close(struct mixer_control*c)
{
    if(c){c->active=0;c->naming=0;c->temporary=0;c->held_tracks=0;c->shift=c->view.colour_edit=0;++c->ticket;pickup(c);}
}
int mixer_control_active(const struct mixer_control*c){return c&&c->active;}
uint64_t mixer_control_name_light(const struct mixer_control*c,unsigned pad)
{return c&&c->naming?phone_light(&c->name,pad+1):0;}
uint64_t mixer_control_ticket(const struct mixer_control*c){return c?c->ticket:0;}
/* Device-agreed layout (07.11.2026): one Mixer page with Track cells.
 * B1 = back to the Mixer from PAD FX/KNOB FX, from the Mixer exit to Sample;
 * Mute and B3 control the selected Tracks; B2 opens Route.
 * Shift+B2 renames the selected Track; Shift+B3 clears all solos.
 * Fader = Level, K1 = Pan, K3 = Colour. The last moved control sets the
 * encoder parameter. */
static void group_gate(struct mixer_control*c,int solo)
{
    if(c->view.master){c->message="Master has no Mute/Solo";return;}
    int all=1;for(unsigned i=0;i<8;++i)if(c->view.selected&(1u<<i))if(!(solo?c->desired.track[i].mix.solo:c->desired.track[i].mix.mute))all=0;
    for(unsigned i=0;i<8;++i)if(c->view.selected&(1u<<i)){
        if(solo)c->desired.track[i].mix.solo=!all;else c->desired.track[i].mix.mute=!all;}
    changed(c);
}
static void open_fx(struct mixer_control*c)
{
    struct mixer_channel*ch=selected(c);
    if(c->view.fx_slot!=MIXER_COMP_SLOT&&c->view.fx_slot>ch->fx.count)c->view.fx_slot=ch->fx.count;
    if(c->view.fx_slot==MIXER_COMP_SLOT&&!comp_visible(c))c->view.fx_slot=ch->fx.count;
    clear_fx_page(c);
    c->parameter_page=0;
    c->view.cursor=0;
}
static struct fx_eq_band mixer_eq_point_default(unsigned point,double rate)
{
    static const double hz[MIXER_EQ_POINTS]={100,250,4000,8000};
    double frequency=hz[point<MIXER_EQ_POINTS?point:MIXER_EQ_POINTS-1];
    return (struct fx_eq_band){fmin(frequency,rate*.45),0,1,FX_EQ_BELL,1};
}
static int fx_page_count(const struct fx_stage*stage)
{
    if(stage->kind==FX_STAGE_EQ)return stage->eq.count?stage->eq.count:1;
    unsigned n=fx_catalog_parameter_count(fx_catalog_find(stage->native_id));
    return n?(int)((n+5)/6):1;
}
static void select_fx_page(struct mixer_control*c,const struct fx_stage*stage,int delta,int wrap)
{
    int count=fx_page_count(stage);
    int next=(int)c->parameter_page+delta;
    if(wrap)next=(next%count+count)%count;
    else if(next<0)next=0;
    else if(next>=count)next=count-1;
    c->parameter_page=c->view.cursor=(unsigned)next;
}
static void browse_track(struct mixer_control*c,int previous)
{
    unsigned current=c->view.master?MIXER_TRACKS:c->view.primary;
    unsigned next=(current+(previous?MIXER_TRACKS:1))%(MIXER_TRACKS+1);
    c->view.master=next==MIXER_TRACKS;
    c->view.selected=c->view.master?0:1u<<next;
    if(!c->view.master)c->view.primary=next;
    else if(c->view.cursor>PARAM_PAN)c->view.cursor=PARAM_LEVEL;
    c->held_tracks=0;c->view.colour_edit=0;c->name_since=0;pickup(c);
    if(c->view.page==MIXER_KNOB_FX){
        int editing=c->view.fx_parameters;
        open_fx(c);
        c->view.fx_parameters=editing&&c->view.fx_slot<selected(c)->fx.count;
    }
}
void mixer_control_set_fx_editing(struct mixer_control*c,int on){if(c)c->fx_editing=on!=0;}
static int put_fx(struct mixer_control*c,const struct fx_stage*stage)
{
    struct fx_chain_config*chain=&selected(c)->fx;
    unsigned slot=c->view.fx_slot;
    if(slot<chain->count)chain->stage[slot]=*stage;
    else if(chain->count>=MIXER_FX_SLOTS||fx_chain_add(chain,stage,c->rate)!=FX_OK){
        c->message="FX chain full";return 0;
    }
    open_fx(c);changed(c);return 1;
}
static void each(struct mixer_control*c,void(*apply)(struct mixer_mix*,float),float value)
{
    if(c->view.master){apply(&c->desired.master.mix,value);changed(c);return;}
    for(unsigned i=0;i<8;++i)if(c->view.selected&(1u<<i))apply(&c->desired.track[i].mix,value);
    changed(c);
}
static void set_send(struct mixer_mix*m,float value){m->send=fmaxf(0,fminf(1,value));}
static void set_send_destination(struct mixer_control*c,unsigned destination)
{
    if(!mixer_set_send_to(&c->desired,c->view.selected,destination)){
        c->message="Send would create a loop";return;
    }
    changed(c);
}
static void set_level(struct mixer_mix*m,float db){m->level_db=db;}
static float step_level(float db,int delta)
{
    float next=(isfinite(db)?db:MIXER_LEVEL_MIN_DB-.5f)+delta*.5f;
    return next<MIXER_LEVEL_MIN_DB?-INFINITY:fminf(next,MIXER_LEVEL_MAX_DB);
}
static void set_pan(struct mixer_mix*m,float pan){m->pan=pan<-1?-1:pan>1?1:pan;}
static void set_colour(struct mixer_control*c,unsigned index)
{
    if(c->view.master){c->message="Master colour is fixed";return;}
    uint32_t rgb=mixer_colour_at(index<MIXER_COLOURS?index:MIXER_COLOURS-1);
    for(unsigned i=0;i<8;++i)if((c->view.selected&(1u<<i))&&c->desired.track[i].colour!=rgb){c->desired.track[i].colour=rgb;changed(c);}
}
static const struct mixer_mix*primary_mix(const struct mixer_control*c)
{return c->view.master?&c->desired.master.mix:&c->desired.track[c->view.primary].mix;}
/* Routing edits address one visible row, even if a group opened Route. */
static void route_track(struct mixer_control*c,unsigned track)
{
    c->view.master=track==MIXER_TRACKS;
    c->view.selected=c->view.master?0:1u<<track;
    if(!c->view.master)c->view.primary=track;
    c->held_tracks=0;pickup(c);
}
static void route_value(struct mixer_control*c,unsigned knob,int value)
{
    if(c->view.route_tab==MIXER_ROUTE_SENDS){
        if(knob==0){route_track(c,(unsigned)value);return;}
        if(c->view.master){c->message="Master has no send";return;}
        if(knob==1){
            if(!mixer_set_send_to(&c->desired,1u<<c->view.primary,(unsigned)value)){
                c->message="Send would create a loop";return;
            }
        }else c->desired.track[c->view.primary].mix.send=value/100.f;
        changed(c);
    }else{
        if(knob==2){route_track(c,(unsigned)value);return;}
        struct fx_chain_config *chain=&selected(c)->fx;
        if(knob==1){
            unsigned last=chain->count<MIXER_FX_SLOTS?chain->count:MIXER_FX_SLOTS-1;
            c->view.fx_slot=(unsigned)value>last?last:(unsigned)value;
            return;
        }
        if(c->view.fx_slot>=chain->count||
           chain->stage[c->view.fx_slot].kind!=FX_STAGE_SIDECHAIN){
            c->message="Select a Sidechain or Empty slot";return;
        }
        chain->stage[c->view.fx_slot].parameter[0]=value/8.f;
        changed(c);
    }
}
static int route_control(struct mixer_control*c,unsigned cc,unsigned value)
{
    int delta=value>64?(int)value-128:value==64?0:(int)value;
    unsigned knob=cc==0x64?(c->view.route_tab==MIXER_ROUTE_KEYS?1:c->view.route_focus):cc-0x10;
    if(knob>2)return 1;
    int maximum=8,current=0;
    if(c->view.route_tab==MIXER_ROUTE_SENDS){
        if(knob==0)current=c->view.master?8:(int)c->view.primary;
        else if(knob==1)current=c->desired.send_to[c->view.primary];
        else {maximum=100;current=(int)lroundf(c->desired.track[c->view.primary].mix.send*100);}
    }else{
        if(knob==2)current=c->view.master?8:(int)c->view.primary;
        else if(knob==1){maximum=3;current=(int)c->view.fx_slot;}
        else {
            const struct fx_chain_config *chain=&selected(c)->fx;
            if(c->view.fx_slot<chain->count&&chain->stage[c->view.fx_slot].kind==FX_STAGE_SIDECHAIN)
                current=(int)fx_sidechain_value(0,chain->stage[c->view.fx_slot].parameter[0]);
        }
    }
    int next=cc==0x64?current+delta:(int)((value*(unsigned)maximum+63)/127);
    if(next<0)next=0;
    if(next>maximum)next=maximum;
    if(cc==0x64&&!delta)return 1;
    c->view.route_focus=knob;c->message="";
    route_value(c,knob,next);
    return 1;
}
static void route_open_key(struct mixer_control*c)
{
    if(!c->fx_editing){c->message="Track FX unavailable";return;}
    struct fx_chain_config *chain=&selected(c)->fx;
    if(c->view.fx_slot>=chain->count){
        struct fx_stage stage;
        if(!fx_mixer_catalog_stage(fx_catalog_count(),c->rate,&stage)||
           fx_chain_add(chain,&stage,c->rate)!=FX_OK){c->message="FX chain full";return;}
        c->view.fx_slot=chain->count-1;changed(c);
        c->message="Sidechain added: Shift+K1 = Key";return;
    }
    if(chain->stage[c->view.fx_slot].kind!=FX_STAGE_SIDECHAIN){
        c->message="Select a Sidechain or Empty slot";return;
    }
    route_track(c,c->view.master?MIXER_TRACKS:c->view.primary);
    c->view.page=MIXER_KNOB_FX;c->temporary=2;open_fx(c);
    c->view.fx_parameters=1;
}
int mixer_control_button(struct mixer_control*c,enum mixer_action action,int down,uint64_t ticket)
{
    if(!c||(unsigned)action>MIXER_MUTE)return 0;
    unsigned bit=1u<<action;
    if(action==MIXER_SHIFT){
        if(c->shift!=(down!=0))pickup(c);
        c->shift=down!=0;
        return 0;
    }
    if(!down&&(c->owned_buttons&bit)){c->owned_buttons&=~bit;return 1;}
    if(down&&(c->owned_buttons&bit))return 1;
    int bank_action=action>=MIXER_BANK&&action<=MIXER_BANK_PREVIOUS;
    if(action==MIXER_ERASE&&!c->naming&&(c->view.page!=MIXER_KNOB_FX||!c->fx_editing))return 0;
    int bank_page=c->view.page==MIXER_ASSIGN||c->view.page==MIXER_INSPECT;
    int history=bank_action&&action!=MIXER_BANK&&c->shift&&!c->naming;
    if(bank_action&&!bank_page&&!history&&!c->naming&&
       !(action==MIXER_BANK&&c->view.page==MIXER_OVERVIEW)&&c->view.page!=MIXER_KNOB_FX)return 0;
    if(action==MIXER_NATIVE_MODE){if(down&&c->active)mixer_control_close(c);return 0;}
    if(!c->active)return 0;
    if(ticket!=c->ticket)return 1;
    if(!down)return 1;
    c->owned_buttons|=bit;c->message="";
    if(c->naming){
        if(action==MIXER_B1)c->naming=0;
        else if(action==MIXER_B3){
            char name[MIXER_NAME_BYTES]={0};
            if(!phone_utf8(&c->name,name,sizeof(name))){c->message="Name required (max 31 bytes)";return 1;}
            if(strcmp(selected(c)->name,name)){memcpy(selected(c)->name,name,sizeof(name));c->name_since=0;changed(c);}
            c->naming=0;
        }else if(action==MIXER_ERASE||action==MIXER_B2)phone_delete(&c->name,!c->name.enabled&&c->name.cursor<c->name.length);
        else if(action==MIXER_SELECT||action==MIXER_BANK_NEXT)phone_move(&c->name,1);
        else if(action==MIXER_BANK_PREVIOUS)phone_move(&c->name,-1);
        return 1;
    }
    if(c->view.fx_delete){
        if(action==MIXER_B1)c->view.fx_delete=MIXER_DELETE_NONE;
        else if(action==MIXER_B3){
            struct fx_chain_config *chain=&selected(c)->fx;
            if(c->view.fx_delete==MIXER_DELETE_POINT){
                if(c->view.fx_slot<chain->count){
                    struct fx_stage *stage=chain->stage+c->view.fx_slot;
                    unsigned point=c->parameter_page;
                    if(stage->kind==FX_STAGE_EQ&&point<stage->eq.count){
                        struct fx_eq_config *eq=&stage->eq;
                        --eq->count;
                        memmove(eq->band+point,eq->band+point+1,(eq->count-point)*sizeof(eq->band[0]));
                        eq->band[eq->count]=(struct fx_eq_band){0};
                        c->parameter_page=c->view.cursor=point<eq->count?point:eq->count?eq->count-1:0;
                        changed(c);
                    }
                }
                c->view.fx_delete=MIXER_DELETE_NONE;
            }else if(fx_chain_remove(chain,c->view.fx_slot)==FX_OK){open_fx(c);changed(c);}
        }
        return 1;
    }
    if(c->view.page==MIXER_INSPECT){
        if(action==MIXER_B2||action==MIXER_B3){
            c->view.route_tab=(c->view.route_tab+(action==MIXER_B2?MIXER_ROUTE_TABS-1:1))%MIXER_ROUTE_TABS;
            c->view.route_focus=c->view.route_tab==MIXER_ROUTE_KEYS?1:0;
            return 1;
        }
        if(action==MIXER_SELECT&&c->view.route_tab){
            if(c->view.route_tab==MIXER_ROUTE_KEYS)route_open_key(c);
            else c->view.route_focus=(c->view.route_focus+1)%3;
            return 1;
        }
    }
    if(action==MIXER_ERASE){
        const struct fx_chain_config *chain=&selected(c)->fx;
        if(!c->catalog&&c->view.fx_slot<chain->count){
            const struct fx_stage *stage=chain->stage+c->view.fx_slot;
            if(!c->view.fx_parameters)c->view.fx_delete=MIXER_DELETE_EFFECT;
            else if(stage->kind==FX_STAGE_EQ&&c->parameter_page<stage->eq.count)
                c->view.fx_delete=MIXER_DELETE_POINT;
        }
        return 1;
    }
    if(action==MIXER_MUTE){
        if(c->view.page==MIXER_INSPECT)return 1;
        if(!c->shift)group_gate(c,0);
        else for(unsigned i=0;i<MIXER_TRACKS;++i)if(c->desired.track[i].mix.mute){
            c->desired.track[i].mix.mute=0;changed(c);
        }
        return 1;
    }
    if(c->catalog&&(action==MIXER_B2||action==MIXER_B3))return 1;
    if(c->view.page==MIXER_KNOB_FX&&c->fx_editing&&!c->catalog&&comp_pinned(c)&&
       (action==MIXER_SELECT||action==MIXER_B2||action==MIXER_B3)){
        if(action==MIXER_SELECT){
            if(!c->view.fx_parameters){
                if(!comp_available())c->message="Open the compressor once (Shift+Pad 5)";
                else if(c->shift){if(comp_ops->bypass)comp_ops->bypass();} /* Shift+DATA: Bypass, as on the other slots */
                else c->view.fx_parameters=1;
            }
        }else if(c->view.fx_parameters){
            if(action==MIXER_B2){
                if(c->shift){selected(c)->fx_bypass^=1;changed(c);}
                else if(comp_available()&&comp_ops->bypass)comp_ops->bypass();
            }else if(comp_available()){
                if(c->shift&&comp_ops->warm)comp_ops->warm();
                else if(!c->shift&&comp_ops->type)comp_ops->type();
            }
        }
        return 1;
    }
    if(history){
        if(c->view.page==MIXER_INSPECT)return 1;
        c->history_request=action==MIXER_BANK_PREVIOUS?-1:1;return 1;
    }
    if(bank_action&&c->view.page==MIXER_INSPECT&&c->view.route_tab){
        if(action==MIXER_BANK){
            unsigned track=c->view.master?8:c->view.primary;
            route_track(c,(track+(c->shift?8:1))%9);
        }
        return 1;
    }
    if(bank_action){
        int inspect=c->view.page==MIXER_INSPECT;
        int bank_key=action==MIXER_BANK;
        int previous=action==MIXER_BANK_PREVIOUS||(bank_key&&c->shift);
        if(inspect?bank_key:!bank_key){
            browse_track(c,previous);
        }else if(bank_page){
            c->view.bank=(c->view.bank+(previous?7:1))%8;
            c->view.sound=c->view.bank*16+c->view.sound%16;
        }
    }else if(action==MIXER_B1){
        if(c->view.page==MIXER_KNOB_FX&&(c->catalog||c->view.fx_parameters)){open_fx(c);return 1;}
        if(c->view.page!=MIXER_OVERVIEW){c->view.page=MIXER_OVERVIEW;c->temporary=0;c->view.cursor=PARAM_LEVEL;}
        else{c->exit_requested=1;mixer_control_close(c);}
    }else if(c->view.page==MIXER_KNOB_FX&&c->fx_editing&&
             action==MIXER_B2&&!c->view.fx_parameters){
        struct fx_chain_config*chain=&selected(c)->fx;
        if(!c->shift){
            if(c->view.fx_slot>=chain->count)c->message="Select an effect to copy";
            else{c->fx_clipboard=chain->stage[c->view.fx_slot];c->fx_copied=1;c->message="Effect copied";}
        }else if(!c->fx_copied)c->message="No effect copied";
        else if(put_fx(c,&c->fx_clipboard))c->message="Effect pasted";
    }else if(action==MIXER_B3&&c->shift&&c->view.page==MIXER_KNOB_FX&&c->view.fx_parameters&&
             c->view.fx_slot<selected(c)->fx.count&&selected(c)->fx.stage[c->view.fx_slot].kind==FX_STAGE_EQ){
        struct fx_eq_config*eq=&selected(c)->fx.stage[c->view.fx_slot].eq;
        if(eq->count>=MIXER_EQ_POINTS)c->message="Four EQ points maximum";
        else{
            unsigned point=eq->count++;
            eq->band[point]=mixer_eq_point_default(point,c->rate);
            c->parameter_page=c->view.cursor=point;changed(c);
        }
    }else if(action==MIXER_B2&&c->view.page==MIXER_KNOB_FX&&c->view.fx_parameters&&!c->shift){
        selected(c)->fx.stage[c->view.fx_slot].bypass^=1;changed(c);
    }else if(action==MIXER_B2&&c->view.page==MIXER_KNOB_FX&&c->view.fx_parameters&&c->fx_editing){
        selected(c)->fx_bypass^=1;changed(c); /* Shift+B2 on an effect page: Chain Bypass */
    }else if(action==MIXER_B2&&c->view.page==MIXER_OVERVIEW){
        if(!c->shift){
            clear_fx_page(c);c->view.page=MIXER_INSPECT;c->temporary=1;return 1;
        }
        if(!one(c)){c->message="Select one Track";return 1;}
        phone_init(&c->name,MIXER_NAME_BYTES-1);
        if(phone_sync(&c->name,selected(c)->name,0))c->naming=1;
    }else if(action==MIXER_B2&&c->view.page==MIXER_ASSIGN&&!c->shift){
        c->view.pad_lock^=1;
    }else if(action==MIXER_B3){
        if(c->view.page==MIXER_KNOB_FX&&c->fx_editing){
            if(c->shift){if(!c->view.fx_parameters){selected(c)->fx_bypass^=1;changed(c);}}
            else group_gate(c,1);
        }else if(c->view.page==MIXER_OVERVIEW&&c->shift){
            for(unsigned i=0;i<MIXER_TRACKS;++i)if(c->desired.track[i].mix.solo){
                c->desired.track[i].mix.solo=0;changed(c);
            }
        }else if(c->view.page==MIXER_OVERVIEW||c->view.page==MIXER_ASSIGN)group_gate(c,1);
    }else if(action==MIXER_PAD_FX){
        if(c->view.page==MIXER_ASSIGN||c->view.page==MIXER_INSPECT){c->view.page=MIXER_OVERVIEW;c->temporary=0;return 1;}
        if(!c->shift&&!one(c)){c->message="Select one Track";return 1;}
        clear_fx_page(c);c->view.page=c->shift?MIXER_INSPECT:MIXER_ASSIGN;c->temporary=1;
        c->view.pad_lock=1;
    }else if(action==MIXER_KNOB){
        if(c->view.page==MIXER_KNOB_FX){clear_fx_page(c);c->view.page=MIXER_OVERVIEW;c->temporary=0;c->view.cursor=PARAM_LEVEL;return 1;}
        if(c->fx_editing&&!one(c)){c->message="Select one Track";return 1;}
        c->view.page=MIXER_KNOB_FX;c->temporary=2;c->view.fx_slot=0;open_fx(c);
        if(!c->fx_editing)c->message="Track FX: coming later";
    }else if(action==MIXER_SELECT){
        if(c->view.page==MIXER_OVERVIEW||c->view.page==MIXER_ASSIGN){
            unsigned last=c->view.master?PARAM_PAN:PARAM_COLOUR;
            c->view.cursor=c->view.cursor>=last?PARAM_LEVEL:c->view.cursor+1;
        }
        else if(c->view.page==MIXER_KNOB_FX&&c->fx_editing){struct mixer_channel*ch=selected(c);
            if(c->catalog){struct fx_stage stage;
                if(fx_mixer_catalog_stage(fx_mixer_filtered_index(c->view.catalog_filter,c->view.cursor),c->rate,&stage)){
                    if(stage.kind==FX_STAGE_EQ)stage.eq.count=1;
                    put_fx(c,&stage);
                }
            }else if(c->view.fx_slot==ch->fx.count){c->catalog=1;c->view.cursor=0;}
            else if(c->view.fx_parameters)select_fx_page(c,ch->fx.stage+c->view.fx_slot,1,1);
            else if(c->shift){ch->fx.stage[c->view.fx_slot].bypass^=1;changed(c);}
            else c->view.fx_parameters=1;
        }
    }
    return 1;
}
int mixer_control_pad(struct mixer_control*c,unsigned pad,int down,uint64_t ticket)
{
    if(!c||pad>=16)return 0;
    unsigned bit=1u<<pad;
    if(!down&&(c->owned_pads&bit)){if(c->naming)phone_pad(&c->name,pad+1,0,name_time());c->owned_pads&=~bit;if(pad<8)c->held_tracks&=~bit;return 1;}
    if(down&&(c->owned_pads&bit))return 1;
    if(!c->active)return 0;
    if(ticket!=c->ticket)return 1;
    if(!down)return 1;
    c->owned_pads|=bit;
    if(c->naming){c->name.shift=c->shift;phone_pad(&c->name,pad+1,1,name_time());return 1;}
    int fx_pad=pad>=8&&pad<8+MIXER_FX_SLOTS;
    if(c->view.page==MIXER_KNOB_FX&&(c->view.fx_delete||(c->view.fx_parameters&&!fx_pad)))return 1;
    if(c->view.page==MIXER_INSPECT&&c->view.route_tab){
        if(pad<8||pad==15)route_track(c,pad==15?MIXER_TRACKS:pad);
        else if(c->view.route_tab==MIXER_ROUTE_KEYS&&fx_pad){
            route_value(c,1,(int)(pad-8));c->view.route_focus=1;
        }
        return 1;
    }
    if(c->view.page==MIXER_ASSIGN||c->view.page==MIXER_INSPECT){unsigned sound=c->view.bank*16+pad;c->view.sound=sound;
        if(c->view.page==MIXER_ASSIGN){
            unsigned current=c->desired.output[sound];
            if(!(c->present[sound/64]&(UINT64_C(1)<<(sound%64))))c->message="Empty pad";
            else if(c->view.pad_lock)c->message="Pad Lock: press B2 to unlock";
            else if(one(c)){
                unsigned target=c->view.master?0:c->view.primary+1;
                unsigned next=current==target?0:target;
                if(next!=current){c->desired.output[sound]=(unsigned char)next;changed(c);}
            }
        }
        return 1;
    }
    if(c->fx_editing&&fx_pad&&
       (c->view.page==MIXER_OVERVIEW||c->view.page==MIXER_KNOB_FX)){
        if(!one(c)){c->message="Select one Track";return 1;}
        unsigned slot=pad-8,used=selected(c)->fx.count;
        if(slot>used){c->message="Select the first free FX slot";return 1;}
        c->view.page=MIXER_KNOB_FX;c->temporary=2;c->view.fx_slot=slot;
        open_fx(c);
        if(slot<used)c->view.fx_parameters=1;
        else{c->catalog=1;c->view.cursor=0;}
        c->message="";
        return 1;
    }
    if(c->view.page==MIXER_KNOB_FX&&c->fx_editing){
        c->message="";
        if(pad<8){c->view.primary=pad;c->view.selected=bit;c->view.master=0;c->view.fx_slot=0;}
        else if(pad==15){c->view.master=1;c->view.selected=0;c->view.fx_slot=0;}
        else return 1;
        open_fx(c);return 1;
    }
    if(pad<8){
        if(c->shift&&!c->view.master){
            /* Shift: add or remove one Track; the last Track stays selected. */
            unsigned next=c->view.selected^bit;
            if(next){c->view.selected=next;if(next&bit)c->view.primary=pad;
                else if(c->view.primary==pad){for(unsigned i=0;i<8;++i)if(next&(1u<<i)){c->view.primary=i;break;}}}
        }else if(!c->held_tracks||c->view.master){c->view.selected=bit;c->view.primary=pad;}
        else c->view.selected|=bit;
        c->held_tracks|=bit;c->view.master=0;pickup(c);
    }else if(pad==15){
        c->view.master=1;c->view.selected=0;c->held_tracks=0;
        if(c->view.cursor>PARAM_PAN)c->view.cursor=PARAM_LEVEL;
        pickup(c);
    }
    c->view.colour_edit=0;
    if(c->view.page==MIXER_KNOB_FX&&(pad<8||pad==15))open_fx(c);
    return 1;
}
float mixer_fader_db(unsigned value)
{
    if(!value)return -INFINITY;
    if(value>127)value=127;
    return value<=103?-60.f+(value-1)*60.f/102.f:(value-103)*.25f;
}
static void move_fader(struct mixer_control*c,unsigned value)
{
    unsigned mask=c->view.selected;
    float db=mixer_fader_db(value);
    if(c->view.master||!(mask&(mask-1))||c->shift){
        each(c,set_level,db);
        return;
    }
    if(!c->last_fader_valid){
        c->last_fader=value;
        for(unsigned i=0;i<MIXER_TRACKS;++i)
            if(mask&(1u<<i))c->anchors[i]=c->desired.track[i].mix.level_db;
        c->last_fader_valid=1;
        return;
    }
    /* Keep moving the rest of the group when one track reaches its limit.
     * Anchors stay fixed so reversing the fader restores the original balance. */
    float offset=mixer_fader_db(value?value:1)-mixer_fader_db(c->last_fader?c->last_fader:1);
    for(unsigned i=0;i<MIXER_TRACKS;++i)if(mask&(1u<<i)){
        float anchor=c->anchors[i];
        float level=-INFINITY;
        if(value&&(isfinite(anchor)||offset>0))
            level=fmaxf(MIXER_LEVEL_MIN_DB,fminf(MIXER_LEVEL_MAX_DB,
                        fmaxf(MIXER_LEVEL_MIN_DB,anchor)+offset));
        if(c->desired.track[i].mix.level_db!=level){
            c->desired.track[i].mix.level_db=level;
            changed(c);
        }
    }
}
static int effect_parameter_index(const struct mixer_control*c,const struct fx_catalog_entry*entry,
                                  unsigned knob,int shift)
{
    if(shift&&!fx_catalog_shift_layer(entry,c->parameter_page))shift=0;
    return fx_catalog_parameter_index(entry,c->parameter_page*6+(shift?3:0)+knob);
}
static void reset_fx_parameter(struct mixer_control*c,unsigned knob,int shift)
{
    struct mixer_channel*channel=selected(c);
    if(!c->view.fx_parameters||c->view.fx_slot>=channel->fx.count)return;
    struct fx_stage*stage=channel->fx.stage+c->view.fx_slot;
    if(stage->kind==FX_STAGE_EQ){
        if(!stage->eq.count){
            if(shift&&knob==2){stage->eq.output_db=0;changed(c);}
            return;
        }
        unsigned point=c->parameter_page<stage->eq.count?c->parameter_page:0;
        struct fx_eq_band*b=stage->eq.band+point;
        struct fx_eq_band defaults=mixer_eq_point_default(point,c->rate);
        if(shift){
            if(knob==0)b->enabled=defaults.enabled;
            else if(knob==1)b->q=defaults.q;
            else stage->eq.output_db=0;
        }else if(knob==0)b->hz=defaults.hz;
        else if(knob==1)b->gain_db=defaults.gain_db;
        else b->type=defaults.type;
    }else{
        const struct fx_catalog_entry*entry=fx_catalog_find(stage->native_id);
        int index=effect_parameter_index(c,entry,knob,shift);
        if(index<0)return;
        stage->parameter[index]=entry->defaults[index];
    }
    changed(c);
}
int mixer_control_reset(struct mixer_control*c,unsigned cc,int shift,uint64_t ticket)
{
    /* Pad Bank held + control: default value for the whole selection. */
    if(!c||!c->active)return 0;
    if(ticket!=c->ticket||c->naming||c->view.fx_delete)return 1;
    if(c->view.page==MIXER_KNOB_FX&&cc>=0x10&&cc<=0x12){
        if(c->view.fx_parameters&&comp_pinned(c)){if(comp_available()&&comp_ops->reset)comp_ops->reset(shift?1u:0u,cc-0x10);return 1;}
        reset_fx_parameter(c,cc-0x10,shift);return 1;
    }
    if(c->view.page==MIXER_INSPECT&&c->view.route_tab){
        if(c->view.route_tab==MIXER_ROUTE_SENDS&&(cc==0x11||cc==0x12))
            route_value(c,cc-0x10,0);
        else if(c->view.route_tab==MIXER_ROUTE_KEYS&&cc==0x10)route_value(c,0,0);
        return 1;
    }
    if(c->view.page!=MIXER_OVERVIEW&&c->view.page!=MIXER_ASSIGN&&
       !(c->view.page==MIXER_KNOB_FX&&cc==0x14))return 1;
    if(cc==0x14){pickup(c);each(c,set_level,0);if(c->view.page!=MIXER_KNOB_FX)c->view.cursor=PARAM_LEVEL;}
    else if(cc==0x10){each(c,set_pan,0);c->view.cursor=PARAM_PAN;}
    else if(cc==0x11&&!c->view.master){
        if(shift)set_send_destination(c,0);else each(c,set_send,0);
        c->view.cursor=PARAM_SEND;
    }
    else if(cc==0x12&&!c->view.master){set_colour(c,MIXER_DEFAULT_TRACK_COLOUR);c->view.cursor=PARAM_COLOUR;}
    return 1;
}
static int fx_cc(struct mixer_control*c,unsigned cc,unsigned value)
{
    struct mixer_channel*ch=selected(c);
    if(c->view.fx_delete)return 1;
    if(cc==0x64){int delta=value>64?(int)value-128:value==64?0:(int)value;
        if(!delta)return 1;
        if(c->shift&&!c->catalog&&!c->view.fx_parameters){
            unsigned from=c->view.fx_slot;
            if(from<ch->fx.count){
                int to=(int)from+delta;
                if(to<0)to=0;
                unsigned last=ch->fx.count-1;
                if(last>=MIXER_FX_SLOTS)last=MIXER_FX_SLOTS-1;
                if((unsigned)to>last)to=(int)last;
                if((unsigned)to!=from&&fx_chain_move(&ch->fx,from,(unsigned)to)==FX_OK){
                    c->view.fx_slot=(unsigned)to;changed(c);
                }
            }
            return 1;
        }
        if(!c->catalog&&!c->view.fx_parameters){
            unsigned last=ch->fx.count<MIXER_FX_SLOTS?ch->fx.count:MIXER_FX_SLOTS-1;
            int top=(int)last+(comp_visible(c)?1:0);
            int next=(c->view.fx_slot==MIXER_COMP_SLOT?(int)last+1:(int)c->view.fx_slot)+delta;
            if(next<0)next=0;
            if(next>top)next=top;
            c->view.fx_slot=next>(int)last?MIXER_COMP_SLOT:(unsigned)next;
            open_fx(c);return 1;
        }
        if(c->catalog){
            int next=(int)c->view.cursor+delta;
            int last=(int)fx_mixer_filtered_count(c->view.catalog_filter)-1;
            c->view.cursor=(unsigned)(next<0?0:next>last?last:next);
        }else if(c->view.fx_slot<ch->fx.count){
            const struct fx_stage*stage=ch->fx.stage+c->view.fx_slot;
            select_fx_page(c,stage,delta,stage->kind==FX_STAGE_EQ);
        }
        return 1;
    }
    if(!one(c)){c->message="Select one Track";return 1;}
    if(c->catalog){
        if(cc==0x10)c->view.cursor=value*fx_mixer_filtered_count(c->view.catalog_filter)/128;
        else if(cc==0x12){ /* K3: category filter (All, Filter, Dynamics ...) */
            unsigned f=value*fx_mixer_category_count()/128;
            if(f!=c->view.catalog_filter){c->view.catalog_filter=f;c->view.cursor=0;}
        }
        return 1;
    }
    if(c->view.fx_parameters&&comp_pinned(c)){
        if(cc>=0x10&&cc<=0x12&&comp_available()&&comp_ops->knob)comp_ops->knob(c->shift?1u:0u,cc-0x10,value);
        return 1;
    }
    if(!c->view.fx_parameters||c->view.fx_slot>=ch->fx.count)return 1;
    struct fx_stage*s=ch->fx.stage+c->view.fx_slot;
    if(s->kind!=FX_STAGE_EQ){
        int index=effect_parameter_index(c,fx_catalog_find(s->native_id),cc-0x10,c->shift);
        if(index>=0){
            float v=value/127.f;unsigned n=fx_catalog_choice_count(fx_catalog_find(s->native_id),(unsigned)index);
            if(n>1)v=roundf(v*(float)(n-1))/(float)(n-1); /* named choices: whole steps only */
            s->parameter[index]=v;changed(c);}
    }else{
        if(!s->eq.count){
            if(c->shift&&cc==0x12){s->eq.output_db=-24+value*36./127;changed(c);}
            return 1;
        }
        struct fx_eq_band*b=s->eq.band+(c->parameter_page<s->eq.count?c->parameter_page:0);
        if(c->shift&&cc==0x10)b->enabled=value>=64;
        else if(c->shift&&cc==0x11)b->q=.2*pow(80,value/127.);
        else if(c->shift&&cc==0x12)s->eq.output_db=-24+value*36./127;
        else if(cc==0x10)b->hz=20*pow(1000,value/127.);
        else if(cc==0x11)b->gain_db=-18+value*36./127;
        else b->type=value*6/128;
        if(b->hz>c->rate*.45)b->hz=c->rate*.45;
        if(fx_eq_valid(&s->eq,c->rate))changed(c);
    }
    return 1;
}
int mixer_control_cc(struct mixer_control*c,unsigned cc,unsigned value,uint64_t ticket)
{
    if(!c||!c->active)return 0;
    if(cc!=0x10&&cc!=0x11&&cc!=0x12&&cc!=0x14&&cc!=0x64)return 0;
    if(ticket!=c->ticket||value>127)return 1;
    if(c->naming){
        if(cc==0x10&&c->name.enabled!=(value>=64))phone_enable(&c->name,value>=64);
        if(cc==0x64){
            int delta=value>64?(int)value-128:value==64?0:(int)value;
            if(c->name.enabled||c->shift)phone_move(&c->name,delta);
            else phone_turn_character(&c->name,delta);
        }
        return 1;
    }
    if(c->view.page==MIXER_INSPECT){
        if(c->view.route_tab)return route_control(c,cc,value);
        if(cc==0x12){c->view.bank=value*8/128;c->view.sound=c->view.bank*16+c->view.sound%16;}
        else if(cc==0x64){int delta=value>64?(int)value-128:(int)value;
            int sound=(int)c->view.sound+delta;if(sound<0)sound=0;if(sound>127)sound=127;c->view.sound=(unsigned)sound;c->view.bank=(unsigned)sound/16;}
        return 1; /* Inspect is read-only, including Fader and K1. */
    }
    /* PAD FX (Select pads): the enlarged Track on the left keeps Fader, K1,
     * K3 and the encoder exactly as on the Mixer page; banks via Bank/+/-. */
    if(cc==0x14&&!c->view.fx_delete){
        move_fader(c,value);
        if(c->view.page!=MIXER_KNOB_FX)c->view.cursor=PARAM_LEVEL;
        return 1;
    }
    if(c->view.page==MIXER_KNOB_FX&&c->fx_editing)return fx_cc(c,cc,value);
    if(c->view.page!=MIXER_OVERVIEW&&c->view.page!=MIXER_ASSIGN)return 1;
    if(cc==0x10){each(c,set_pan,value==64?0:((float)value-64)/(value<64?64:63));c->view.cursor=PARAM_PAN;return 1;}
    if(cc==0x12){set_colour(c,value*MIXER_COLOURS/128);if(!c->view.master)c->view.cursor=PARAM_COLOUR;return 1;}
    if(cc==0x11){
        if(c->view.master)return 1;
        c->view.cursor=PARAM_SEND;
        if(c->shift)set_send_destination(c,value*9/128);
        else each(c,set_send,value/127.f);
        return 1;
    }
    int delta=value>64?(int)value-128:value==64?0:(int)value;if(!delta)return 1;
    const struct mixer_mix*m=primary_mix(c);
    if(c->view.cursor==PARAM_LEVEL){
        pickup(c);
        if(one(c)||c->shift){
            each(c,set_level,step_level(m->level_db,delta));
        }else for(unsigned i=0;i<MIXER_TRACKS;++i)if(c->view.selected&(1u<<i)){
            float *level=&c->desired.track[i].mix.level_db;
            float next=step_level(*level,delta);
            if(next!=*level){*level=next;changed(c);}
        }
    }
    else if(c->view.cursor==PARAM_PAN)each(c,set_pan,m->pan+delta*.02f);
    else if(c->view.cursor==PARAM_SEND&&!c->view.master){
        if(c->shift){
            int to=c->desired.send_to[c->view.primary]+delta;
            set_send_destination(c,to<0?0:to>8?8:(unsigned)to);
        }else each(c,set_send,m->send+delta*.01f);
    }
    else if(c->view.cursor==PARAM_COLOUR&&!c->view.master){int index=mixer_colour_index(c->desired.track[c->view.primary].colour);
        int next=index<0?0:index+delta;if(next<0)next=0;if(next>=(int)MIXER_COLOURS)next=MIXER_COLOURS-1;set_colour(c,(unsigned)next);}
    return 1;
}
int mixer_control_exit_requested(struct mixer_control*c)
{
    if(!c||!c->exit_requested)return 0;
    c->exit_requested=0;return 1;
}
void mixer_control_tick(struct mixer_control*c)
{
    if(!c||c->submitted)return;
    if(!c->dirty&&c->history_request){
        int next=(int)c->history_pos+c->history_request;
        if(next<0||next>=(int)c->history_count)c->message=c->history_request<0?"Nothing to undo":"Nothing to redo";
        else{
            c->history_step=c->history_request;
            c->desired=c->history[history_index(c,(unsigned)next)];c->dirty=1;pickup(c);
            if(c->view.page==MIXER_KNOB_FX)open_fx(c);
            c->message=c->history_step<0?"Mixer undo":"Mixer redo";
        }
        c->history_request=0;
    }
    if(!c->dirty)return;
    uint64_t revision=0;
    if(c->ops.submit(c->ops.user,&c->desired,c->revision,&revision)&&revision>c->revision){c->submitted=revision;c->dirty=0;}
    else c->message="Audio update pending";
}
void mixer_control_presence(struct mixer_control*c,uint64_t lo,uint64_t hi,unsigned bank)
{
    if(c&&bank<8){c->present[0]=lo;c->present[1]=hi;
        if(c->view.bank!=bank)c->view.sound=bank*16+c->view.sound%16;
        c->view.bank=bank;}
}
int mixer_control_led(const struct mixer_control*c,unsigned pad,uint32_t*rgb)
{
    if(!c||!c->active||pad>=16||!rgb||!mixer_view_sound_pads(&c->view))return 0;
    unsigned sound=c->view.bank*16+pad,output=c->model.output[sound];
    if(!(c->present[sound/64]&(UINT64_C(1)<<(sound%64))))*rgb=0;
    else if(!output)*rgb=0x181818;
    else{uint32_t colour=c->model.track[output-1].colour;
        *rgb=!c->view.master&&output==c->view.primary+1?colour:((colour&0xfcfcfc)>>2);}
    return 1;
}
int mixer_control_cursor(const struct mixer_control*c,unsigned pad)
{
    /* Device feedback: no Track colours on the pads. 2 = sound on the selected
     * Track (Master when Master is selected), shown as the blinking white
     * cursor; 1 = other loaded sound; 0 = empty. Uses the desired routing so
     * several quick presses all show before audio confirms them. */
    if(!c||!c->active||pad>=16||!mixer_view_sound_pads(&c->view))return -1;
    unsigned sound=c->view.bank*16+pad,output=c->desired.output[sound];
    unsigned target=c->view.master?0:c->view.primary+1;
    if(!(c->present[sound/64]&(UINT64_C(1)<<(sound%64))))return 0;
    return output==target?2:1;
}
int mixer_control_output(const struct mixer_control*c,unsigned sound)
{
    return c&&sound<MIXER_PADS?(int)c->desired.output[sound]:-1;
}
int mixer_control_set_output(struct mixer_control*c,unsigned sound,unsigned output)
{
    if(!c||sound>=MIXER_PADS||output>8||
       (c->active&&c->view.page==MIXER_ASSIGN&&c->view.pad_lock))return 0;
    if(c->desired.output[sound]!=output){c->desired.output[sound]=(unsigned char)output;changed(c);}
    return 1;
}
void mixer_control_fx_monitor(struct mixer_control*c,const struct mixer_fx_monitor*signal,uint64_t now)
{
    c->fx_signal=*signal;c->fx_signal_ms=now;
}
void mixer_control_meter(struct mixer_control*c,const struct mixer_meter_snapshot*m,uint64_t now)
{
    if(c&&m&&m->epoch==c->epoch){c->meter=*m;c->view.meter_ms=now;}
}
void mixer_control_view(struct mixer_control*c,uint64_t now,struct mixer_view_state*out)
{
    if(c&&out){*out=c->view;memset(&out->comp,0,sizeof(out->comp));
        if(comp_visible(c)){out->comp.visible=1;comp_ops->state(&out->comp);out->comp.visible=1;}
        out->fx_signal=c->fx_signal_ms&&now>=c->fx_signal_ms&&now-c->fx_signal_ms<=250?&c->fx_signal:NULL;out->project=c->view.page==MIXER_KNOB_FX||(c->view.page==MIXER_INSPECT&&c->view.route_tab)||c->view.cursor==PARAM_SEND?&c->desired:&c->model;out->meter=&c->meter;out->epoch=c->epoch;out->now_ms=now;
        out->revision=c->revision;out->pending=c->submitted||c->dirty;out->fx_catalog=c->catalog;out->parameter_page=c->parameter_page;
        out->message=c->message;out->pickup=!c->pickup;out->present[0]=c->present[0];out->present[1]=c->present[1];}
    if(c&&out){
        unsigned target=(unsigned)c->view.page*9+(c->view.master?8:c->view.primary);
        if(now&&(!c->name_since||target!=c->name_target||now<c->name_since)){
            c->name_since=now;c->name_target=target;
        }
        out->name_elapsed=now>=c->name_since?now-c->name_since:0;
        if(c->naming){
            phone_tick(&c->name,name_time());
            phone_view(&c->name,13,c->name_preview,sizeof(c->name_preview));
            out->name_edit=c->name_preview;out->name_pads=c->name.enabled;
        }
        out->shifted=c->shift;out->fx_editing=c->fx_editing;out->rate=c->rate;}
}
const char*mixer_control_message(const struct mixer_control*c){return c?c->message:"";}
