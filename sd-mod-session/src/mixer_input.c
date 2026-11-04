#include "mixer_input.h"
#include <string.h>
int mixer_input_action(unsigned code, enum mixer_action *mapped)
{
    enum mixer_action action;
    switch(code){
    case 0x00:action=MIXER_KNOB;break;
    case 0x01:action=MIXER_PAD_FX;break;
    case 0x0c:action=MIXER_B1;break;
    case 0x0d:action=MIXER_B2;break;
    case 0x0e:action=MIXER_B3;break;
    case 0x6f:action=MIXER_SELECT;break;
    case 0x31:action=MIXER_SHIFT;break;
    case 0x06:case 0x07:action=MIXER_NATIVE_MODE;break;
    default:return 0;
    }
    if (mapped) *mapped = action;
    return 1;
}
int mixer_input_button(struct mixer_control*c,unsigned code,int down,uint64_t ticket)
{
    enum mixer_action action;
    return mixer_input_action(code, &action) && mixer_control_button(c, action, down, ticket);
}
void mixer_entry_init(struct mixer_entry*g,unsigned hold_ms)
{
    if(g){memset(g,0,sizeof(*g));g->hold_ms=hold_ms?hold_ms:MIXER_ENTRY_HOLD_MS;}
}
void mixer_entry_cancel(struct mixer_entry*g){if(g)g->armed=0;}
static int valid(const struct mixer_entry*g,uint64_t now,uint64_t ticket,int ready,int blocked)
{
    return g->ticket==ticket&&ready&&!blocked&&now>=g->started_ms;
}
enum mixer_entry_action mixer_entry_tick(struct mixer_entry*g,uint64_t now,uint64_t ticket,int ready,int blocked)
{
    if(!g||!g->held||!g->armed)return MIXER_ENTRY_PASS;
    if(!valid(g,now,ticket,ready,blocked)){g->armed=0;return MIXER_ENTRY_CANCELLED;}
    if(now-g->started_ms<g->hold_ms)return MIXER_ENTRY_CONSUME;
    g->armed=0;g->native_down=1;return MIXER_ENTRY_LONG_DOWN;
}
enum mixer_entry_action mixer_entry_pad(struct mixer_entry*g,unsigned pad,int down,
    uint64_t now,uint64_t ticket,int shift,int ready,int blocked)
{
    if(!g||pad!=8)return MIXER_ENTRY_PASS;
    if(down){
        if(g->held)return MIXER_ENTRY_CONSUME;
        if(!shift||!ready||blocked||!ticket)return MIXER_ENTRY_PASS;
        g->held=g->armed=1;g->native_down=0;g->started_ms=now;g->ticket=ticket;
        return MIXER_ENTRY_CONSUME;
    }
    if(!g->held)return MIXER_ENTRY_PASS;
    g->held=0;
    if(g->native_down){g->native_down=0;return MIXER_ENTRY_NATIVE_UP;}
    if(!g->armed)return MIXER_ENTRY_CONSUME;
    g->armed=0;
    if(!valid(g,now,ticket,ready,blocked))return MIXER_ENTRY_CANCELLED;
    return now-g->started_ms<g->hold_ms?MIXER_ENTRY_OPEN:MIXER_ENTRY_LONG_PAIR;
}
