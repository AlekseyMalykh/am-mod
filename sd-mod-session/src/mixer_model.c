#include "mixer_model.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

_Static_assert(sizeof(float)==4&&sizeof(double)==8&&FLT_RADIX==2&&FLT_MANT_DIG==24&&DBL_MANT_DIG==53,
               "Mixer codec requires IEEE binary32/binary64");
#define ALL_FIELDS (MIXER_SCENE_LEVEL|MIXER_SCENE_PAN|MIXER_SCENE_SEND|MIXER_SCENE_MUTE|MIXER_SCENE_SOLO)
static const char prefix[]="AMMX:";
static int level_valid(float v){return (isinf(v)&&v<0)||(isfinite(v)&&v>=MIXER_LEVEL_MIN_DB&&v<=MIXER_LEVEL_MAX_DB);}
static int mix_valid(const struct mixer_mix*m){return level_valid(m->level_db)&&isfinite(m->pan)&&m->pan>=-1&&m->pan<=1&&isfinite(m->send)&&m->send>=0&&m->send<=1&&m->mute<=1&&m->solo<=1;}
static int name_valid(const char*s)
{
    unsigned n=0;
    while(n<MIXER_NAME_BYTES&&s[n])++n;
    if(!n||n==MIXER_NAME_BYTES)return 0;
    for(unsigned i=0;i<n;){
        unsigned a=(unsigned char)s[i++],more,min;uint32_t code;
        if(a<128){if(a<32||a==127)return 0;continue;}
        if(a>=0xc2&&a<=0xdf){more=1;min=0x80;code=a&31;}
        else if(a>=0xe0&&a<=0xef){more=2;min=0x800;code=a&15;}
        else if(a>=0xf0&&a<=0xf4){more=3;min=0x10000;code=a&7;}
        else return 0;
        if(i+more>n)return 0;
        while(more--){unsigned b=(unsigned char)s[i++];if((b&0xc0)!=0x80)return 0;code=code*64+(b&63);}
        if(code<min||code>0x10ffff||(code>=0xd800&&code<=0xdfff))return 0;
    }
    return 1;
}
static int scene_valid(const struct mixer_scene*s)
{
    if(!s||!s->part_id)return 0;
    for(unsigned i=0;i<MIXER_TRACKS;++i){unsigned f=s->fields[i];const struct mixer_mix*m=&s->track[i];
        if(f&~ALL_FIELDS)return 0;
        if((f&MIXER_SCENE_LEVEL)&&!level_valid(m->level_db))return 0;
        if((f&MIXER_SCENE_PAN)&&(!isfinite(m->pan)||m->pan< -1||m->pan>1))return 0;
        if((f&MIXER_SCENE_SEND)&&(!isfinite(m->send)||m->send<0||m->send>1))return 0;
        if((f&MIXER_SCENE_MUTE)&&m->mute>1)return 0;
        if((f&MIXER_SCENE_SOLO)&&m->solo>1)return 0;
    }
    return 1;
}
void mixer_defaults(struct mixer_project*p)
{
    if(!p)return;
    static const uint32_t colours[]={0x00c700,0x00c8ff,0x8830ff,0xffc247,0x2030ff,0xff6000,0xff20b0,0x98ff00};
    memset(p,0,sizeof(*p));
    for(unsigned i=0;i<MIXER_TRACKS;++i){snprintf(p->track[i].name,MIXER_NAME_BYTES,"Track %u",i+1);p->track[i].colour=colours[i];}
    memcpy(p->master.name,"Master",7);p->master.colour=0xffc247;
}
int mixer_project_valid(const struct mixer_project*p)
{
    if(!p||p->scene_count>MIXER_MAX_SCENES)return 0;
    for(unsigned i=0;i<=MIXER_TRACKS;++i){const struct mixer_channel*c=i<MIXER_TRACKS?p->track+i:&p->master;
        /* Persist sample-rate-independent settings. Actual engine rate must
         * be revalidated when preparing DSP; no native processor is created. */
        if(!name_valid(c->name)||c->colour>0xffffff||!mix_valid(&c->mix)||c->fx_bypass>1||!fx_chain_valid(&c->fx,192000))return 0;
    }
    if(p->master.mix.mute||p->master.mix.solo)return 0;
    for(unsigned i=0;i<MIXER_PADS;++i)if(p->output[i]>MIXER_TRACKS)return 0;
    for(unsigned i=0;i<p->scene_count;++i){
        if(!scene_valid(p->scene+i))return 0;
        for(unsigned j=0;j<i;++j)if(p->scene[j].part_id==p->scene[i].part_id)return 0;
    }
    return 1;
}
enum mixer_result mixer_set_output(struct mixer_project*p,unsigned pad,unsigned output)
{
    if(!p||pad>=MIXER_PADS||output>MIXER_TRACKS)return MIXER_INVALID;
    p->output[pad]=(unsigned char)output;return MIXER_OK;
}
enum mixer_result mixer_scene_set(struct mixer_project*p,const struct mixer_scene*s)
{
    if(!p||p->scene_count>MIXER_MAX_SCENES||!scene_valid(s))return MIXER_INVALID;
    for(unsigned i=0;i<p->scene_count;++i)if(p->scene[i].part_id==s->part_id){p->scene[i]=*s;return MIXER_OK;}
    if(p->scene_count==MIXER_MAX_SCENES)return MIXER_FULL;
    p->scene[p->scene_count++]=*s;return MIXER_OK;
}
int mixer_scene_remove(struct mixer_project*p,uint64_t id)
{
    if(!p||!id||p->scene_count>MIXER_MAX_SCENES)return 0;
    for(unsigned i=0;i<p->scene_count;++i)if(p->scene[i].part_id==id){
        memmove(p->scene+i,p->scene+i+1,(p->scene_count-i-1)*sizeof(*p->scene));
        memset(p->scene+--p->scene_count,0,sizeof(*p->scene));return 1;
    }
    return 0;
}
int mixer_scene_resolve(const struct mixer_project*p,uint64_t id,struct mixer_mix out[MIXER_TRACKS])
{
    if(!p||!out||p->scene_count>MIXER_MAX_SCENES)return 0;
    const struct mixer_scene*s=NULL;
    for(unsigned i=0;i<p->scene_count;++i)if(p->scene[i].part_id==id){s=p->scene+i;break;}
    if(s&&!scene_valid(s))return 0;
    for(unsigned i=0;i<MIXER_TRACKS;++i){
        struct mixer_mix m=p->track[i].mix;
        if(s){const struct mixer_mix*v=s->track+i;unsigned f=s->fields[i];
            if(f&MIXER_SCENE_LEVEL)m.level_db=v->level_db;
            if(f&MIXER_SCENE_PAN)m.pan=v->pan;
            if(f&MIXER_SCENE_SEND)m.send=v->send;
            if(f&MIXER_SCENE_MUTE)m.mute=v->mute;
            if(f&MIXER_SCENE_SOLO)m.solo=v->solo;
        }
        out[i]=m;
    }
    return s!=NULL;
}
int mixer_shift_levels(struct mixer_project*p,unsigned selected,float requested,float*applied)
{
    if(!p||!selected||selected>255||!isfinite(requested))return 0;
    float low=-INFINITY,high=INFINITY;unsigned finite=0;
    for(unsigned i=0;i<MIXER_TRACKS;++i)if(selected&(1u<<i)){
        float g=p->track[i].mix.level_db;if(!level_valid(g))return 0;
        if(isfinite(g)){low=fmaxf(low,MIXER_LEVEL_MIN_DB-g);high=fminf(high,MIXER_LEVEL_MAX_DB-g);++finite;}
    }
    if(!finite)return 0;
    float delta=fmaxf(low,fminf(high,requested));
    for(unsigned i=0;i<MIXER_TRACKS;++i)if((selected&(1u<<i))&&isfinite(p->track[i].mix.level_db))p->track[i].mix.level_db+=delta;
    if(applied)*applied=delta;
    return 1;
}

