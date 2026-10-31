#define _GNU_SOURCE
#include "music_settings.h"
#ifdef MPC_PAD_EQ
#include "pad_eq.h"
#endif
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const music_note_names[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
const char *const music_scale_names[8]={"Major","Natural Minor","Dorian","Phrygian","Lydian","Mixolydian","Locrian","Chromatic"};
const char *const music_colour_names[12]={"Green","Cyan","Blue","Violet","Magenta","Red","Orange","Amber","Yellow","White","Grey","Off"};
const uint32_t music_colours[12]={0x00e060,0x00c8ff,0x2030ff,0x8830ff,0xff20b0,0xff0000,0xff6000,0xffb830,0xffe000,0xffffff,0x505050,0};
static const uint16_t scale_masks[8]={0xab5,0x5ad,0x6ad,0x5ab,0xad5,0x6b5,0x56b,0xfff};
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static struct music_project project;
static _Atomic uint64_t generation=1, published;
static _Atomic int key_edited;
/* 0 is the stock colour (the native pad colour passes through); the old
 * Cyan moved to the end. */
const char *const music_sequence_colour_names[MUSIC_SEQUENCE_COLOURS]={"Stock","Green","Amber","Violet","Magenta","Red","Orange","Yellow","Blue","Lime","Mint","White","Cyan"};
const uint32_t music_sequence_colours[MUSIC_SEQUENCE_COLOURS]={0x40ff40,0x32c780,0xffc247,0x8830ff,0xff20b0,0xff0000,0xff6000,0xffe000,0x2030ff,0x98ff00,0x00ffb0,0xffffff,0x00c8ff};
int music_sequence_colour(unsigned sequence){
    if(sequence>=128)return 0;
    pthread_mutex_lock(&mutex);int v=project.sequence_colours[sequence];pthread_mutex_unlock(&mutex);return v;
}
int music_set_sequence_colour(unsigned sequence,unsigned colour){
    if(sequence>=128||colour>=MUSIC_SEQUENCE_COLOURS)return 0;
    pthread_mutex_lock(&mutex);project.sequence_colours[sequence]=(unsigned char)colour;pthread_mutex_unlock(&mutex);
    atomic_fetch_add(&generation,1);return 1;
}

int music_pad_colour(unsigned pad){
    if(pad>=128)return 0;
    pthread_mutex_lock(&mutex);int v=project.pad_colours[pad];pthread_mutex_unlock(&mutex);return v;
}
int music_set_pad_colour(unsigned pad,unsigned colour){
    if(pad>=128||colour>=MUSIC_SEQUENCE_COLOURS)return 0;
    pthread_mutex_lock(&mutex);project.pad_colours[pad]=(unsigned char)colour;pthread_mutex_unlock(&mutex);
    atomic_fetch_add(&generation,1);return 1;
}
int music_pad_colours_used(void){
    int used=0;pthread_mutex_lock(&mutex);for(unsigned i=0;i<128&&!used;++i)used=project.pad_colours[i]!=0;pthread_mutex_unlock(&mutex);return used;
}

static void sample_default(struct music_sample *p,const char *name)
{
    memset(p,0,sizeof(*p)); p->root=-1; p->tonic=-1; p->scale=-1;
    p->colour[0]=0; p->colour[1]=1; p->colour[2]=10;
    p->original_colour=4; /* magenta distinguishes original pitch from tonic */
    if (name) memcpy(p->name,name,strlen(name)+1);
}
void music_defaults(struct music_project *p) { memset(p,0,sizeof(*p)); }
void music_reset(void)
{
    pthread_mutex_lock(&mutex); music_defaults(&project); pthread_mutex_unlock(&mutex);
#ifdef MPC_PAD_EQ
    uint64_t empty[128]={0};pad_eq_publish_all(empty);
#endif
    atomic_store(&published,0); atomic_store(&key_edited,0); atomic_fetch_add(&generation,1);
}
uint64_t music_generation(void) { return atomic_load(&generation); }
void music_get_key(int *tonic,int *scale)
{
    pthread_mutex_lock(&mutex); *tonic=project.tonic; *scale=project.scale; pthread_mutex_unlock(&mutex);
}
void music_set_key(int tonic,int scale)
{
    if (tonic<0 || tonic>11 || scale<0 || scale>7) return;
    pthread_mutex_lock(&mutex); project.tonic=tonic; project.scale=scale; pthread_mutex_unlock(&mutex);
    atomic_store(&key_edited,1);
    atomic_fetch_add(&generation,1);
}
int music_get_sample(const char *name,struct music_sample *out)
{
    if (!name || !*name || strnlen(name,MUSIC_NAME_BYTES)>=MUSIC_NAME_BYTES) return 0;
    sample_default(out,name);
    int found=0;
    pthread_mutex_lock(&mutex);
    for (unsigned i=0;i<project.count;++i) if (!strcmp(project.samples[i].name,name)) {
        *out=project.samples[i]; found=1; break;
    }
    pthread_mutex_unlock(&mutex); return found;
}
int music_set_sample(const char *name,int field,int value)
{
    if (!name || !*name || strnlen(name,MUSIC_NAME_BYTES)>=MUSIC_NAME_BYTES) return 0;
    if ((field==MUSIC_ROOT && (value< -1 || value>11))
        || (field==MUSIC_MODE && (value<0 || value>4))
        || (field==MUSIC_TRANSPOSE && (value< -24 || value>24))
        || (field>=MUSIC_COLOUR0 && field<=MUSIC_COLOUR2 && (value<0 || value>11))
        || (field==MUSIC_LOCAL_TONIC && (value< -1 || value>11))
        || (field==MUSIC_LOCAL_SCALE && (value< -1 || value>7))
        || (field==MUSIC_ORIGINAL_COLOUR && (value<0||value>11))
        || field<0 || field>MUSIC_ORIGINAL_COLOUR) return 0;
    pthread_mutex_lock(&mutex);
    unsigned i;
    for (i=0;i<project.count;++i) if (!strcmp(project.samples[i].name,name)) break;
    if (i==project.count) {
        if (project.count==MUSIC_MAX_SAMPLES) { pthread_mutex_unlock(&mutex); return 0; }
        sample_default(&project.samples[project.count++],name);
    }
    struct music_sample *p=&project.samples[i];
    if (field==MUSIC_ROOT) p->root=value;
    else if (field==MUSIC_MODE) p->mode=value;
    else if (field==MUSIC_TRANSPOSE) p->transpose=value;
    else if (field==MUSIC_LOCAL_TONIC) p->tonic=value;
    else if (field==MUSIC_LOCAL_SCALE) p->scale=value;
    else if (field==MUSIC_ORIGINAL_COLOUR) p->original_colour=value;
    else p->colour[field-MUSIC_COLOUR0]=value;
    pthread_mutex_unlock(&mutex); atomic_fetch_add(&generation,1); return 1;
}
void music_snapshot(struct music_project *out)
{ pthread_mutex_lock(&mutex); *out=project; pthread_mutex_unlock(&mutex); }
void music_replace(const struct music_project *p)
{
    pthread_mutex_lock(&mutex); project=*p; pthread_mutex_unlock(&mutex);
#ifdef MPC_PAD_EQ
    pad_eq_publish_all(p->pad_eq);
#endif
    atomic_store(&published,0); atomic_store(&key_edited,0); atomic_fetch_add(&generation,1);
}
uint64_t music_get_pad_eq(unsigned pad){
    if(pad>=128)return 0;
    pthread_mutex_lock(&mutex);uint64_t v=project.pad_eq[pad];pthread_mutex_unlock(&mutex);return v;
}
int music_set_pad_eq(unsigned pad,uint64_t v){
    if(pad>=128)return 0;
#ifdef MPC_PAD_EQ
    if(!pad_eq_valid(v))return 0;
#else
    if(v)return 0;
#endif
    pthread_mutex_lock(&mutex);project.pad_eq[pad]=v;pthread_mutex_unlock(&mutex);
#ifdef MPC_PAD_EQ
    pad_eq_publish(pad,v);
#endif
    atomic_fetch_add(&generation,1);return 1;
}
void music_key_text(int tonic,int scale,char *out,size_t size)
{ snprintf(out,size,"%s %s",music_note_names[tonic],music_scale_names[scale]); }
int music_key_edited(void) { return atomic_load(&key_edited); }
int music_key_from_text(const char *text,int *tonic,int *scale)
{
    *tonic=0; *scale=0; if (!text) return 0;
    for (int i=0;i<12;++i) for (int j=0;j<8;++j) {
        char key[48]; music_key_text(i,j,key,sizeof(key));
        if (!strcmp(key,text)) { *tonic=i; *scale=j; return 1; }
    }
    return 0;
}

/* Versioned ASCII payload carried in a named native JSON string field. Names
 * are hex UTF-8, so delimiters/newlines cannot alter the record structure. */
int music_encode(const struct music_project *p,char *out,size_t size)
{
    if (p->count>MUSIC_MAX_SAMPLES) return 0;
    unsigned extended=0;
    unsigned pads=0;
    for(unsigned i=0;i<128;++i){if(p->sequence_colours[i]>=MUSIC_SEQUENCE_COLOURS)return 0;if(p->sequence_colours[i]>=5)extended=1;}
    for(unsigned i=0;i<128;++i){if(p->pad_colours[i]>=MUSIC_SEQUENCE_COLOURS)return 0;if(p->pad_colours[i])pads=1;}
    /* Version 7 only when a pad colour is set, so older builds keep reading
     * projects that do not use the feature. */
    int n=snprintf(out,size,"MPCS1%c %d %d %u\n",pads?'7':extended?'6':'5',p->tonic,p->scale,p->count);
    if (n<0 || (size_t)n>=size) return 0;
    size_t used=(size_t)n; const char hex[]="0123456789abcdef";
    for (unsigned i=0;i<p->count;++i) {
        const struct music_sample *s=&p->samples[i];
        n=snprintf(out+used,size-used,"%d %d %d %d %d %d %d %d %d\n",s->root,s->mode,s->transpose,s->colour[0],s->colour[1],s->colour[2],s->tonic,s->scale,s->original_colour);
        if (n<0 || (size_t)n>=size-used) return 0;
        used+=(size_t)n;
        size_t len=strnlen(s->name,MUSIC_NAME_BYTES);
        if (!len || len>=MUSIC_NAME_BYTES || size-used<=len*2+1) return 0;
        for (size_t j=0;j<len;++j) { unsigned char c=(unsigned char)s->name[j]; out[used++]=hex[c>>4]; out[used++]=hex[c&15]; }
        out[used++]='\n'; out[used]=0;
    }
    n=snprintf(out+used,size-used,"EQ128\n");if(n<0||(size_t)n>=size-used)return 0;used+=(size_t)n;
    for(unsigned pad=0;pad<128;++pad){
        uint64_t v=p->pad_eq[pad];
#ifdef MPC_PAD_EQ
        if(!pad_eq_valid(v))return 0;
#endif
        n=snprintf(out+used,size-used,"%08llx\n",(unsigned long long)v);if(n!=9||(size_t)n>=size-used)return 0;used+=(size_t)n;
    }
    n=snprintf(out+used,size-used,"SEQ128\n");if(n!=7||(size_t)n>=size-used)return 0;used+=(size_t)n;
    if(size-used<=129)return 0;
    for(unsigned i=0;i<128;++i)out[used++]=hex[p->sequence_colours[i]];
    out[used++]='\n';out[used]=0;
    if(pads){
        n=snprintf(out+used,size-used,"PAD128\n");if(n!=7||(size_t)n>=size-used)return 0;used+=(size_t)n;
        if(size-used<=129)return 0;
        for(unsigned i=0;i<128;++i)out[used++]=hex[p->pad_colours[i]];
        out[used++]='\n';out[used]=0;
    }
    return (int)used;
}
static int number(const char **cursor,const char *end,int *out,char delimiter)
{
    const char *s=*cursor; int sign=1,value=0,digits=0;
    if (s<end && *s=='-') { sign=-1; ++s; }
    while (s<end && *s>='0' && *s<='9') {
        if (++digits>6) return 0;
        value=value*10+*s++-'0';
    }
    if (!digits || s==end || *s++!=delimiter) return 0;
    *out=sign*value; *cursor=s; return 1;
}
static int unhex(char c) { return c>='0' && c<='9' ? c-'0' : c>='a' && c<='f' ? c-'a'+10 : -1; }
int music_decode(const char *in,size_t size,struct music_project *out)
{
    if (size<12 || size>=MUSIC_PAYLOAD_BYTES || (memcmp(in,"MPCS12 ",7)&&memcmp(in,"MPCS13 ",7)&&memcmp(in,"MPCS14 ",7)&&memcmp(in,"MPCS15 ",7)&&memcmp(in,"MPCS16 ",7)&&memcmp(in,"MPCS17 ",7))) return 0;
    int version13=in[5]>='3',version14=in[5]>='4',version15=in[5]>='5',version16=in[5]>='6',version17=in[5]=='7';
    const char *s=in+7,*end=in+size; int count;
    music_defaults(out);
    if (!number(&s,end,&out->tonic,' ') || !number(&s,end,&out->scale,' ')
        || !number(&s,end,&count,'\n') || out->tonic<0 || out->tonic>11
        || out->scale<0 || out->scale>7 || count<0 || count>MUSIC_MAX_SAMPLES) return 0;
    out->count=(unsigned)count;
    for (unsigned i=0;i<out->count;++i) {
        struct music_sample *p=&out->samples[i];
        if (!number(&s,end,&p->root,' ') || !number(&s,end,&p->mode,' ')
            || !number(&s,end,&p->transpose,' ') || !number(&s,end,&p->colour[0],' ')
            || !number(&s,end,&p->colour[1],' ') || !number(&s,end,&p->colour[2],' ')
            || !number(&s,end,&p->tonic,' ') || !number(&s,end,&p->scale,version13?' ':'\n')) return 0;
        p->original_colour=4;
        if(version13&&!number(&s,end,&p->original_colour,'\n'))return 0;
        if (p->root< -1 || p->root>11 || p->mode<0 || p->mode>(version13?4:3) || p->transpose< -24 || p->transpose>24 || p->original_colour<0 || p->original_colour>11) return 0;
        for (unsigned j=0;j<3;++j) if (p->colour[j]<0 || p->colour[j]>11) return 0;
        if (p->tonic< -1 || p->tonic>11 || p->scale< -1 || p->scale>7) return 0;
        size_t n=0;
        while (s<end && *s!='\n') {
            if (n>=MUSIC_NAME_BYTES-1 || end-s<2) return 0;
            int a=unhex(*s++),b=unhex(*s++);
            if (a<0 || b<0 || !(a|b)) return 0;
            p->name[n++]=(char)((a<<4)|b);
        }
        if (!n || s==end || *s++!='\n') return 0;
        p->name[n]=0;
        for (unsigned j=0;j<i;++j) if (!strcmp(p->name,out->samples[j].name)) return 0;
    }
    if(version14){
        if(end-s<6||memcmp(s,"EQ128\n",6))return 0;
        s+=6;
        for(unsigned pad=0;pad<128;++pad){uint64_t v=0;if(end-s<9)return 0;
            for(unsigned j=0;j<8;++j){int d=unhex(*s++);if(d<0)return 0;v=(v<<4)|(unsigned)d;}
            if(*s++!='\n')return 0;
#ifdef MPC_PAD_EQ
            if(!pad_eq_valid(v))return 0;
#endif
            out->pad_eq[pad]=v;
        }
    }
    if(version15){
        if(end-s!=(version17?272:136)||memcmp(s,"SEQ128\n",7))return 0;
        s+=7;
        for(unsigned i=0;i<128;++i){int v=unhex(*s++);if(v<0||v>=(version16?MUSIC_SEQUENCE_COLOURS:5))return 0;out->sequence_colours[i]=(unsigned char)v;}
        if(*s++!='\n')return 0;
        if(version17){
            if(memcmp(s,"PAD128\n",7))return 0;
            s+=7;
            for(unsigned i=0;i<128;++i){int v=unhex(*s++);if(v<0||v>=MUSIC_SEQUENCE_COLOURS)return 0;out->pad_colours[i]=(unsigned char)v;}
            if(*s++!='\n')return 0;
        }
    }
    return s==end;
}

uint64_t music_light_snapshot(const struct music_sample *p,int semitones)
{
    int tonic,scale; music_get_key(&tonic,&scale);
    if (!p || p->root< -1 || p->root>11 || p->mode<0 || p->mode>MUSIC_DEGREES
        || p->transpose< -24 || p->transpose>24 || p->tonic< -1 || p->tonic>11
        || p->scale< -1 || p->scale>7 || p->original_colour<0 || p->original_colour>11
        || semitones< -96 || semitones>96) return 0;
    for(unsigned i=0;i<3;++i)if(p->colour[i]<0||p->colour[i]>11)return 0;
    if (p->tonic>=0) tonic=p->tonic;
    if (p->scale>=0) scale=p->scale;
    int mode=p->mode==MUSIC_TONIC||p->mode==MUSIC_DEGREES?MUSIC_SCALE:p->mode;
    uint64_t v=(uint64_t)(p->root+1) | ((uint64_t)(mode&3)<<4) | ((uint64_t)(p->transpose+24)<<6)
        | ((uint64_t)(semitones+96)<<12) | ((uint64_t)tonic<<20) | ((uint64_t)scale<<24);
    for (unsigned i=0;i<3;++i) v|=(uint64_t)p->colour[i]<<(28+i*4);
    v|=(uint64_t)(mode>>2)<<40;
    v|=(uint64_t)p->original_colour<<41;
    return v;
}
void music_publish(const struct music_sample *p,int semitones)
{ atomic_store(&published,music_light_snapshot(p,semitones)); }
uint64_t music_published(void) { return atomic_load(&published); }
/* The Lights page offers Monochrome, Piano and Scale. Tonic and Degrees were
 * dropped in r25f; projects that chose them light as Scale. */
int music_mode_ui_index(int mode){return mode==MUSIC_MONO?0:mode==MUSIC_PIANO?1:2;}
int music_mode_from_ui_index(int index){return index<=0?MUSIC_MONO:index==1?MUSIC_PIANO:MUSIC_SCALE;}
int music_transposition(void) { uint64_t v=music_published(); return v ? (int)((v>>6)&63)-24 : 0; }
uint64_t music_pitch_light(uint64_t v,int semitones,int *replace)
{
    *replace=0;
    int root=(int)(v&15)-1,mode=(int)((v>>4)&3)|((int)((v>>40)&1)<<2);
    if (!v || root<0 || mode==MUSIC_MONO || semitones< -120 || semitones>120) return 0;
    int note=(root+(int)((v>>12)&255)-96+semitones)%12;
    if (note<0) note+=12;
    uint32_t colour; float brightness;
    if (mode==MUSIC_PIANO) {
        int natural=(0xab5u>>note)&1u;
        colour=natural ? 0xffffff : 0x2030ff; brightness=natural ? 0.35f : 0.10f;
    } else {
        int degree=(note-(int)((v>>20)&15)+12)%12;
        unsigned mask=scale_masks[(v>>24)&7];
        int role=degree==0 ? 0 : mode>=MUSIC_SCALE && (mask&(1u<<degree)) ? 1 : 2;
        colour=music_colours[(v>>(28+role*4))&15];
        brightness=role==0 ? 0.60f : role==1 ? 0.28f : 0.08f;
        if(mode==MUSIC_DEGREES&&(mask&(1u<<degree))){
            static const unsigned palette[11]={0,1,2,3,6,7,8,5,9,4,10};
            unsigned ordinal=0;for(int i=0;i<degree;++i)ordinal+=(mask>>i)&1u;
            unsigned original=(v>>41)&15,available=original<11?10:11;
            ordinal%=available;
            for(unsigned i=0;i<11;++i)if(palette[i]!=original){if(!ordinal--){colour=music_colours[palette[i]];break;}}
            brightness=.45f;
        }
    }
    /* Mark the reference pitch, not every octave of its pitch class. */
    if(semitones==0){colour=music_colours[(v>>41)&15];brightness=.75f;}
    uint32_t bits; memcpy(&bits,&brightness,sizeof(bits)); *replace=1;
    return colour|((uint64_t)bits<<32);
}
uint64_t music_pad_light(uint64_t v,unsigned pad,int original_pad,int *replace)
{
    if(original_pad<0||original_pad>15){*replace=0;return 0;}
    return music_pitch_light(v,(int)(pad&15)-original_pad+(int)((v>>6)&63)-24,replace);
}
