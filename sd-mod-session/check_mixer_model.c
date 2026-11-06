#include "src/mixer_model.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static struct mixer_project project,decoded,saved;
static char encoded[MIXER_PAYLOAD_BYTES],again[MIXER_PAYLOAD_BYTES],damaged[MIXER_PAYLOAD_BYTES];
static const unsigned fields=MIXER_SCENE_LEVEL|MIXER_SCENE_PAN|MIXER_SCENE_SEND|MIXER_SCENE_MUTE|MIXER_SCENE_SOLO;
static void defaults(void)
{
    mixer_defaults(&project);assert(mixer_project_valid(&project));
    for(unsigned i=0;i<MIXER_PADS;++i)assert(project.output[i]==0);
    for(unsigned i=0;i<MIXER_TRACKS;++i){assert(project.track[i].mix.level_db==0);assert(!project.track[i].mix.mute&&!project.track[i].mix.solo&&!project.track[i].fx.count);}
    memset(&decoded,0x5a,sizeof(decoded));assert(mixer_decode(NULL,0,&decoded)==MIXER_OK);
    assert(!memcmp(&project,&decoded,sizeof(project)));
    for(unsigned pad=0;pad<MIXER_PADS;++pad)assert(mixer_set_output(&project,pad,pad%9)==MIXER_OK);
    assert(project.output[16]==7&&project.output[127]==1);saved=project;
    assert(mixer_set_output(&project,128,1)==MIXER_INVALID);
    assert(mixer_set_output(&project,3,9)==MIXER_INVALID);
    assert(!memcmp(&project,&saved,sizeof(project)));
    project.master.mix.mute=1;assert(!mixer_project_valid(&project));project=saved;
    project.track[0].mix.level_db=NAN;assert(!mixer_project_valid(&project));project=saved;
    project.track[0].mix.level_db=-INFINITY;assert(mixer_project_valid(&project));
    project.track[0].mix.level_db=INFINITY;assert(!mixer_project_valid(&project));
    puts("PASS defaults/legacy extension absence, all 128 project-level routes and invalid-write isolation");
}
static void groups(void)
{
    mixer_defaults(&project);project.track[0].mix.level_db=-3;project.track[1].mix.level_db=-9;project.track[2].mix.level_db=-6;
    project.track[3].mix.level_db=-INFINITY;float delta=99;
    assert(mixer_shift_levels(&project,15,2,&delta)&&delta==2);
    assert(project.track[0].mix.level_db==-1&&project.track[1].mix.level_db==-7&&project.track[2].mix.level_db==-4);
    assert(isinf(project.track[3].mix.level_db)&&project.track[3].mix.level_db<0);
    assert(mixer_shift_levels(&project,7,20,&delta)&&delta==7);
    assert(project.track[0].mix.level_db==6&&project.track[1].mix.level_db==0&&project.track[2].mix.level_db==3);
    assert(mixer_shift_levels(&project,7,-100,&delta)&&delta==-60);
    assert(project.track[0].mix.level_db==-54&&project.track[1].mix.level_db==-60&&project.track[2].mix.level_db==-57);
    saved=project;assert(!mixer_shift_levels(&project,8,1,&delta));assert(!mixer_shift_levels(&project,256,1,&delta));
    assert(!mixer_shift_levels(&project,0,1,&delta));assert(!mixer_shift_levels(&project,1,NAN,&delta));
    assert(!memcmp(&saved,&project,sizeof(saved)));
    puts("PASS relative dB moves preserve balance at both limits; Off and Master excluded");
}
static void scenes(void)
{
    mixer_defaults(&project);project.track[0].mix.level_db=-3;
    assert(mixer_set_output(&project,127,8)==MIXER_OK);
    struct mixer_scene first={0},second={0};first.part_id=1001;second.part_id=1002;
    first.fields[0]=MIXER_SCENE_LEVEL|MIXER_SCENE_MUTE;first.track[0].level_db=-12;first.track[0].mute=1;
    second.fields[0]=MIXER_SCENE_PAN;second.track[0].pan=.5f;
    assert(mixer_scene_set(&project,&first)==MIXER_OK&&mixer_scene_set(&project,&second)==MIXER_OK);
    saved=project;struct mixer_mix mix[MIXER_TRACKS];
    assert(mixer_scene_resolve(&project,1001,mix)&&mix[0].level_db==-12&&mix[0].mute==1);
    assert(mixer_scene_resolve(&project,1002,mix)&&mix[0].level_db==-3&&!mix[0].mute&&mix[0].pan==.5f);
    assert(!mixer_scene_resolve(&project,999,mix)&&mix[0].level_db==-3&&!mix[0].mute&&mix[0].pan==0);
    assert(!memcmp(&project,&saved,sizeof(project))&&project.output[127]==8);
    first.track[0].level_db=-10;assert(mixer_scene_set(&project,&first)==MIXER_OK&&project.scene_count==2);
    assert(mixer_scene_remove(&project,1001)&&project.scene_count==1&&project.scene[0].part_id==1002);
    assert(!mixer_scene_remove(&project,1001));
    for(unsigned i=1;i<MIXER_MAX_SCENES;++i){second.part_id=2000+i;assert(mixer_scene_set(&project,&second)==MIXER_OK);}
    saved=project;second.part_id=99999;assert(mixer_scene_set(&project,&second)==MIXER_FULL);
    assert(!memcmp(&project,&saved,sizeof(project)));
    puts("PASS Part-specific Scenes, fresh project fallback, stable ids, full-table rejection and unchanged routing");
}
static void fixture(void)
{
    mixer_defaults(&project);
    memcpy(project.track[0].name,"\xd0\x91\xd0\xb0\xd1\x81",7); /* UTF-8 Bass */
    for(unsigned i=0;i<=MIXER_TRACKS;++i){struct mixer_channel*c=i<MIXER_TRACKS?project.track+i:&project.master;
        c->mix.level_db=i==2?-INFINITY:-(float)i;c->mix.pan=(float)i/8-1;c->mix.send=(float)i/8;
        c->fx_bypass=i&1;c->fx.count=2;
        struct fx_stage*n=c->fx.stage;n->kind=FX_STAGE_NATIVE;n->bypass=1;n->native_id=0x12345678+i;
        for(unsigned j=0;j<FX_CHAIN_PARAMETERS;++j)n->parameter[j]=(float)j/31;
        struct fx_stage*eq=n+1;eq->kind=FX_STAGE_EQ;eq->bypass=0;fx_eq_default(&eq->eq);
        eq->eq.count=FX_EQ_MAX_BANDS;eq->eq.output_db=1.25;
        for(unsigned j=0;j<FX_EQ_MAX_BANDS;++j)eq->eq.band[j]=(struct fx_eq_band){100.+j*350,1.5,.71,FX_EQ_BELL,1};
    }
    for(unsigned i=0;i<MIXER_PADS;++i)project.output[i]=(unsigned char)(i%9);
    for(unsigned i=0;i<MIXER_MAX_SCENES;++i){struct mixer_scene*s=project.scene+i;s->part_id=UINT64_C(0x100000000)+i;
        for(unsigned j=0;j<MIXER_TRACKS;++j){s->fields[j]=(unsigned char)fields;s->track[j]=(struct mixer_mix){-3.f-j,.125f*j,.1f*j,j&1,(j+1)&1};}
    }
    project.scene_count=MIXER_MAX_SCENES;assert(mixer_project_valid(&project));
}
static unsigned nibble(char c){return c<='9'?(unsigned)(c-'0'):(unsigned)(c-'a'+10);}
static unsigned payload_byte(const char*s,size_t i){return nibble(s[5+i*2])*16+nibble(s[6+i*2]);}
static void set_byte(char*s,size_t i,unsigned value){const char*h="0123456789abcdef";s[5+i*2]=h[value>>4];s[6+i*2]=h[value&15];}
static void repair_crc(char*s,size_t n)
{
    uint32_t crc=UINT32_MAX;size_t bytes=(n-5)/2;
    for(size_t i=0;i<bytes-4;++i){crc^=payload_byte(s,i);for(unsigned j=0;j<8;++j)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}
    crc=~crc;for(unsigned j=0;j<4;++j)set_byte(s,bytes-4+j,(crc>>(8*j))&255);
}
static void codec(void)
{
    fixture();size_t needed=0,n=0;
    assert(mixer_encode(&project,NULL,0,&needed)==MIXER_OK&&needed+1<MIXER_PAYLOAD_BYTES);
    memset(encoded,'!',sizeof(encoded));assert(mixer_encode(&project,encoded,needed,&n)==MIXER_SHORT_BUFFER&&n==needed);
    for(unsigned i=0;i<sizeof(encoded);++i)assert(encoded[i]=='!');
    assert(mixer_encode(&project,encoded,sizeof(encoded),&n)==MIXER_OK&&n==needed&&encoded[n]==0);
    assert(mixer_decode(encoded,n,&decoded)==MIXER_OK&&mixer_project_valid(&decoded));
    assert(decoded.output[127]==1&&decoded.scene_count==128&&decoded.scene[127].part_id==UINT64_C(0x10000007f));
    assert(!strcmp(project.track[0].name,decoded.track[0].name));
    assert(decoded.track[0].fx.stage[0].parameter[31]==1&&decoded.track[0].fx.stage[1].eq.band[7].hz==2550);
    assert(isinf(decoded.track[2].mix.level_db)&&decoded.track[2].mix.level_db<0);
    assert(mixer_encode(&decoded,again,sizeof(again),&needed)==MIXER_OK&&needed==n&&!memcmp(encoded,again,n+1));
    /* Fully occupied five-slot EQ8 chains plus 128 fully populated Scenes. */
    for(unsigned i=0;i<=MIXER_TRACKS;++i){struct mixer_channel*c=i<MIXER_TRACKS?project.track+i:&project.master;
        struct fx_stage eq=c->fx.stage[1];for(unsigned j=0;j<FX_CHAIN_MAX;++j)c->fx.stage[j]=eq;c->fx.count=FX_CHAIN_MAX;
        memset(c->name,'x',MIXER_NAME_BYTES-1);c->name[MIXER_NAME_BYTES-1]=0;
    }
    assert(mixer_encode(&project,again,sizeof(again),&needed)==MIXER_OK&&needed+1<=MIXER_PAYLOAD_BYTES);
    assert(mixer_decode(again,needed,&decoded)==MIXER_OK);
    printf("PASS exact-size transactional codec, UTF-8, IEEE Off, all 32 FX parameters, EQ8 and largest V1 payload (%zu bytes)\n",needed);
}
static void invalid_payloads(void)
{
    mixer_defaults(&project);size_t n=0;assert(mixer_encode(&project,encoded,sizeof(encoded),&n)==MIXER_OK);
    saved=project;
    for(size_t cut=0;cut<n;++cut){assert(mixer_decode(encoded,cut,&project)!=MIXER_OK);assert(!memcmp(&project,&saved,sizeof(project)));}
    memcpy(damaged,encoded,n+1);damaged[20]='z';assert(mixer_decode(damaged,n,&project)==MIXER_CORRUPT);
    memcpy(damaged,encoded,n+1);set_byte(damaged,0,2);repair_crc(damaged,n);
    assert(mixer_decode(damaged,n,&project)==MIXER_UNSUPPORTED&&!memcmp(&project,&saved,sizeof(project)));
    memcpy(damaged,encoded,n+1);set_byte(damaged,2,1);repair_crc(damaged,n);
    assert(mixer_decode(damaged,n,&project)==MIXER_UNSUPPORTED);
    memcpy(damaged,encoded,n+1);set_byte(damaged,8,32);repair_crc(damaged,n);
    assert(mixer_decode(damaged,n,&project)==MIXER_CORRUPT);
    memcpy(damaged,encoded,n+1);set_byte(damaged,9,0);repair_crc(damaged,n);
    assert(mixer_decode(damaged,n,&project)==MIXER_CORRUPT);
    memcpy(damaged,encoded,n+1);set_byte(damaged,9,0xc0);repair_crc(damaged,n);
    assert(mixer_decode(damaged,n,&project)==MIXER_CORRUPT);
    /* Mutate valid envelopes, repairing checksums, to exercise internal
     * lengths/counts/numeric validation rather than only the checksum gate. */
    uint32_t random=0x4d495845;
    for(unsigned iteration=0;iteration<400;++iteration){memcpy(damaged,encoded,n+1);project=saved;
        random=random*1664525u+1013904223u;size_t where=8+random%((n-5)/2-12);
        random=random*1664525u+1013904223u;set_byte(damaged,where,random>>24);repair_crc(damaged,n);
        enum mixer_result result=mixer_decode(damaged,n,&project);
        if(result==MIXER_OK)assert(mixer_project_valid(&project));else assert(!memcmp(&project,&saved,sizeof(project)));
    }
    puts("PASS every truncated default payload, future version/flags, hostile names and checksum-valid malformed payloads without partial commits");
}
int main(int argc,char**argv)
{
    if(argc==2&&!strcmp(argv[1],"--fixture")){fixture();size_t n;assert(mixer_encode(&project,encoded,sizeof(encoded),&n)==MIXER_OK);puts(encoded);return 0;}
    assert(argc==1);defaults();groups();scenes();codec();invalid_payloads();
    printf("PASS Mixer data model; project RAM %zu bytes; no native project/audio/UI integration\n",sizeof(project));
    return 0;
}
