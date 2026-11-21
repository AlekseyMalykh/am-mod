#include "midi_voice.h"
#include <string.h>

static struct am_midi_voice *find(struct am_midi_voices *state,uint64_t id)
{
    if(!id)return NULL;
    for(unsigned i=0;i<AM_MIDI_VOICES;++i)if(state->voices[i].id==id)return state->voices+i;
    return NULL;
}
static int send(am_midi_queue queue,void *user,unsigned port,unsigned status,
                 unsigned number,unsigned value)
{
    struct am_midi_packet packet={port,{3,{(uint8_t)status,(uint8_t)number,(uint8_t)value}}};
    return queue(user,&packet)!=0;
}
int am_midi_voice_on(struct am_midi_voices *state,uint64_t id,uint32_t scope,
                     unsigned port,unsigned channel,unsigned base,
                     const struct am_midi_event *event,am_midi_queue queue,void *user)
{
    if(!state||!queue||!event||!id||(port!=AM_MIDI_USB&&port!=AM_MIDI_EXTERNAL)
        ||!channel||channel>16||event->velocity>127)return -1;
    unsigned note;
    struct am_midi_voice *prior=find(state,id);
    if(prior)return prior->released?-1:0;
    if(!event->velocity||!am_midi_event_target(event,base,&note))return 0;
    struct am_midi_target target={port,{3,{(uint8_t)(0x90|(channel-1)),(uint8_t)note,event->velocity}},
                                      {3,{(uint8_t)(0x80|(channel-1)),(uint8_t)note,0}}};
    return am_midi_voice_target_on(state,id,scope,&target,queue,user);
}
int am_midi_voice_target_on(struct am_midi_voices *state,uint64_t id,uint32_t scope,
                           const struct am_midi_target *target,am_midi_queue queue,void*user)
{
    if(!state||!target||!queue||!id||(target->port!=AM_MIDI_USB&&target->port!=AM_MIDI_EXTERNAL))return -1;
    const struct am_midi_message*m=&target->on;unsigned type=m->data[0]&0xf0;
    if((type!=0x90&&type!=0xb0&&type!=0xc0)||m->size!=(type==0xc0?2:3)||m->data[1]>127||(m->size==3&&m->data[2]>127))return -1;
    if(type==0x90&&!m->data[2])return 0;
    unsigned port=target->port,channel=(m->data[0]&15)+1,note=m->data[1];
    unsigned kind=type==0x90?AM_MIDI_NOTE:type==0xb0?AM_MIDI_CC:AM_MIDI_PROGRAM;
    if(kind!=AM_MIDI_PROGRAM&&(target->off.size!=3||target->off.data[0]!=(uint8_t)((kind==AM_MIDI_CC?0xb0:0x80)|(channel-1))||target->off.data[1]!=note||target->off.data[2]))return -1;
    struct am_midi_voice *prior=find(state,id);if(prior)return prior->released?-1:0;
    uint32_t bit=UINT32_C(1)<<((port-1)*16+channel-1);
    if((state->sustain_pending|state->panic_pending)&bit)return -1;
    struct am_midi_voice *voice=NULL;
    for(unsigned i=0;i<AM_MIDI_VOICES;++i){
        struct am_midi_voice *candidate=state->voices+i;
        if(candidate->id&&candidate->released&&candidate->port==port
            &&candidate->channel==channel&&candidate->note==note&&candidate->kind==kind)return -1;
        if(!candidate->id&&!voice)voice=candidate;
    }
    if(!voice)return -1;
    uint16_t *gate=kind==AM_MIDI_NOTE?&state->gates[port-1][channel-1][note]:kind==AM_MIDI_CC?&state->cc_gates[port-1][channel-1][note]:NULL;
    /* MIDI 1.0 cannot distinguish overlapping voices on one channel/key.
     * Share one balanced gate until the final owner releases it. */
    /* CC values are updates; Note gates suppress duplicate retriggers. */
    if(kind!=AM_MIDI_NOTE||!*gate){struct am_midi_packet packet={port,*m};if(!queue(user,&packet))return -1;}
    if(gate)++*gate;
    *voice=(struct am_midi_voice){id,scope,(uint8_t)port,(uint8_t)channel,(uint8_t)note,0,(uint8_t)kind};
    return 1;
}
int am_midi_voice_off(struct am_midi_voices *state,uint64_t id,am_midi_queue queue,void *user)
{
    if(!state||!queue)return -1;
    struct am_midi_voice *voice=find(state,id);if(!voice)return 0;
    uint16_t *gate=voice->kind==AM_MIDI_CC?&state->cc_gates[voice->port-1][voice->channel-1][voice->note]:&state->gates[voice->port-1][voice->channel-1][voice->note];
    voice->released=1;
    if(voice->kind==AM_MIDI_PROGRAM){memset(voice,0,sizeof *voice);return 1;}
    if(*gate==1&&!send(queue,user,voice->port,(voice->kind==AM_MIDI_CC?0xb0:0x80)|(voice->channel-1),voice->note,0))return -1;
    if(*gate)--*gate;
    memset(voice,0,sizeof *voice);return 1;
}
int am_midi_voice_pressure(struct am_midi_voices *state,uint64_t id,unsigned value,
                           am_midi_queue queue,void *user)
{
    if(!state||!queue||value>127)return -1;
    struct am_midi_voice *voice=find(state,id);if(!voice||voice->released||voice->kind!=AM_MIDI_NOTE)return 0;
    return send(queue,user,voice->port,0xa0|(voice->channel-1),voice->note,value)?1:-1;
}
static int retry_controls(struct am_midi_voices *state,am_midi_queue queue,void *user)
{
    int ok=1;
    for(unsigned i=0;i<32;++i){uint32_t bit=UINT32_C(1)<<i;
        if(state->sustain_pending&bit){
            if(send(queue,user,1+i/16,0xb0|(i%16),64,0))state->sustain_pending&=~bit;
            else{ok=0;continue;}
        }
        if(state->panic_pending&bit){
            if(send(queue,user,1+i/16,0xb0|(i%16),123,0))state->panic_pending&=~bit;
            else ok=0;
        }
    }
    return ok;
}
int am_midi_voices_retry(struct am_midi_voices *state,am_midi_queue queue,void *user)
{
    if(!state||!queue)return -1;
    int ok=1;
    for(unsigned i=0;i<AM_MIDI_VOICES;++i)if(state->voices[i].id&&state->voices[i].released)
        if(am_midi_voice_off(state,state->voices[i].id,queue,user)<0)ok=0;
    return retry_controls(state,queue,user)&&ok?1:-1;
}
int am_midi_voices_release_scope(struct am_midi_voices *state,uint32_t scope,
                                am_midi_queue queue,void *user)
{
    if(!state||!queue)return -1;
    int ok=1;
    for(unsigned i=0;i<AM_MIDI_VOICES;++i)if(state->voices[i].id&&state->voices[i].scope==scope)
        if(am_midi_voice_off(state,state->voices[i].id,queue,user)<0)ok=0;
    return ok?1:-1;
}
int am_midi_voices_panic(struct am_midi_voices *state,am_midi_queue queue,void *user)
{
    if(!state||!queue)return -1;
    int ok=1;
    for(unsigned i=0;i<AM_MIDI_VOICES;++i)if(state->voices[i].id){
        struct am_midi_voice *voice=state->voices+i;
        uint32_t bit=UINT32_C(1)<<((voice->port-1)*16+voice->channel-1);
        state->sustain_pending|=bit;state->panic_pending|=bit;
        if(am_midi_voice_off(state,voice->id,queue,user)<0)ok=0;
    }
    return retry_controls(state,queue,user)&&ok?1:-1;
}
