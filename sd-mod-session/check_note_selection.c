/* Native model fixtures exercise production selection while Sample is hidden. */
#include "src/note_native.c"
#include <assert.h>
#include <sys/mman.h>
static unsigned levels,top_sample=1,releases;
static unsigned char context[0x700],instrument[0x2400],other[0x2400],layer[0x800],sample_value[0x140];
static void *normal_instrument=instrument;
int music_ui_chords_available(void){return top_sample;}
int music_lighting_levels_state(unsigned *e,unsigned *m,unsigned *o){*e=levels;*m=1;*o=3;return 1;}
static void *get_instrument(void *p,int index){(void)p;(void)index;return normal_instrument;}
static int get_index(void *p,void *i){(void)p;return i==other?115:0;}
static int yes(void *p){(void)p;return 1;}
static int no(void *p){(void)p;return 0;}
static void noop(void *p){(void)p;}
static void release(void *p){assert(p==sample_value);++releases;}
void step_layer_sample(void *i,unsigned n,uintptr_t *out,uintptr_t fn){
    assert((i==instrument||i==other)&&n==0&&fn==base+0x2803b90);
    out[0]=out[1]=(uintptr_t)sample_value;
}
static void stub(unsigned offset,void *fn){
    uint32_t op[]={0x58000050,0xd61f0200};
    memcpy((void*)(base+offset),op,8);memcpy((void*)(base+offset+8),&fn,8);
}
int main(void){
    void *memory=mmap(NULL,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(memory!=MAP_FAILED);
    base=(uintptr_t)memory;
    stub(0x286bbe0,get_instrument);stub(0x286bc20,get_index);stub(0x123bca0,release);
    stub(0x112fd50,yes);stub(0x112f6b0,no);stub(0x1149880,noop);
    __builtin___clear_cache(memory,(char*)memory+0x7500000);
    unsigned char screen[0x6300]={0};root=screen;slider=screen+SEMI_OFFSET;
    FIELD(root,0,uintptr_t)=base+SAMPLE_VPTR;FIELD(slider,0,uintptr_t)=base+SLIDER_VPTR;
    FIELD(root,0x148,void*)=context;FIELD(context,0xb8,void*)=context;
    FIELD(instrument,0x22e8,void*)=FIELD(other,0x22e8,void*)=layer;
    FIELD(sample_value,0xa8,const char*)="Scale sample";
    FIELD(instrument,0xb98,int8_t)=-5;FIELD(layer,0x60,int8_t)=2;
    music_reset();assert(music_set_sample("Scale sample",MUSIC_ROOT,5));
    for(unsigned i=0;i<128;++i){
        FIELD(context,0x5f8,unsigned)=i;assert(selection());unsigned pad=128;
        assert(note_native_pad_index(&pad)&&pad==i);
        char expected[16];snprintf(expected,sizeof(expected),"%c%02u",'A'+i/16,i%16+1);
        assert(!strcmp(expected,selected_pad)&&original_note==5&&selected_tuning==-3);
    }
    assert(releases==128);
    /* In 16 Levels the source instrument is fixed, despite pad/key input. */
    levels=1;FIELD(context,0x668,void*)=other;FIELD(context,0x5f8,unsigned)=2;
    FIELD(other,0xb98,int8_t)=7;assert(selection());
    assert(!strcmp(selected_pad,"H04")&&selected_tuning==9);
    levels=0;normal_instrument=NULL;assert(!selection());
    unsigned empty=128;assert(note_native_pad_index(&empty)&&empty==2);
    normal_instrument=instrument;
    /* Native Sample is still showing underneath Project. Neither hidden
     * Orig Note nor Pad Colour may take its controls or first encoder click. */
    FIELD(root,0x190+0x21c,int)=2;FIELD(root,0x190+0x218,int)=1;
    FIELD(root,MIX_VOLUME_OFFSET,uintptr_t)=base+SLIDER_VPTR;
    FIELD((void*)(base+0x723cde0),0,uintptr_t)=(uintptr_t)root+MIX_VOLUME_OFFSET+0x1d8;
    unsigned char event[16]={0};event[8]=0x6f;event[9]=1;
    top_sample=0;assert(!showing()&&!trim_showing()&&!note_native_button(event));
    top_sample=1;assert(showing()&&trim_showing()&&note_native_button(event));
    top_sample=0;event[9]=0;assert(note_native_button(event));
    assert(!note_native_button(event));
    munmap(memory,0x7500000);
    puts("PASS model selection: all 128 pads, hidden Sample, Levels source, Tune, empty pad, Project encoder ownership");
}