/* Byte-level codec. Only active fields are persisted; padding/native pointers
 * and unused FX/Scene storage never enter the payload. */
struct writer {char*out;size_t count;uint32_t crc;};
static uint32_t crc_byte(uint32_t crc,unsigned byte){crc^=byte;for(unsigned i=0;i<8;++i)crc=(crc>>1)^((crc&1)?0xedb88320u:0);return crc;}
static void byte(struct writer*w,unsigned v){static const char hex[]="0123456789abcdef";v&=255;if(w->out){w->out[5+w->count*2]=hex[v>>4];w->out[6+w->count*2]=hex[v&15];}++w->count;w->crc=crc_byte(w->crc,v);}
static void integer(struct writer*w,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i){byte(w,(unsigned)v);v>>=8;}}
static void number(struct writer*w,float v){uint32_t bits;memcpy(&bits,&v,4);integer(w,bits,4);}
static void real(struct writer*w,double v){uint64_t bits;memcpy(&bits,&v,8);integer(w,bits,8);}
static void emit_mix(struct writer*w,const struct mixer_mix*m,unsigned fields)
{
    if(fields&MIXER_SCENE_LEVEL)number(w,m->level_db);
    if(fields&MIXER_SCENE_PAN)number(w,m->pan);
    if(fields&MIXER_SCENE_SEND)number(w,m->send);
    if(fields&MIXER_SCENE_MUTE)byte(w,m->mute);
    if(fields&MIXER_SCENE_SOLO)byte(w,m->solo);
}
static void emit_channel(struct writer*w,const struct mixer_channel*c)
{
    unsigned n=(unsigned)strlen(c->name);byte(w,n);for(unsigned i=0;i<n;++i)byte(w,(unsigned char)c->name[i]);
    integer(w,c->colour,4);emit_mix(w,&c->mix,ALL_FIELDS);byte(w,c->fx_bypass);byte(w,c->fx.count);
    for(unsigned i=0;i<c->fx.count;++i){const struct fx_stage*s=c->fx.stage+i;byte(w,s->kind);byte(w,s->bypass);
        if(s->kind==FX_STAGE_NATIVE){integer(w,s->native_id,4);for(unsigned j=0;j<FX_CHAIN_PARAMETERS;++j)number(w,s->parameter[j]);}
        else {byte(w,s->eq.count);byte(w,s->eq.bypass);real(w,s->eq.output_db);
            for(unsigned j=0;j<s->eq.count;++j){const struct fx_eq_band*b=s->eq.band+j;real(w,b->hz);real(w,b->gain_db);real(w,b->q);byte(w,b->type);byte(w,b->enabled);}
        }
    }
}
static void emit_project(struct writer*w,const struct mixer_project*p)
{
    for(unsigned i=0;i<MIXER_TRACKS;++i)emit_channel(w,p->track+i);
    emit_channel(w,&p->master);
    for(unsigned i=0;i<MIXER_PADS;++i)byte(w,p->output[i]);
    integer(w,p->scene_count,2);
    for(unsigned i=0;i<p->scene_count;++i){const struct mixer_scene*s=p->scene+i;integer(w,s->part_id,8);
        for(unsigned j=0;j<MIXER_TRACKS;++j){byte(w,s->fields[j]);emit_mix(w,s->track+j,s->fields[j]);}
    }
}
enum mixer_result mixer_encode(const struct mixer_project*p,char*out,size_t capacity,size_t*needed)
{
    if(!mixer_project_valid(p))return MIXER_INVALID;
    struct writer count={0};emit_project(&count,p);
    size_t n=5+2*(8+count.count+4);
    if(n+1>MIXER_PAYLOAD_BYTES)return MIXER_FULL;
    if(needed)*needed=n;
    if(!out)return capacity?MIXER_INVALID:MIXER_OK;
    if(capacity<n+1)return MIXER_SHORT_BUFFER;
    memcpy(out,prefix,5);struct writer w={out,0,UINT32_MAX};
    integer(&w,MIXER_MODEL_VERSION,2);integer(&w,0,2);integer(&w,count.count,4);
    emit_project(&w,p);uint32_t checksum=~w.crc;integer(&w,checksum,4);
    out[n]=0;return MIXER_OK;
}
struct reader {const char*in;size_t count,position;int failed;};
static int hex(unsigned char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static unsigned read_byte(struct reader*r)
{
    if(r->position>=r->count){r->failed=1;return 0;}
    int hi=hex((unsigned char)r->in[r->position*2]),lo=hex((unsigned char)r->in[r->position*2+1]);++r->position;
    if(hi<0||lo<0){r->failed=1;return 0;}return (unsigned)(hi*16+lo);
}
static uint64_t read_integer(struct reader*r,unsigned n){uint64_t v=0;for(unsigned i=0;i<n;++i)v|=(uint64_t)read_byte(r)<<(i*8);return v;}
static float read_number(struct reader*r){uint32_t bits=(uint32_t)read_integer(r,4);float v;memcpy(&v,&bits,4);return v;}
static double read_real(struct reader*r){uint64_t bits=read_integer(r,8);double v;memcpy(&v,&bits,8);return v;}
static void read_mix(struct reader*r,struct mixer_mix*m,unsigned fields)
{
    if(fields&MIXER_SCENE_LEVEL)m->level_db=read_number(r);
    if(fields&MIXER_SCENE_PAN)m->pan=read_number(r);
    if(fields&MIXER_SCENE_SEND)m->send=read_number(r);
    if(fields&MIXER_SCENE_MUTE)m->mute=read_byte(r);
    if(fields&MIXER_SCENE_SOLO)m->solo=read_byte(r);
}
static int read_channel(struct reader*r,struct mixer_channel*c)
{
    unsigned n=read_byte(r);if(!n||n>=MIXER_NAME_BYTES)return 0;
    for(unsigned i=0;i<n;++i){c->name[i]=(char)read_byte(r);if(!c->name[i])return 0;}
    c->name[n]=0;c->colour=(uint32_t)read_integer(r,4);read_mix(r,&c->mix,ALL_FIELDS);
    c->fx_bypass=read_byte(r);c->fx.count=read_byte(r);if(c->fx.count>FX_CHAIN_MAX)return 0;
    for(unsigned i=0;i<c->fx.count;++i){struct fx_stage*s=c->fx.stage+i;s->kind=read_byte(r);s->bypass=read_byte(r);
        if(s->kind==FX_STAGE_NATIVE){s->native_id=(uint32_t)read_integer(r,4);for(unsigned j=0;j<FX_CHAIN_PARAMETERS;++j)s->parameter[j]=read_number(r);}
        else if(s->kind==FX_STAGE_EQ){s->eq.count=read_byte(r);s->eq.bypass=read_byte(r);s->eq.output_db=read_real(r);if(s->eq.count>FX_EQ_MAX_BANDS)return 0;
            for(unsigned j=0;j<s->eq.count;++j){struct fx_eq_band*b=s->eq.band+j;b->hz=read_real(r);b->gain_db=read_real(r);b->q=read_real(r);b->type=read_byte(r);b->enabled=read_byte(r);}
        }else return 0;
    }
    return !r->failed;
}
enum mixer_result mixer_decode(const char*in,size_t size,struct mixer_project*out)
{
    if(!out)return MIXER_INVALID;
    if(!in&&!size){mixer_defaults(out);return MIXER_OK;}
    if(!in||size<29||size>=MIXER_PAYLOAD_BYTES||memcmp(in,prefix,5)||(size-5)%2)return MIXER_CORRUPT;
    struct reader r={in+5,(size-5)/2,0,0};uint32_t crc=UINT32_MAX;
    for(size_t i=0;i<r.count-4;++i)crc=crc_byte(crc,read_byte(&r));
    uint32_t saved=(uint32_t)read_integer(&r,4);if(r.failed||~crc!=saved)return MIXER_CORRUPT;
    r.position=0;unsigned version=(unsigned)read_integer(&r,2),flags=(unsigned)read_integer(&r,2);
    size_t bytes=(size_t)read_integer(&r,4);
    if(bytes!=r.count-12)return MIXER_CORRUPT;
    if(version!=MIXER_MODEL_VERSION||flags)return MIXER_UNSUPPORTED;
    r.count-=4;struct mixer_project pending;memset(&pending,0,sizeof(pending));
    for(unsigned i=0;i<MIXER_TRACKS;++i)if(!read_channel(&r,pending.track+i))return MIXER_CORRUPT;
    if(!read_channel(&r,&pending.master))return MIXER_CORRUPT;
    for(unsigned i=0;i<MIXER_PADS;++i)pending.output[i]=(unsigned char)read_byte(&r);
    pending.scene_count=(unsigned)read_integer(&r,2);if(pending.scene_count>MIXER_MAX_SCENES)return MIXER_CORRUPT;
    for(unsigned i=0;i<pending.scene_count;++i){struct mixer_scene*s=pending.scene+i;s->part_id=read_integer(&r,8);
        for(unsigned j=0;j<MIXER_TRACKS;++j){s->fields[j]=(unsigned char)read_byte(&r);if(s->fields[j]&~ALL_FIELDS)return MIXER_UNSUPPORTED;read_mix(&r,s->track+j,s->fields[j]);}
    }
    if(r.failed||r.position!=r.count||!mixer_project_valid(&pending))return MIXER_CORRUPT;
    *out=pending;return MIXER_OK;
}
