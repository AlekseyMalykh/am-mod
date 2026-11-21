#define _POSIX_C_SOURCE 200809L
#include "midi_tools.h"
#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
extern void am_synth_midi(const unsigned char*,unsigned) __attribute__((weak));
extern void am_synth_key(const unsigned char*,unsigned) __attribute__((weak));
_Static_assert(sizeof(struct am_midi_map)==8,"Explicit map wire fields");
static _Atomic uint64_t maps[128];
static _Atomic int native_notes[128];
static _Atomic unsigned notes_ready;
static pthread_mutex_t log_lock=PTHREAD_MUTEX_INITIALIZER;
static struct am_midi_record records[AM_MIDI_LOG];
static unsigned log_used,log_head;
static uint64_t log_serial,clear_serial;
static _Atomic uint64_t revision,dropped;
static _Atomic unsigned paused;
static _Atomic uint64_t bslots[AM_MIDI_BUTTON_SLOTS]; /* port | channel<<8 | kind<<16 | number<<24 | code<<32 */
static _Atomic uint64_t learn_first;static _Atomic unsigned learn_rel,modheld; /* combo learn: first (modifier) source, its release, held mask per slot */
static _Atomic unsigned learn_btn; /* native button code+1 while Learn waits for its MIDI source */
static _Atomic uint32_t bq[32];static _Atomic unsigned bq_w,bq_r;
static _Atomic unsigned learn_pad; /* pad+1; callback publishes a message, UI commits */
static _Atomic uint64_t learn_message;
static uint64_t diagnostic_serial;
static unsigned diagnostic_lines;
static uint64_t milliseconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;}
const char*am_midi_port_name(unsigned p){static const char*names[]={"Other","USB","External","Surface"};return p<4?names[p]:"Other";}
static unsigned needed(unsigned status){if(status<0x80)return 0;if(status<0xf0)return (status&0xe0)==0xc0?1:2;return status==0xf1||status==0xf3?1:status==0xf2?2:0;}
void am_midi_parse(struct am_midi_parser*p,unsigned byte,am_midi_emit emit,void*user){
    if(!p||!emit||byte>255)return;
    struct am_midi_message m={0,{0,0,0}};
    if(byte>=0xf8){m.size=1;m.data[0]=(uint8_t)byte;emit(user,&m);return;}
    if(byte&0x80){
        p->used=0;
        if(byte==0xf0){p->status=0;p->sysex=1;p->sysex_bytes=1;return;}
        if(p->sysex&&byte==0xf7){p->sysex=0;m.size=1;m.data[0]=0xf0;emit(user,&m);return;}
        p->sysex=0;p->status=(uint8_t)byte;p->needed=(uint8_t)needed(byte);
        if(!p->needed){m.size=1;m.data[0]=(uint8_t)byte;emit(user,&m);p->status=0;}
        return;
    }
    if(p->sysex){if(p->sysex_bytes!=UINT32_MAX)++p->sysex_bytes;return;}
    if(!p->status||!p->needed)return;
    p->data[p->used++]=(uint8_t)byte;
    if(p->used!=p->needed)return;
    m.size=(uint8_t)(p->needed+1);m.data[0]=p->status;memcpy(m.data+1,p->data,p->needed);
    p->used=0;if(p->status>=0xf0)p->status=0;emit(user,&m);
}
static _Atomic uint32_t mode_word=60u<<16; /* mode | channel<<8 | root<<16 */
static _Atomic uint8_t mode_pad[16],mode_kind[16];
static _Atomic int selected_pad=-1;
static _Atomic uint8_t voice_count[128];
#define TUNE_SLOTS 8u
static _Atomic uint64_t tune_ring[128][TUNE_SLOTS]; /* bit0 valid, bits8..15 tune+128, bits16.. ms */
static uint32_t tune_ms(void){return (uint32_t)milliseconds();}
void am_midi_tune_push(unsigned pad,int tune,int chop){
 if(pad>=128||tune<-120||tune>120)return;
 uint32_t now=tune_ms();uint64_t value=((uint64_t)now<<16)|((uint64_t)(unsigned)(tune+128)<<8)|(chop?3u:1u);
 for(unsigned i=0;i<TUNE_SLOTS;++i){
  uint64_t old=atomic_load(&tune_ring[pad][i]);
  if(old&&(uint32_t)(now-(uint32_t)(old>>16))<=150u)continue;
  if(atomic_compare_exchange_strong(&tune_ring[pad][i],&old,value))return;
 }
}
int am_midi_tune_take(unsigned pad,int *tune,int *chop){
 if(pad>=128||!tune||!chop)return 0;
 uint32_t now=tune_ms();
 for(unsigned round=0;round<TUNE_SLOTS;++round){
  int best=-1;uint32_t age=0;uint64_t seen=0;
  for(unsigned i=0;i<TUNE_SLOTS;++i){
   uint64_t v=atomic_load(&tune_ring[pad][i]);if(!v)continue;
   uint32_t a=now-(uint32_t)(v>>16);
   if(a>150u){atomic_compare_exchange_strong(&tune_ring[pad][i],&v,0);continue;}
   if(best<0||a>age){best=(int)i;age=a;seen=v;}
  }
  if(best<0)return 0;
  if(atomic_compare_exchange_strong(&tune_ring[pad][best],&seen,0)){*tune=(int)((seen>>8)&255)-128;*chop=(seen&2)!=0;return 1;}
 }
 return 0;
}
void am_midi_selected_pad(int pad){atomic_store(&selected_pad,pad>=0&&pad<128?pad:-1);}
int am_midi_mode_get(struct am_midi_mode*m){
 if(!m)return 0;
 uint32_t w=atomic_load(&mode_word);
 m->mode=(uint8_t)w;m->channel=(uint8_t)(w>>8);m->root=(uint8_t)(w>>16);
 for(unsigned i=0;i<16;++i){m->pad[i]=atomic_load(mode_pad+i);m->kind[i]=atomic_load(mode_kind+i);}
 return 1;
}
int am_midi_mode_active(void){for(unsigned i=0;i<16;++i)if(atomic_load(mode_pad+i))return 1;return atomic_load(&mode_word)&255;}
static int mode_valid(const struct am_midi_mode*m){
 if(!m||m->mode>AM_MIDI_MODE_INSTRUMENT||m->channel>16||m->root>127)return 0;
 for(unsigned i=0;i<16;++i)if(m->pad[i]>128||m->kind[i]>AM_MIDI_KEY_SYNTH)return 0;
 return 1;
}
int am_midi_mode_set(const struct am_midi_mode*m){
 if(!mode_valid(m))return 0;
 atomic_store(&mode_word,(uint32_t)m->mode|((uint32_t)m->channel<<8)|((uint32_t)m->root<<16));
 for(unsigned i=0;i<16;++i){atomic_store(mode_pad+i,m->pad[i]);atomic_store(mode_kind+i,m->kind[i]);}
 atomic_fetch_add(&revision,1);return 1;
}
static int pitched_pad(unsigned ch,int *pad,int *kind){
 uint32_t w=atomic_load(&mode_word);unsigned mode=w&255,mch=(w>>8)&255;
 if(mode==AM_MIDI_MODE_SELECTED){int s=atomic_load(&selected_pad);if(mch&&mch!=ch+1)return 0;*pad=s;*kind=AM_MIDI_KEY_TUNE;return 1;} /* s<0: claimed, nothing to play */
 if(mode==AM_MIDI_MODE_CHANNELS){unsigned p=atomic_load(mode_pad+ch);if(!p)return 0;*pad=(int)p-1;*kind=atomic_load(mode_kind+ch);return 1;}
 return 0;
}
static _Atomic uint64_t level_maps[128];
static _Atomic unsigned char level_enabled[128];
static int valid(const struct am_midi_map*m){return m&&m->in_kind<=2&&m->out_kind<=4&&m->in_port<=2&&m->out_port<=2&&m->in_channel<=16&&m->out_channel<=16&&m->in_number<128&&m->out_number<128;}
static uint64_t pack(const struct am_midi_map*m){uint64_t n=0;const uint8_t*b=(const uint8_t*)m;for(unsigned i=0;i<8;++i)n|=(uint64_t)b[i]<<(i*8);return n;}
static void unpack(uint64_t n,struct am_midi_map*m){uint8_t*b=(uint8_t*)m;for(unsigned i=0;i<8;++i)b[i]=(uint8_t)(n>>(i*8));}
int am_midi_map_get(unsigned pad,struct am_midi_map*out){if(pad>=128||!out)return 0;unpack(atomic_load_explicit(maps+pad,memory_order_acquire),out);return 1;}
static int overlaps(const struct am_midi_map*a,const struct am_midi_map*b){return a->in_kind&&a->in_kind==b->in_kind&&a->in_number==b->in_number&&(!a->in_port||!b->in_port||a->in_port==b->in_port)&&(!a->in_channel||!b->in_channel||a->in_channel==b->in_channel);}
int am_midi_map_set(unsigned pad,const struct am_midi_map*m){
    if(pad>=128||!valid(m))return 0;
    for(unsigned i=0;i<128;++i)if(i!=pad){struct am_midi_map b;am_midi_map_get(i,&b);if(overlaps(m,&b))return 0;}
    atomic_store_explicit(maps+pad,pack(m),memory_order_release);atomic_fetch_add(&revision,1);return 1;
}
int am_midi_layer_get(unsigned pad,struct am_midi_map*out,unsigned*enabled){
 if(pad>=128||!out)return 0;
 unpack(atomic_load_explicit(level_maps+pad,memory_order_acquire),out);
 if(enabled)*enabled=atomic_load_explicit(level_enabled+pad,memory_order_acquire);
 return 1;
}
int am_midi_layer_set(unsigned pad,const struct am_midi_map*m,unsigned enabled){
 if(pad>=128||enabled>1||!valid(m)||m->in_kind||m->in_port||m->in_channel||m->in_number)return 0;
 atomic_store_explicit(level_maps+pad,pack(m),memory_order_release);
 atomic_store_explicit(level_enabled+pad,enabled,memory_order_release);atomic_fetch_add(&revision,1);return 1;
}
int am_midi_layer_resolve(unsigned pad,unsigned layer,struct am_midi_map*out){
 if(pad>=128||layer>AM_MIDI_LAYER_LEVELS||!out)return 0;
 if(layer==AM_MIDI_LAYER_LEVELS&&atomic_load_explicit(level_enabled+pad,memory_order_acquire))return am_midi_layer_get(pad,out,NULL);
 return am_midi_map_get(pad,out);
}
void am_midi_reset(void){for(unsigned i=0;i<AM_MIDI_BUTTON_SLOTS;++i)atomic_store(bslots+i,0);atomic_store(&learn_btn,0);atomic_store(&mode_word,60u<<16);for(unsigned i=0;i<16;++i){atomic_store(mode_pad+i,0);atomic_store(mode_kind+i,0);}for(unsigned i=0;i<128;++i){atomic_store_explicit(maps+i,0,memory_order_release);atomic_store(level_maps+i,0);atomic_store(level_enabled+i,0);}am_midi_learn(0,0);}
void am_midi_native_notes(const int n[128]){atomic_store(&notes_ready,0);for(unsigned i=0;i<128;++i)atomic_store(native_notes+i,n?n[i]:-1);atomic_store_explicit(&notes_ready,n!=NULL,memory_order_release);}
int am_midi_button_get(unsigned slot,struct am_midi_button*out){
    if(slot>=AM_MIDI_BUTTON_SLOTS||!out)return 0;
    uint64_t v=atomic_load_explicit(bslots+slot,memory_order_acquire);
    out->port=(uint8_t)v;out->channel=(uint8_t)(v>>8);out->kind=(uint8_t)(v>>16);out->number=(uint8_t)(v>>24);out->code=(uint8_t)(v>>32);out->mport=(uint8_t)((v>>40)&3);out->mkind=(uint8_t)((v>>42)&3);out->mchannel=(uint8_t)((v>>44)&31);out->mnumber=(uint8_t)((v>>49)&127);return 1;
}
static uint64_t bpack(const struct am_midi_button*b){return (uint64_t)b->port|(uint64_t)b->channel<<8|(uint64_t)b->kind<<16|(uint64_t)b->number<<24|(uint64_t)b->code<<32|(uint64_t)(b->mport&3)<<40|(uint64_t)(b->mkind&3)<<42|(uint64_t)(b->mchannel&31)<<44|(uint64_t)(b->mnumber&127)<<49;}
static int bvalid(const struct am_midi_button*b){return b->kind<=2&&b->port<=2&&b->channel<=16&&b->number<128&&b->mkind<=2&&b->mport<=2&&b->mchannel<=16&&b->mnumber<128&&(b->kind||!(b->port|b->channel|b->number|b->code|b->mkind));}
void am_midi_button_learn(unsigned code,int on){atomic_store(&learn_first,0);atomic_store(&learn_rel,0);atomic_store(&learn_message,0);atomic_store(&learn_pad,0);atomic_store(&learn_btn,on&&code<256?code+1:0);}
unsigned am_midi_button_learning(void){return atomic_load(&learn_btn);}
void am_midi_button_clear(void){for(unsigned i=0;i<AM_MIDI_BUTTON_SLOTS;++i)atomic_store(bslots+i,0);atomic_store(&learn_btn,0);atomic_fetch_add(&revision,1);}
int am_midi_button_take(unsigned*code,unsigned*down){
    unsigned r=atomic_load(&bq_r);
    if(r==atomic_load(&bq_w))return 0;
    uint32_t v=atomic_load(bq+(r&31));atomic_store(&bq_r,r+1);if(code)*code=(((v>>8)&0x1ff)-1)|(((v>>24)&1)?0x200u:0u);if(down)*down=v&255;return 1;
}
static void button_commit(uint64_t n,uint64_t mod){
    unsigned code=atomic_load(&learn_btn)-1;struct am_midi_button b={(uint8_t)(n>>32),(uint8_t)(((n>>16)&15)+1),(uint8_t)(((n>>20)&15)==9?1:2),(uint8_t)(n>>8),(uint8_t)code,0,0,0,0};
    if(mod){b.mport=(uint8_t)(mod>>32);b.mchannel=(uint8_t)(((mod>>16)&15)+1);b.mkind=(uint8_t)(((mod>>20)&15)==9?1:2);b.mnumber=(uint8_t)(mod>>8);}
    int slot=-1;
    for(unsigned i=0;i<AM_MIDI_BUTTON_SLOTS;++i){struct am_midi_button o;am_midi_button_get(i,&o);
        if(o.kind&&(o.code==code||(o.kind==b.kind&&o.number==b.number&&o.channel==b.channel&&o.port==b.port&&o.mkind==b.mkind&&o.mnumber==b.mnumber&&o.mchannel==b.mchannel&&o.mport==b.mport))){if(slot<0)slot=(int)i;else atomic_store(bslots+i,0);}}
    if(slot<0)for(unsigned i=0;i<AM_MIDI_BUTTON_SLOTS;++i){struct am_midi_button o;am_midi_button_get(i,&o);if(!o.kind){slot=(int)i;break;}}
    if(slot<0||!bvalid(&b))return;
    atomic_store_explicit(bslots+slot,bpack(&b),memory_order_release);atomic_store(&learn_btn,0);atomic_store(&learn_first,0);atomic_store(&learn_rel,0);atomic_fetch_add(&revision,1);
}
static int native_note(unsigned pad){return atomic_load_explicit(&notes_ready,memory_order_acquire)?atomic_load(native_notes+pad):-1;}
static int scope(unsigned port,unsigned channel,unsigned wanted_port,unsigned wanted_channel){return (!wanted_port||port==wanted_port)&&(!wanted_channel||channel+1==wanted_channel);}
static int translate_impl(struct am_midi_router*r,unsigned direction,unsigned port,const struct am_midi_message*in,struct am_midi_message*out){
    if(!r||!in||!out||!in->size||in->size>3)return 0;
    *out=*in;
    if(port!=AM_MIDI_USB&&port!=AM_MIDI_EXTERNAL)return 1;
    unsigned status=in->data[0],kind=status&0xf0,ch=status&15,num=in->data[1];
    if(status>=0xf0||num>=128||in->size!=3)return 1;
    int on=kind==0x90&&in->data[2],off=kind==0x80||(kind==0x90&&!in->data[2]);
    int cc_source=direction==AM_MIDI_IN&&kind==0xb0;
    if(direction==AM_MIDI_IN&&(kind==0x90||kind==0x80||kind==0xe0||(kind==0xb0&&num==1))&&am_synth_key){
        uint32_t w=atomic_load(&mode_word);unsigned mch=(w>>8)&255;
        if((w&255)==AM_MIDI_MODE_INSTRUMENT&&(!mch||mch==ch+1)){am_synth_key(in->data,in->size);return 0;} /* Instrument: keys play the synth only */
        if((w&255)==AM_MIDI_MODE_CHANNELS&&atomic_load(mode_pad+ch)&&atomic_load(mode_kind+ch)==AM_MIDI_KEY_SYNTH){am_synth_key(in->data,in->size);return 0;} /* channel role: Synth */
    }
    if(direction==AM_MIDI_IN&&(kind==0x90||kind==0x80||kind==0xb0)){
        unsigned mvalue=kind==0x80?0:in->data[2];
        for(unsigned i=0;i<AM_MIDI_BUTTON_SLOTS;++i){ /* modifier source: track held state, swallow */
            uint64_t v=atomic_load_explicit(bslots+i,memory_order_acquire);unsigned mk=(unsigned)((v>>42)&3);if(!(((v>>16)&255)&&mk))continue;
            unsigned mp=(unsigned)((v>>40)&3),mc=(unsigned)((v>>44)&31),mn=(unsigned)((v>>49)&127);
            if((mp&&mp!=port)||(mc&&mc!=ch+1)||mn!=num||mk!=(kind==0xb0?2u:1u))continue;
            if(mvalue)atomic_fetch_or(&modheld,1u<<i);else atomic_fetch_and(&modheld,~(1u<<i));
            return 0;
        }
        for(unsigned pass=0;pass<2;++pass)for(unsigned i=0;i<AM_MIDI_BUTTON_SLOTS;++i){
            uint64_t v=atomic_load_explicit(bslots+i,memory_order_acquire);if(!((v>>16)&255))continue;
            unsigned combo=(unsigned)((v>>42)&3)!=0;if(combo!=(pass==0))continue;
            
            unsigned sp=(unsigned)(v&255),sc=(unsigned)((v>>8)&255),sk=(unsigned)((v>>16)&255),sn=(unsigned)((v>>24)&255),code=(unsigned)((v>>32)&255);
            if((sp&&sp!=port)||(sc&&sc!=ch+1)||sn!=num||sk!=(kind==0xb0?2u:1u))continue;
            unsigned value=kind==0x80?0:in->data[2];
            if(code&0x80){ /* knob / encoder source: value travels to the native controller */
                if(kind==0x80)return 0;
                if((code&0x7f)==0x64){int d=(int)value-64;value=d<0?(unsigned)(128+d):(unsigned)d;}
            }else value=value!=0;
            unsigned w=atomic_load(&bq_w);if(w-atomic_load(&bq_r)<32){atomic_store(bq+(w&31),((code+1)<<8)|value|(combo?1u<<24:0));atomic_store(&bq_w,w+1);}
            return 0;
        }
    }
    struct am_midi_held*h=&r->held[cc_source?1:0][ch][num];
    if(kind==0xa0&&h->active){
        if(h->kind!=AM_MIDI_NOTE)return 0;
        out->data[0]=(uint8_t)(0xa0|(h->status&15));out->data[1]=h->number;return 1;
    }
    if((off||(cc_source&&!in->data[2]))&&h->active){
        if(h->active>1){--h->active;return 0;}
        h->active=0;
        if(h->kind==AM_MIDI_PITCHED){
            unsigned n=h->number&127u,c=atomic_load(voice_count+n);
            if(c>1){atomic_store(voice_count+n,(uint8_t)(c-1));return 0;}
            atomic_store(voice_count+n,0);
        }
        if(h->kind==AM_MIDI_OFF||h->kind==AM_MIDI_PROGRAM)return 0;
        out->data[0]=(uint8_t)((h->kind==AM_MIDI_CC?0xb0:0x80)|(h->status&15));out->data[1]=h->number;out->data[2]=h->kind==AM_MIDI_CC?0:in->data[2];return 1;
    }
    if((on||(cc_source&&in->data[2]))&&h->active){
        if(cc_source||h->kind==AM_MIDI_PITCHED)return 0; /* one gate until released */
        if(h->active<255)++h->active;
        if(h->kind==AM_MIDI_OFF)return 0;
        out->data[0]=h->status;out->data[1]=h->number;
        if(h->kind==AM_MIDI_PROGRAM)out->size=2;
        return 1;
    }
    int selected=-1;struct am_midi_map m={0};
    for(unsigned pad=0;pad<128;++pad){struct am_midi_map a;am_midi_map_get(pad,&a);
        if(direction==AM_MIDI_IN){
            if(a.in_kind&&scope(port,ch,a.in_port,a.in_channel)&&a.in_number==num&&
               ((a.in_kind==1&&(on||off||kind==0xa0))||(a.in_kind==2&&kind==0xb0))){selected=(int)pad;m=a;break;}
        }else if((on||off||kind==0xa0)&&a.out_kind&&native_note(pad)==(int)num&&scope(port,ch,a.out_port,0)){
            if(selected>=0)return 1; /* ambiguous native mapping: preserve stock */
            selected=(int)pad;m=a;
        }
    }
    unsigned action=AM_MIDI_NOTE;
    if(selected<0&&direction==AM_MIDI_IN&&(on||off)&&kind!=0xb0){
        int target,key;
        if(pitched_pad(ch,&target,&key)){
            if(off||target<0)return 0;
            int note=native_note((unsigned)target);if(note<0||note>127)return 0;
            int tune=(int)num-(int)((atomic_load(&mode_word)>>16)&127u);
            if(key==AM_MIDI_KEY_CHOP){if(tune<0||tune>15)return 0;tune+=1;} /* slice 1..16 */
            am_midi_tune_push((unsigned)note,tune,key==AM_MIDI_KEY_CHOP);
            {static unsigned logged;if(logged<200){++logged;fprintf(stderr,"AM_MIDI MODE_PUSH ch=%u key=%u pad=%d native=%d %s=%d\n",ch+1,num,target,note,key==AM_MIDI_KEY_CHOP?"slice":"tune",tune);}}
            unsigned c=atomic_load(voice_count+note);if(c<255)atomic_store(voice_count+note,(uint8_t)(c+1));
            out->data[0]=(uint8_t)(0x90|ch);out->data[1]=(uint8_t)note;
            *h=(struct am_midi_held){1,out->data[0],out->data[1],AM_MIDI_PITCHED};return 1;
        }
    }
    if(selected>=0){
        if(direction==AM_MIDI_IN){int note=native_note((unsigned)selected);if(note<0||note>127)return 1;
            out->data[0]=(uint8_t)(kind==0xa0?0xa0|ch:(off||(kind==0xb0&&!in->data[2]))?0x80|ch:0x90|ch);out->data[1]=(uint8_t)note;
            if(cc_source&&in->data[2])on=1;
        }else{
            action=m.out_kind;if(action==AM_MIDI_OFF){if(on)*h=(struct am_midi_held){1,0,0,AM_MIDI_OFF};return 0;}
            if(kind==0xa0&&action!=AM_MIDI_NOTE)return 0;
            unsigned target=m.out_channel?(unsigned)m.out_channel-1u:ch;out->data[1]=m.out_number;
            out->data[0]=(uint8_t)((action==AM_MIDI_CC?0xb0:action==AM_MIDI_PROGRAM?0xc0:kind==0xa0?0xa0:off?0x80:0x90)|target);
            if(action==AM_MIDI_CC&&off)out->data[2]=0;
            if(action==AM_MIDI_PROGRAM){out->size=2;if(off)return 0;}
        }
    }
    if(on){if(h->active&&h->active<255)++h->active;else *h=(struct am_midi_held){1,out->data[0],out->data[1],(uint8_t)action};}
    return 1;
}

