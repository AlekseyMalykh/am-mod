#include "src/mixer_buses.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct mixer_project project;
static void compare(float actual,float expected){assert(fabsf(actual-expected)<.00002f);}
static float value(struct mixer_buses*b,unsigned bus,unsigned ch,unsigned sample){struct mixer_pcm_view v;assert(mixer_buses_view(b,bus,&v)&&sample<v.frames);return v.channel[ch][sample];}
static struct mixer_buses*prepare(void){enum mixer_bus_result error;struct mixer_buses*b=mixer_buses_prepare(&project,0,48000,128,NULL,&error);assert(b&&error==MIXER_BUS_OK);return b;}
static void routing(void)
{
    mixer_defaults(&project);
    for(unsigned i=0;i<8;++i)assert(mixer_set_output(&project,i*16,i+1)==MIXER_OK);
    struct mixer_buses*b=prepare();struct mixer_meters*m=mixer_meters_create(48000);assert(m);
    /* The audio graph owns a prepared snapshot, not the mutable UI model. */
    project.output[0]=8;project.track[0].mix.level_db=-30;
    float l[128],r[128];struct mixer_pcm_view view;
    assert(!mixer_buses_view(b,0,&view));assert(mixer_buses_begin(b,128)==MIXER_BUS_OK);
    for(unsigned track=0;track<8;++track){for(unsigned i=0;i<128;++i){l[i]=(float)(track+1)/64;r[i]=-l[i];}
        assert(mixer_buses_accumulate(b,track*16,l,r,2,0)==MIXER_BUS_OK);assert(l[0]==(float)(track+1)/64);}
    for(unsigned i=0;i<128;++i)l[i]=.125f;
    assert(mixer_buses_accumulate(b,127,l,NULL,1,0)==MIXER_BUS_OK);
    assert(mixer_buses_finish(b,m,1)==MIXER_BUS_OK);
    for(unsigned track=0;track<8;++track){compare(value(b,track,0,0),(float)(track+1)/64);compare(value(b,track,1,127),-(float)(track+1)/64);}
    compare(value(b,MIXER_METER_DIRECT,0,0),.125f);compare(value(b,MIXER_METER_DIRECT,1,0),.125f);
    compare(value(b,MIXER_METER_MASTER,0,0),.6875f);compare(value(b,MIXER_METER_MASTER,1,127),-.4375f);
    struct mixer_meter_snapshot meter;assert(mixer_meters_take(m,1,&meter)&&meter.valid_mask==1023);
    compare(meter.peak[MIXER_METER_MASTER][0],.6875f);compare(meter.peak[0][1],1.f/64);
    /* More than one voice/pad adds to one Track; other banks share the one
     * project Output table. Master Direct still contains only direct pads. */
    assert(mixer_buses_begin(b,8)==MIXER_BUS_OK);
    for(unsigned i=0;i<8;++i)l[i]=.25f;
    assert(mixer_buses_accumulate(b,0,l,NULL,1,0)==MIXER_BUS_OK);
    assert(mixer_buses_accumulate(b,0,l,NULL,1,0)==MIXER_BUS_OK);
    assert(mixer_buses_finish(b,NULL,0)==MIXER_BUS_OK);
    compare(value(b,0,0,0),.5f);compare(value(b,MIXER_METER_DIRECT,0,0),0);
    compare(value(b,MIXER_METER_MASTER,0,0),.5f);
    assert(mixer_buses_pcm_bytes(b)==128*22*sizeof(float));
    mixer_meters_destroy(m);mixer_buses_destroy(b);
    puts("PASS all eight independent stereo Tracks, all-bank Pad mapping, polyphonic accumulation, Direct isolation, final sum and real bus meter taps");
}
static void controls(void)
{
    mixer_defaults(&project);project.output[0]=1;project.output[1]=2;
    project.track[0].mix.level_db=-6.0206f;project.track[0].mix.pan=1;
    project.track[1].mix.level_db=-INFINITY;project.master.mix.level_db=-6.0206f;
    struct mixer_buses*b=prepare();float pcm[]={.5f,.25f};
    assert(mixer_buses_begin(b,2)==MIXER_BUS_OK);
    assert(mixer_buses_accumulate(b,0,pcm,NULL,1,0)==MIXER_BUS_OK);
    assert(mixer_buses_accumulate(b,1,pcm,NULL,1,0)==MIXER_BUS_OK);
    assert(mixer_buses_finish(b,NULL,0)==MIXER_BUS_OK);
    compare(value(b,0,0,0),0);compare(value(b,0,1,0),.25f);
    compare(value(b,1,0,0),0);compare(value(b,MIXER_METER_MASTER,1,0),.125f);
    mixer_buses_destroy(b);
    project.track[0].mix.pan=0;project.track[0].mix.level_db=0;project.track[0].mix.mute=1;
    project.track[1].mix.level_db=0;project.track[1].mix.solo=1;project.master.mix.level_db=0;
    struct mixer_scene*s=project.scene;s->part_id=11;s->fields[0]=MIXER_SCENE_MUTE|MIXER_SCENE_SOLO;
    s->track[0].mute=0;s->track[0].solo=1;project.scene_count=1;
    enum mixer_bus_result error;b=mixer_buses_prepare(&project,11,48000,128,NULL,&error);assert(b);
    assert(mixer_buses_begin(b,2)==MIXER_BUS_OK);
    for(unsigned pad=0;pad<3;++pad)assert(mixer_buses_accumulate(b,pad,pcm,NULL,1,0)==MIXER_BUS_OK);
    assert(mixer_buses_finish(b,NULL,0)==MIXER_BUS_OK);
    compare(value(b,0,0,0),.5f);compare(value(b,1,0,0),.5f);compare(value(b,MIXER_METER_DIRECT,0,0),0);
    compare(value(b,MIXER_METER_MASTER,0,0),1);
    assert(project.track[0].mix.mute&&project.output[2]==0);mixer_buses_destroy(b);
    b=prepare();assert(mixer_buses_begin(b,2)==MIXER_BUS_OK);
    for(unsigned pad=0;pad<3;++pad)assert(mixer_buses_accumulate(b,pad,pcm,NULL,1,0)==MIXER_BUS_OK);
    assert(mixer_buses_finish(b,NULL,0)==MIXER_BUS_OK);compare(value(b,0,0,0),0);compare(value(b,MIXER_METER_MASTER,0,0),.5f);
    mixer_buses_destroy(b);
    puts("PASS Track gain/pan/Off/Mute/Solo, Master gain, Part Scene resolution and unchanged project/routing");
}
struct processor {uint32_t id;float memory[2];};
static unsigned creates,destroys,processes;
static void*create(void*user,uint32_t id,double rate,const float*params){(void)user;(void)params;assert(rate==48000);if(id==999)return NULL;struct processor*p=calloc(1,sizeof(*p));assert(p);p->id=id;++creates;return p;}
static void destroy(void*user,void*instance){(void)user;++destroys;free(instance);}
static void process(void*user,void*instance,float*pcm,size_t frames){(void)user;++processes;struct processor*p=instance;
    for(size_t i=0;i<frames;++i)for(unsigned ch=0;ch<2;++ch){float*v=pcm+i*2+ch;
        if(p->id==1)*v*=2;else if(p->id==2)*v+=.1f;
        else if(p->id==3){*v+=p->memory[ch]*.5f;p->memory[ch]=*v;}
        else if(p->id==4)*v=NAN;
    }
}
static void stage(struct mixer_channel*c,unsigned slot,uint32_t id){c->fx.count=slot+1;c->fx.stage[slot].kind=FX_STAGE_NATIVE;c->fx.stage[slot].native_id=id;}
static void effects(void)
{
    mixer_defaults(&project);project.output[0]=1;project.output[1]=2;
    stage(project.track,0,1);stage(project.track,1,2);stage(&project.master,0,1);
    struct fx_backend backend={NULL,create,destroy,process};enum mixer_bus_result error;
    struct mixer_buses*b=mixer_buses_prepare(&project,0,48000,8,&backend,&error);assert(b);float pcm[]={.25f};
    assert(mixer_buses_begin(b,1)==MIXER_BUS_OK&&mixer_buses_accumulate(b,0,pcm,NULL,1,0)==MIXER_BUS_OK);
    assert(mixer_buses_finish(b,NULL,0)==MIXER_BUS_OK);compare(value(b,0,0,0),.6f);compare(value(b,1,0,0),0);compare(value(b,MIXER_METER_MASTER,0,0),1.2f);
    mixer_buses_destroy(b);assert(creates==destroys);
    project.track[0].fx_bypass=1;b=mixer_buses_prepare(&project,0,48000,8,&backend,&error);assert(b);
    assert(mixer_buses_begin(b,1)==MIXER_BUS_OK&&mixer_buses_accumulate(b,0,pcm,NULL,1,0)==MIXER_BUS_OK);
    assert(mixer_buses_finish(b,NULL,0)==MIXER_BUS_OK);compare(value(b,0,0,0),.25f);compare(value(b,MIXER_METER_MASTER,0,0),.5f);
    mixer_buses_destroy(b);assert(creates==destroys);
    project.track[0].fx_bypass=0;project.track[0].fx.stage[1].native_id=999;
    assert(!mixer_buses_prepare(&project,0,48000,8,&backend,&error)&&error==MIXER_BUS_FX_ERROR);assert(creates==destroys);
    mixer_defaults(&project);project.output[0]=1;project.output[1]=2;stage(project.track,0,3);stage(project.track+1,0,3);
    b=mixer_buses_prepare(&project,0,48000,8,&backend,&error);assert(b);
    assert(mixer_buses_begin(b,1)==MIXER_BUS_OK&&mixer_buses_accumulate(b,0,pcm,NULL,1,0)==MIXER_BUS_OK&&mixer_buses_finish(b,NULL,0)==MIXER_BUS_OK);
    assert(mixer_buses_begin(b,1)==MIXER_BUS_OK&&mixer_buses_finish(b,NULL,0)==MIXER_BUS_OK);
    compare(value(b,0,0,0),.125f);compare(value(b,1,0,0),0);mixer_buses_destroy(b);assert(creates==destroys&&processes>0);
    puts("PASS per-Track FX order, independent state/tails, Master FX placement, bypass and partial preparation cleanup");
}
static void failures(void)
{
    mixer_defaults(&project);enum mixer_bus_result error;project.track[0].mix.send=.5f;
    assert(!mixer_buses_prepare(&project,0,48000,8,NULL,&error)&&error==MIXER_BUS_UNSUPPORTED);
    project.track[0].mix.send=0;struct mixer_buses*b=prepare();float bad[]={NAN},good[]={.25f};struct mixer_pcm_view out;
    assert(mixer_buses_accumulate(b,0,good,NULL,1,0)==MIXER_BUS_STATE);
    assert(mixer_buses_begin(b,1)==MIXER_BUS_OK&&mixer_buses_begin(b,1)==MIXER_BUS_STATE);
    assert(mixer_buses_accumulate(b,0,bad,NULL,1,0)==MIXER_BUS_AUDIO_ERROR);
    assert(mixer_buses_finish(b,NULL,0)==MIXER_BUS_AUDIO_ERROR&&!mixer_buses_view(b,9,&out));
    assert(mixer_buses_begin(b,1)==MIXER_BUS_OK&&mixer_buses_accumulate(b,0,NULL,NULL,2,1)==MIXER_BUS_OK&&mixer_buses_finish(b,NULL,0)==MIXER_BUS_OK);
    compare(value(b,9,0,0),0);
    assert(mixer_buses_begin(b,1)==MIXER_BUS_OK&&mixer_buses_accumulate(b,128,good,NULL,1,0)==MIXER_BUS_INVALID);
    assert(mixer_buses_finish(b,NULL,0)==MIXER_BUS_INVALID&&!mixer_buses_view(b,9,&out));
    assert(mixer_buses_begin(b,1)==MIXER_BUS_OK);float large[]={FLT_MAX};
    assert(mixer_buses_accumulate(b,0,large,NULL,1,0)==MIXER_BUS_OK);
    assert(mixer_buses_accumulate(b,0,large,NULL,1,0)==MIXER_BUS_AUDIO_ERROR&&mixer_buses_finish(b,NULL,0)==MIXER_BUS_AUDIO_ERROR);
    mixer_buses_destroy(b);
    mixer_defaults(&project);project.output[0]=1;stage(project.track,0,4);struct fx_backend backend={NULL,create,destroy,process};
    b=mixer_buses_prepare(&project,0,48000,8,&backend,&error);assert(b);
    assert(mixer_buses_begin(b,1)==MIXER_BUS_OK&&mixer_buses_finish(b,NULL,0)==MIXER_BUS_AUDIO_ERROR&&!mixer_buses_view(b,9,&out));
    mixer_buses_destroy(b);assert(creates==destroys);
    puts("PASS invalid PCM/overflow/FX fault blocks incomplete output; explicit clear silence and unsupported Send rejection");
}
int main(void){routing();controls();effects();failures();puts("PASS real PCM eight-bus core (native Pad adapter not installed)");return 0;}
