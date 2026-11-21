/* Development/preflight CLI over the same allocation-free core intended for
 * firmware. stdin is one PGM, stdout one JSON. Never opens a Sample project. */
#include "src/program_pgm.h"
#include <stdio.h>
static void quote(const char*s){
    putchar('"');for(;*s;++s){if(*s=='"'||*s=='\\')putchar('\\');putchar(*s);}putchar('"');
}
int main(void){
    unsigned char bytes[AM_PGM_BYTES+1];size_t n=fread(bytes,1,sizeof(bytes),stdin);
    struct am_pgm_program p;struct am_pgm_error e;
    if(ferror(stdin)){fputs("{\"error\":\"read\"}\n",stdout);return 2;}
    if(am_pgm_read(bytes,n,&p,&e)!=AM_PGM_OK){
        printf("{\"error\":\"pgm\",\"status\":%u,\"offset\":%zu}\n",(unsigned)e.status,e.offset);return 2;
    }
    printf("{\"format\":\"MPC1000 PGM 1.00\",\"program_change\":%u,\"pads\":[",p.program_change);
    unsigned count=0;
    for(unsigned i=0;i<64;++i){
        const struct am_pgm_pad*d=&p.pads[i];unsigned layers=0;
        for(unsigned s=0;s<4;++s)layers+=d->layers[s].name[0]!=0;
        if(!layers)continue;
        printf("%s{\"index\":%u,\"pad\":\"%c%02u\",\"midi_note\":%u,\"layers\":[",count++?",":"",i,'A'+i/16,i%16+1,p.note_for_pad[i]);
        unsigned written=0;
        for(unsigned s=0;s<4;++s){
            const struct am_pgm_layer*l=&d->layers[s];if(!l->name[0])continue;
            printf("%s{\"index\":%u,\"name\":",written++?",":"",s);quote(l->name);
            printf(",\"level\":%u,\"velocity_low\":%u,\"velocity_high\":%u,\"tune_cents\":%d,\"play_mode\":%u}",l->level,l->velocity_low,l->velocity_high,l->tune_cents,l->play_mode);
        }
        printf("]");
#define EMIT(field) printf(",\"" #field "\":%u",d->field)
        EMIT(overlap);EMIT(mute_group);EMIT(attack);EMIT(decay);EMIT(decay_mode);EMIT(velocity_level);
        EMIT(filter1);EMIT(frequency1);EMIT(resonance1);EMIT(velocity_frequency1);
        EMIT(filter2);EMIT(frequency2);EMIT(resonance2);EMIT(level);EMIT(pan);
        EMIT(output);EMIT(fx_send);EMIT(fx_level);EMIT(filter_attenuation);
#undef EMIT
        putchar('}');
    }
    printf("],\"note_for_pad\":[");
    for(unsigned i=0;i<64;++i)printf("%s%u",i?",":"",p.note_for_pad[i]);
    printf("],\"pad_for_note\":[");
    for(unsigned i=0;i<128;++i)printf("%s%u",i?",":"",p.pad_for_note[i]);
    printf("],\"sliders\":[");
    for(unsigned i=0;i<2;++i){
        const struct am_pgm_slider*s=&p.sliders[i];
        printf("%s{\"pad\":%u,\"parameter\":%u,\"tune_low\":%d,\"tune_high\":%d,\"filter_low\":%d,\"filter_high\":%d,\"layer_low\":%u,\"layer_high\":%u,\"attack_low\":%u,\"attack_high\":%u,\"decay_low\":%u,\"decay_high\":%u}",i?",":"",s->pad,s->parameter,s->tune_low,s->tune_high,s->filter_low,s->filter_high,s->layer_low,s->layer_high,s->attack_low,s->attack_high,s->decay_low,s->decay_high);
    }
    puts("]}");return 0;
}