/* Incoming keys currently held on the note path (stream owner only). Lets the
 * pad lights show a MIDI-played pad as pressed even when the cursor is hidden. */
static _Atomic unsigned held_notes;
unsigned am_midi_held_notes(void){return atomic_load(&held_notes);}
int am_midi_translate(struct am_midi_router*r,unsigned direction,unsigned port,const struct am_midi_message*in,struct am_midi_message*out){
    int rc=translate_impl(r,direction,port,in,out);
    if(direction==AM_MIDI_IN){
        unsigned n=0;
        for(unsigned ch=0;ch<16;++ch)for(unsigned k=0;k<128;++k){
            const struct am_midi_held*h=&r->held[0][ch][k];
            if(h->active&&(h->kind==AM_MIDI_NATIVE||h->kind==AM_MIDI_PITCHED||h->kind==AM_MIDI_NOTE))++n;
        }
        atomic_store(&held_notes,n);
    }
    return rc;
}
static uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;}
void am_midi_router_panic(struct am_midi_router*r,unsigned channel,am_midi_emit emit,void*user){
    if(!r||!emit||channel>16)return;
    unsigned targets=0;
    for(unsigned source=0;source<2;++source)for(unsigned ch=0;ch<16;++ch){
        if(channel<16&&ch!=channel)continue;
        for(unsigned note=0;note<128;++note){struct am_midi_held*h=&r->held[source][ch][note];if(!h->active)continue;h->active=0;
            if(h->kind==AM_MIDI_OFF||h->kind==AM_MIDI_PROGRAM)continue;
            struct am_midi_message off={3,{(uint8_t)((h->kind==AM_MIDI_CC?0xb0:0x80)|(h->status&15)),h->number,0}};
            targets|=1u<<(h->status&15);emit(user,&off);
        }
    }
    for(unsigned ch=0;ch<16;++ch)if(targets&(1u<<ch)){
        struct am_midi_message reset={3,{(uint8_t)(0xb0|ch),64,0}};emit(user,&reset);reset.data[1]=123;emit(user,&reset);
    }
}
int am_midi_encode(char*out,size_t cap){
 if(!out||cap<AM_MIDI_PAYLOAD)return 0;
 uint8_t wire[2340]={0};unsigned expanded=0,hasb=0,hasm=0;
 struct am_midi_mode mode;am_midi_mode_get(&mode);
 unsigned modal=mode.mode||mode.channel||mode.root!=60;for(unsigned i=0;i<16;++i)modal|=mode.pad[i]!=0||mode.kind[i]!=0;
 for(unsigned i=0;i<128;++i){struct am_midi_map m;am_midi_map_get(i,&m);memcpy(wire+i*8,&m,8);
  unsigned enabled;am_midi_layer_get(i,&m,&enabled);wire[1024+i*9]=(uint8_t)enabled;memcpy(wire+1025+i*9,&m,8);expanded|=enabled;
 }
 for(unsigned i=0;i<AM_MIDI_BUTTON_SLOTS;++i){struct am_midi_button b;am_midi_button_get(i,&b);hasb|=b.kind!=0;wire[2212+i*5]=b.port;wire[2213+i*5]=b.channel;wire[2214+i*5]=b.kind;wire[2215+i*5]=b.number;wire[2216+i*5]=b.code;wire[2292+i*3]=b.mnumber;wire[2293+i*3]=(uint8_t)(b.mkind|b.mport<<2);wire[2294+i*3]=b.mchannel;hasm|=b.mkind!=0;}
 unsigned bytes=hasm?2340:hasb?2292:modal?2212:expanded?2176:1024;memcpy(out,hasm?"AMM6 ":hasb?"AMM5 ":modal?"AMM4 ":expanded?"AMM2 ":"AMM1 ",5);modal|=hasb;
 if(modal){wire[2176]=mode.mode;wire[2177]=mode.channel;wire[2178]=mode.root;memcpy(wire+2179,mode.pad,16);memcpy(wire+2195,mode.kind,16);}
 static const char hex[]="0123456789abcdef";
 for(unsigned i=0;i<bytes;++i){out[5+i*2]=hex[wire[i]>>4];out[6+i*2]=hex[wire[i]&15];}
 int n=snprintf(out+5+bytes*2,cap-5-bytes*2," %08x\n",crc(wire,bytes));return n==10?(int)(15+bytes*2):0;
}
static int nibble(unsigned c){return c>='0'&&c<='9'?(int)c-'0':c>='a'&&c<='f'?(int)c-'a'+10:-1;}
int am_midi_restore(const char*text){
 if(!text){am_midi_reset();return 1;}
 size_t length=strnlen(text,AM_MIDI_PAYLOAD);unsigned bytes=length==2063?1024:length==4367?2176:length==4407?2196:length==4439?2212:length==4599?2292:length==4695?2340:0;
 if(!bytes||memcmp(text,bytes==1024?"AMM1 ":bytes==2176?"AMM2 ":bytes==2196?"AMM3 ":bytes==2212?"AMM4 ":bytes==2292?"AMM5 ":"AMM6 ",5)||text[5+bytes*2]!=' '||text[length-1]!='\n')return 0;
 uint8_t wire[2340]={0};uint32_t expected=0;struct am_midi_mode mode={0,0,60,{0},{0}};
 for(unsigned i=0;i<bytes;++i){int a=nibble((unsigned char)text[5+i*2]),b=nibble((unsigned char)text[6+i*2]);if(a<0||b<0)return 0;wire[i]=(uint8_t)((a<<4)|b);}
 for(unsigned i=6+bytes*2;i<14+bytes*2;++i){int n=nibble((unsigned char)text[i]);if(n<0)return 0;expected=(expected<<4)|(unsigned)n;}
 if(crc(wire,bytes)!=expected)return 0;
 for(unsigned i=0;i<128;++i){struct am_midi_map m;memcpy(&m,wire+i*8,8);if(!valid(&m))return 0;
  for(unsigned j=0;j<i;++j){struct am_midi_map other;memcpy(&other,wire+j*8,8);if(overlaps(&m,&other))return 0;}
  if(bytes>1024){memcpy(&m,wire+1025+i*9,8);if(wire[1024+i*9]>1||!valid(&m)||m.in_kind||m.in_port||m.in_channel||m.in_number)return 0;}
 }
 if(bytes>=2196){mode.mode=wire[2176];mode.channel=wire[2177];mode.root=wire[2178];memcpy(mode.pad,wire+2179,16);if(bytes>=2212)memcpy(mode.kind,wire+2195,16);if(!mode_valid(&mode))return 0;}
 for(unsigned i=0;i<128;++i){struct am_midi_map m;memcpy(&m,wire+i*8,8);atomic_store_explicit(maps+i,pack(&m),memory_order_release);
  memcpy(&m,wire+1025+i*9,8);atomic_store(level_maps+i,pack(&m));atomic_store(level_enabled+i,wire[1024+i*9]);
 }
 if(bytes>=2292)for(unsigned i=0;i<AM_MIDI_BUTTON_SLOTS;++i){struct am_midi_button b={wire[2212+i*5],wire[2213+i*5],wire[2214+i*5],wire[2215+i*5],wire[2216+i*5],0,0,0,0};if(bytes==2340){b.mnumber=wire[2292+i*3];b.mkind=wire[2293+i*3]&3;b.mport=(uint8_t)(wire[2293+i*3]>>2);b.mchannel=wire[2294+i*3];}if(!bvalid(&b))return 0;}
 for(unsigned i=0;i<AM_MIDI_BUTTON_SLOTS;++i){struct am_midi_button b={0,0,0,0,0,0,0,0,0};if(bytes>=2292){b.port=wire[2212+i*5];b.channel=wire[2213+i*5];b.kind=wire[2214+i*5];b.number=wire[2215+i*5];b.code=wire[2216+i*5];if(bytes==2340){b.mnumber=wire[2292+i*3];b.mkind=wire[2293+i*3]&3;b.mport=(uint8_t)(wire[2293+i*3]>>2);b.mchannel=wire[2294+i*3];}}atomic_store(bslots+i,bpack(&b));}atomic_store(&modheld,0);
 am_midi_mode_set(&mode);memset((void*)voice_count,0,sizeof voice_count);
 am_midi_learn(0,0);atomic_fetch_add(&revision,1);return 1;
}
void am_midi_learn(unsigned pad,int enabled){atomic_store(&learn_message,0);atomic_store(&learn_btn,0);atomic_store(&learn_pad,enabled&&pad<128?pad+1:0);}
void am_midi_learn_retarget(unsigned pad){unsigned old=atomic_load(&learn_pad);if(!old||pad>=128)return;atomic_store(&learn_message,0);atomic_store(&learn_pad,pad+1);}
int am_midi_learn_active(void){return atomic_load(&learn_pad)!=0;}
void am_midi_learn_tick(void){uint64_t n=atomic_exchange(&learn_message,0);
    if(atomic_load(&learn_btn)){
        unsigned lb=atomic_load(&learn_btn)-1;uint64_t first=atomic_load(&learn_first);
        if(lb&0x80u){if(n)button_commit(n,0);return;} /* knobs: no combos */
        if(!first){if(n)atomic_store(&learn_first,n);return;}
        #define SRC(x) ((x)&~1ull&~(0xfull<<16)) /* port + number + kind nibble */
        if(n&&SRC(n)!=SRC(first)){button_commit(n,first);return;}
        if(atomic_load(&learn_rel)){button_commit(first,0);return;}
        return;
    }unsigned p=atomic_load(&learn_pad);if(!n||!p)return;struct am_midi_map m;am_midi_map_get(p-1,&m);m.in_port=(uint8_t)(n>>32);m.in_channel=(uint8_t)(((n>>16)&15)+1);m.in_kind=((n>>20)&15)==9?1:2;m.in_number=(uint8_t)(n>>8);if(am_midi_map_set(p-1,&m))atomic_store(&learn_pad,0);}
