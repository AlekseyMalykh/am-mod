#include "program_pgm.h"
#include <string.h>
static enum am_pgm_status error(struct am_pgm_error*e,enum am_pgm_status s,size_t o){
    if(e){e->status=s;e->offset=o;}return s;
}
static unsigned u16(const unsigned char*p){return (unsigned)p[0]|((unsigned)p[1]<<8);}
static int signed_byte(unsigned n){return n<128?(int)n:(int)n-256;}
static int name_char(unsigned c){
    return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||
        (c&&strchr(" !#$%&'()-@_{}",(int)c)!=NULL);
}
enum am_pgm_status am_pgm_read(const void*input,size_t size,struct am_pgm_program*out,struct am_pgm_error*e){
    const unsigned char*b=input;
    if(!input||!out)return error(e,AM_PGM_ARGUMENT,0);
    if(size!=AM_PGM_BYTES)return error(e,AM_PGM_SIZE,0);
    if(u16(b)!=AM_PGM_BYTES)return error(e,AM_PGM_SIZE,0);
    if(memcmp(b+4,"MPC1000 PGM 1.00",16))return error(e,AM_PGM_FORMAT,4);
    struct am_pgm_program p;memset(&p,0,sizeof(p));
#define RANGE(value,max,offset) do{if((value)>(max))return error(e,AM_PGM_RANGE,(offset));}while(0)
    for(unsigned pad=0;pad<64;++pad){
        size_t base=24+pad*164;struct am_pgm_pad*d=&p.pads[pad];int used=0;
        for(unsigned layer=0;layer<4;++layer){
            size_t off=base+layer*24;struct am_pgm_layer*l=&d->layers[layer];
            for(unsigned n=0;n<16&&b[off+n];++n){
                if(!name_char(b[off+n]))return error(e,AM_PGM_NAME,off+n);
                l->name[n]=(char)b[off+n];
            }
            if(!l->name[0])continue;
            used=1;l->level=b[off+17];l->velocity_low=b[off+18];l->velocity_high=b[off+19];
            unsigned tune=u16(b+off+20);l->tune_cents=tune<32768?(int)tune:(int)tune-65536;
            l->play_mode=b[off+22];
            RANGE(l->level,100,off+17);RANGE(l->velocity_high,127,off+19);
            RANGE(l->velocity_low,l->velocity_high,off+18);RANGE(l->play_mode,1,off+22);
            if(l->tune_cents < -3600||l->tune_cents>3600)return error(e,AM_PGM_RANGE,off+20);
        }
        if(!used)continue;
#define FIELD(field,offset,max) do{d->field=b[base+(offset)];RANGE(d->field,(max),base+(offset));}while(0)
        FIELD(overlap,0x62,1);FIELD(mute_group,0x63,32);
        FIELD(attack,0x66,100);FIELD(decay,0x67,100);FIELD(decay_mode,0x68,1);
        FIELD(velocity_level,0x6b,100);FIELD(filter1,0x71,3);FIELD(frequency1,0x72,100);
        FIELD(resonance1,0x73,100);FIELD(velocity_frequency1,0x78,100);
        FIELD(filter2,0x79,4);FIELD(frequency2,0x7a,100);FIELD(resonance2,0x7b,100);
        FIELD(level,0x8f,100);FIELD(pan,0x90,100);FIELD(output,0x91,2);
        FIELD(fx_send,0x92,2);FIELD(fx_level,0x93,100);FIELD(filter_attenuation,0x94,2);
#undef FIELD
    }
    for(unsigned n=0;n<64;++n){p.note_for_pad[n]=b[0x2918+n];RANGE(p.note_for_pad[n],127,0x2918+n);}
    for(unsigned n=0;n<128;++n){p.pad_for_note[n]=b[0x2958+n];RANGE(p.pad_for_note[n],64,0x2958+n);}
    p.program_change=b[0x29d8];RANGE(p.program_change,128,0x29d8);
    for(unsigned i=0;i<2;++i){
        size_t o=0x29d9+i*13;struct am_pgm_slider*s=&p.sliders[i];
        s->pad=b[o];s->parameter=b[o+2];s->tune_low=signed_byte(b[o+3]);s->tune_high=signed_byte(b[o+4]);
        s->filter_low=signed_byte(b[o+5]);s->filter_high=signed_byte(b[o+6]);
        s->layer_low=b[o+7];s->layer_high=b[o+8];s->attack_low=b[o+9];s->attack_high=b[o+10];
        s->decay_low=b[o+11];s->decay_high=b[o+12];
        /* Slider implementations vary. Preserve values; application is a
         * separate capability, never a reason to drop the pad assignments. */
    }
#undef RANGE
    memcpy(out,&p,sizeof(p));return error(e,AM_PGM_OK,0);
}
int am_pgm_plan(const struct am_pgm_program*p,unsigned first,const uint8_t occupied[128],uint8_t mapping[64]){
    if(!p||!occupied||!mapping||first>=128||first%16)return 0;
    uint8_t result[64];memset(result,255,sizeof(result));
    for(unsigned pad=0;pad<64;++pad){
        int used=0;for(unsigned l=0;l<4;++l)used|=p->pads[pad].layers[l].name[0]!=0;
        if(!used)continue;
        unsigned target=first+pad;if(target>=128||occupied[target])return 0;
        result[pad]=(uint8_t)target;
    }
    memcpy(mapping,result,sizeof(result));return 1;
}