void am_midi_observe(unsigned dir,unsigned port,const struct am_midi_message*m){
    if(port==AM_MIDI_SURFACE||!m||!m->size||m->size>3)return;
    if(dir==AM_MIDI_IN&&port>0&&port<3&&(atomic_load(&learn_pad)||atomic_load(&learn_btn))&&m->size==3&&((m->data[0]&0xf0)==0x90||(m->data[0]&0xf0)==0xb0)&&m->data[2]){uint64_t empty=0,value=((uint64_t)port<<32)|((uint64_t)m->data[0]<<16)|((uint64_t)m->data[1]<<8)|1;atomic_compare_exchange_strong(&learn_message,&empty,value);}
    if(dir==AM_MIDI_IN&&port>0&&port<3&&atomic_load(&learn_btn)&&m->size==3){uint64_t f=atomic_load(&learn_first);unsigned k=m->data[0]&0xf0;
        if(f&&(k==0x80||((k==0x90||k==0xb0)&&!m->data[2]))&&m->data[1]==(uint8_t)(f>>8)&&(uint8_t)port==(uint8_t)(f>>32)&&((f>>16)&15)==(m->data[0]&15u))atomic_store(&learn_rel,1);}
    if(dir==AM_MIDI_IN&&am_synth_midi&&(atomic_load(&mode_word)&255)!=AM_MIDI_MODE_INSTRUMENT&&!((atomic_load(&mode_word)&255)==AM_MIDI_MODE_CHANNELS&&m->size>0&&atomic_load(mode_pad+(m->data[0]&15))&&atomic_load(mode_kind+(m->data[0]&15))==AM_MIDI_KEY_SYNTH))am_synth_midi(m->data,m->size);
    if(atomic_load(&paused))return;
    if(pthread_mutex_trylock(&log_lock)){atomic_fetch_add(&dropped,1);return;}
    unsigned at=(log_head+log_used)%AM_MIDI_LOG;if(log_used==AM_MIDI_LOG){at=log_head;log_head=(log_head+1)%AM_MIDI_LOG;}else ++log_used;
    records[at]=(struct am_midi_record){++log_serial,milliseconds(),(uint8_t)dir,(uint8_t)port,m->size,{m->data[0],m->data[1],m->data[2]}};
    atomic_fetch_add(&revision,1);pthread_mutex_unlock(&log_lock);
}
unsigned am_midi_snapshot(struct am_midi_record*out,unsigned cap,uint64_t*latest){
    if(!out||!cap)return 0;
    if(pthread_mutex_trylock(&log_lock))return 0;
    unsigned n=log_used<cap?log_used:cap;
    for(unsigned i=0;i<n;++i)out[i]=records[(log_head+log_used-n+i)%AM_MIDI_LOG];
    if(latest)*latest=log_serial;
    pthread_mutex_unlock(&log_lock);return n;
}
void am_midi_log_clear(void){pthread_mutex_lock(&log_lock);log_used=log_head=0;clear_serial=log_serial;diagnostic_serial=log_serial;pthread_mutex_unlock(&log_lock);atomic_fetch_add(&revision,1);}
void am_midi_log_pause(int on){atomic_store(&paused,on!=0);atomic_fetch_add(&revision,1);}
int am_midi_log_paused(void){return atomic_load(&paused)!=0;}
uint64_t am_midi_log_revision(void){return atomic_load(&revision);}
uint64_t am_midi_log_dropped(void){return atomic_load(&dropped);}
void am_midi_io_identity(unsigned port,const char*id,const char*name){fprintf(stderr,"AM_MIDI PORT kind=%s id=%.48s name=%.48s\n",am_midi_port_name(port),id?id:"",name?name:"");}
void am_midi_diagnostic_tick(void){
    struct am_midi_record batch[AM_MIDI_LOG];uint64_t latest=0;unsigned n=am_midi_snapshot(batch,AM_MIDI_LOG,&latest),budget=32;
    for(unsigned i=0;i<n&&budget&&diagnostic_lines<4096;++i){struct am_midi_record*r=batch+i;if(r->serial<=diagnostic_serial||r->serial<=clear_serial)continue;
        /* Clock/active sensing remain in the live ring but never flood the diagnostic. */
        diagnostic_serial=r->serial;if(r->data[0]==0xf8||r->data[0]==0xfe)continue;
        fprintf(stderr,"AM_MIDI WIRE %s port=%s t=%llu seq=%llu bytes=%02x %02x %02x size=%u\n",r->direction?"OUT_ACCEPTED":"IN_RECEIVED",am_midi_port_name(r->port),(unsigned long long)r->ms,(unsigned long long)r->serial,r->data[0],r->data[1],r->data[2],r->size);--budget;++diagnostic_lines;
    }
}
