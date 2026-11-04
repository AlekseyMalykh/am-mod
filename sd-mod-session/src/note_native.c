#define _GNU_SOURCE
#include "note_native.h"
#include "music_settings.h"
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

/* Sample names are the native project sample-pool reference keys. The project
 * archive restores the per-project map. No cross-project filename registry. */
static uintptr_t base;
static void *root, *slider;
static _Atomic(uintptr_t) identity;
static _Atomic unsigned destroyed, active;
static _Atomic unsigned requested_shift, focused_k1,repeat_held;
static _Atomic unsigned trim_active,focused_trim;
static unsigned colour_push_owned;
static void give_back_knob_focus(void);
static int original_note = -1, previous_shift = -1, selection_valid;
static char selected_pad[16], selected_sample[513];
static unsigned log_count;
static uint64_t selection_generation;
#define FN(off,type) ((type)(base+(off)))
#define FIELD(p,off,type) (*(type *)((char *)(p)+(off)))
#define SAMPLE_VPTR 0x6e212a0
#define SLIDER_VPTR 0x706e5d0
#define SEMI_OFFSET 0x60b8
extern void note_label_string(void *label,uint64_t *out,uintptr_t function);
static const char *notes[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

static void log_note(const char *s)
{
    if (log_count++ < 24) fprintf(stderr,"SAMPLE_NOTE %s\n",s);
}

void note_native_init(uintptr_t b) { base=b; }
int note_native_active(void) { return (int)(atomic_load(&active)||atomic_load(&trim_active)); }
void note_native_button_requested(unsigned code,unsigned down)
{
    if (code==0x31) atomic_store(&requested_shift,!!down);
    if (code==0x0b) atomic_store(&repeat_held,!!down);
}
int note_native_owns(unsigned cc)
{
    if (atomic_load(&repeat_held)) return 0;
    if (!atomic_load(&requested_shift))
        return atomic_load(&trim_active) && (cc==0x11 || (cc==0x64 && atomic_load(&focused_trim)));
    return atomic_load(&active) && (cc==0x10 || (cc==0x64 && atomic_load(&focused_k1)));
}
void note_native_destroyed(void *p)
{
    if ((uintptr_t)p == atomic_load(&identity)) {
        atomic_store(&active,0);
        atomic_store(&identity,0);
        atomic_store(&destroyed,1);
    }
}

static void forget(void)
{
    atomic_store(&active,0); atomic_store(&identity,0);
    atomic_store(&requested_shift,0); atomic_store(&focused_k1,0); atomic_store(&trim_active,0); atomic_store(&focused_trim,0);
    root=0; slider=0; original_note=-1; previous_shift=-1;
    selection_valid=0; selected_pad[0]=0; selected_sample[0]=0;
    selection_generation=0;
}

static int showing(void)
{
    if (atomic_exchange(&destroyed,0)) forget();
    if (atomic_load(&repeat_held) || !root || !slider || FIELD(root,0,uintptr_t)!=base+SAMPLE_VPTR
        || FIELD(slider,0,uintptr_t)!=base+SLIDER_VPTR) return 0;
    return FN(0x112fd50,int(*)(void*))(slider)
        && !FN(0x112f6b0,int(*)(void*))(slider);
}

/* Mix has an empty K2 without Shift. Pad Colour fills that slot; the native
 * Shift+K2 Kit Volume control and K1 Volume keep their own handlers. */
/* Mix page K1 "Volume" (SampleScreen+0x4638, the Mix anchor in ui-audit). */
#define MIX_VOLUME_OFFSET 0x4638
static void *volume_slider_here(void)
{
    if (!root || FIELD(root,0,uintptr_t)!=base+SAMPLE_VPTR) return NULL;
    void *s=(char*)root+MIX_VOLUME_OFFSET;
    return FIELD(s,0,uintptr_t)==base+SLIDER_VPTR ? s : NULL;
}
static void *trim_slider(void){ return volume_slider_here(); }
/* Mix is up when the B1 top control (Sample+0x190) is a sub-tab switch
 * (+0x21c == 2) with index 1 (+0x218: 0 Trim, 1 Mix, 2 Amp Env), and the
 * Sample screen itself is showing. */
static int trim_showing(void)
{
    if (!root || FIELD(root,0,uintptr_t)!=base+SAMPLE_VPTR || atomic_load(&repeat_held)) return 0;
    void *b1=(char*)root+0x190;
    return FIELD(b1,0x21c,int)==2 && FIELD(b1,0x218,int)==1
        && FN(0x112fd50,int(*)(void*))(root) && !FN(0x112f6b0,int(*)(void*))(root);
}
static void trim_repaint(void)
{
    if (root) FN(0x1149880,void(*)(void*,uint64_t,uint64_t))(root,0,FIELD(root,0x28,uint64_t));
}
static int shifted(void)
{
    /* Pinned native binding at 0x166da40 links this exact BooleanProperty
     * to shiftIndicator. Read the controller property, not a delayed image. */
    void *controller=FIELD(root,0x108,void*);
    return controller && __atomic_load_n((unsigned char*)controller+0x2250+0x50,
                                         __ATOMIC_ACQUIRE)!=0;
}

static int label(void *p,char *out,size_t size)
{
    uint64_t text=0;
    note_label_string(p,&text,base+0x11bae00);
    const char *s=(const char*)(uintptr_t)text;
    size_t n=s ? strnlen(s,size) : size;
    int ok=n<size;
    if (ok) memcpy(out,s,n+1);
    FN(0xf0a290,void(*)(uint64_t*))(&text);
    return ok;
}

static int selection(void)
{
    char pad[16]={0}, sample[513]={0};
    if (!label((char*)root+0x1290,pad,sizeof(pad))
        || !label((char*)root+0x1438,sample,sizeof(sample))) {
        original_note=-1; selection_valid=0; return 0;
    }
    int number=(pad[1]>='0' && pad[1]<='9' && pad[2]>='0' && pad[2]<='9')
        ? (pad[1]-'0')*10+pad[2]-'0' : 0;
    int ok=pad[0]>='A' && pad[0]<='D' && strlen(pad)==3
        && number>=1 && number<=16 && sample[0] && strcmp(sample,"Sample Name");
    uint64_t generation=music_generation();
    if (strcmp(pad,selected_pad) || strcmp(sample,selected_sample) || selection_generation!=generation) {
        struct music_sample settings={0};
        music_get_sample(sample,&settings);
        original_note=ok ? settings.root : -1;
        memcpy(selected_pad,pad,strlen(pad)+1);
        memcpy(selected_sample,sample,strlen(sample)+1);
        selection_generation=generation;
        log_note("SELECTION_SETTINGS_RESTORED project_sample_key");
    }
    selection_valid=ok;
    return ok;
}

void note_native_observe_root(void *p)
{
    if (atomic_exchange(&destroyed,0)) forget();
    if (!p || FIELD(p,0,uintptr_t)!=base+SAMPLE_VPTR) return;
    if (root!=p) {
        forget(); root=p; slider=(char*)p+SEMI_OFFSET;
        atomic_store(&identity,(uintptr_t)p);
        log_note("TUNE_READY r12 project_sample_settings");
    }
}

int note_native_current(char *name,size_t size)
{
    if (atomic_exchange(&destroyed,0)) forget();
    if (!root || FIELD(root,0,uintptr_t)!=base+SAMPLE_VPTR || !selection()) return 0;
    size_t n=strlen(selected_sample);
    if (n>=size) return 0;
    memcpy(name,selected_sample,n+1); return 1;
}

int note_native_pad_index(unsigned*out){
    if(atomic_exchange(&destroyed,0))forget();
    char pad[16]={0};if(!out||!root||FIELD(root,0,uintptr_t)!=base+SAMPLE_VPTR||!label((char*)root+0x1290,pad,sizeof(pad)))return 0;
    if(strlen(pad)!=3||pad[0]<'A'||pad[0]>'H'||pad[1]<'0'||pad[1]>'1'||pad[2]<'0'||pad[2]>'9')return 0;
    unsigned number=(unsigned)(pad[1]-'0')*10+(unsigned)(pad[2]-'0');if(number<1||number>16)return 0;
    *out=(unsigned)(pad[0]-'A')*16+number-1;
    return *out<128;
}
void *note_native_context(void)
{
    if (atomic_exchange(&destroyed,0)) forget();
    /* Constructor 16737b8..7e8: CurrentThings at SampleScreen+148.
     * Consumed immediately by the UI Timer; no pointer retained. */
    return root && FIELD(root,0,uintptr_t)==base+SAMPLE_VPTR ? FIELD(root,0x148,void*) : NULL;
}
int note_native_request_poly(void)
{
    void *things=note_native_context();
    void *instrument=things?FIELD(things,0x668,void*):NULL;
    if (!instrument) return 0;
    if (FIELD(instrument,0x1690,int)==0) return 1;
    /* Native binding 166dd64: SamplePadControls+928 is the unsigned Play
     * property. 1671830 (the knob's float listener) calls this same command.
     * The widget's +170 is an intrusive observer list, NOT a JUCE Value. */
    void *controls=FIELD(root,0x178,void*);
    if (!controls) return 0;
    void *property=(char*)controls+0x928;
    if (!FIELD(property,0x88,void*) || !FIELD(property,0x78,uintptr_t)
        || !FIELD(property,0x80,uintptr_t) || !FIELD(property,0x38,void*)) return 0;
    const unsigned value=0;
    FN(0x1671650,void(*)(void*,const unsigned*))(property,&value);
    log_note("CHORDS_POLY_REQUEST native_unsigned_Play_command");
    return 2;
}

int note_native_sample_visible(void)
{
    return note_native_context() && FN(0x112fd50,int(*)(void*))(root)
        && !FN(0x112f6b0,int(*)(void*))(root);
}
void note_native_repaint_sample_region(unsigned x,unsigned y,unsigned w,unsigned h)
{
    if(root && !atomic_load(&destroyed) && FIELD(root,0,uintptr_t)==base+SAMPLE_VPTR)
        FN(0x1149880,void(*)(void*,uint64_t,uint64_t))(root,x|((uint64_t)y<<32),w|((uint64_t)h<<32));
}
void note_native_repaint_sample(void){if(root&&FIELD(root,0,uintptr_t)==base+SAMPLE_VPTR)
 FN(0x1149880,void(*)(void*,uint64_t,uint64_t))(root,0,FIELD(root,0x28,uint64_t));}
static void repaint(void)
{
    if (slider) FN(0x1149880,void(*)(void*,uint64_t,uint64_t))
        (slider,0,FIELD(slider,0x28,uint64_t));
}

int note_native_tick(void)
{
    int visible=showing();
    if (!root || !slider || FIELD(root,0,uintptr_t)!=base+SAMPLE_VPTR) {
        atomic_store(&active,0); music_publish(NULL,0); return 0;
    }
    int shift=shifted(), before=original_note, was_valid=selection_valid;
    atomic_store(&requested_shift,(unsigned)shift);
    atomic_store(&focused_k1,FIELD((void*)(base+0x723cde0),0,uintptr_t)
                              ==(uintptr_t)slider+0x1d8);
    {
        void *s=trim_slider();
        atomic_store(&trim_active,(unsigned)trim_showing());
        (void)s;
        /* A native knob movement can replace our focus before the next tick. */
        if ((!atomic_load(&trim_active) || shift || FIELD((void*)(base+0x723cde0),0,uintptr_t)) && atomic_load(&focused_trim)) { give_back_knob_focus(); trim_repaint(); }
        static int trim_shift=-1;
        if (atomic_load(&trim_active) && shift!=trim_shift) trim_repaint();
        trim_shift=shift;
    }
    selection();
    if (selection_valid) {
        struct music_sample settings;
        music_get_sample(selected_sample,&settings);
        double semi=FN(0x11234b0,double(*)(void*))(slider);
        if (isfinite(semi) && semi>= -96 && semi<=96) music_publish(&settings,(int)lround(semi));
        else music_publish(NULL,0);
    } else music_publish(NULL,0);
    if (shift!=previous_shift || original_note!=before || was_valid!=selection_valid) {
        previous_shift=shift; repaint();
    }
    atomic_store(&active,(unsigned)visible);
    return 1; /* Observe the bound selected sample while other screens show. */
}

/* Insert Pad Colour between native Mix Volume and Pan in the encoder cycle.
 * A physical K2 movement selects it directly. Shift hands K2 back to Kit Volume. */
/* The native knob focus (the pointer at base+0x723cde0, which the K panel
 * compares to paint its amber frame) is cleared while Pad Colour holds the
 * focus, so Kit Volume does not stay framed, and restored afterwards. A null
 * focus is the native state before the first encoder push on a page. */
static uintptr_t saved_knob_focus;
static void take_knob_focus(void)
{
    uintptr_t *focus=(uintptr_t*)(base+0x723cde0);
    saved_knob_focus=*focus; *focus=0;
    atomic_store(&focused_trim,1);
}
static void give_back_knob_focus(void)
{
    uintptr_t *focus=(uintptr_t*)(base+0x723cde0);
    if (atomic_exchange(&focused_trim,0) && *focus==0) *focus=saved_knob_focus;
    saved_knob_focus=0;
}
int note_native_button(void *event)
{
    unsigned code=FIELD(event,8,unsigned char),down=FIELD(event,9,unsigned char)!=0;
    if(code==0x6f && !down && colour_push_owned){colour_push_owned=0;return 1;}
    if (!root || FIELD(root,0,uintptr_t)!=base+SAMPLE_VPTR) return 0;
    if(code==0x31 && down && atomic_load(&focused_trim)) { give_back_knob_focus(); trim_repaint(); }
    if(code!=0x6f || !down || !trim_showing() || shifted()) return 0;
    if(atomic_load(&focused_trim)){give_back_knob_focus();trim_repaint();return 0;}
    void *volume=trim_slider();
    if(volume && FIELD((void*)(base+0x723cde0),0,uintptr_t)==(uintptr_t)volume+0x1d8){
        take_knob_focus();colour_push_owned=1;trim_repaint();return 1;
    }
    return 0;
}
int note_native_control(void *event)
{
    unsigned cc=FIELD(event,8,unsigned char), value=FIELD(event,9,unsigned char);
    if (root && FIELD(root,0,uintptr_t)==base+SAMPLE_VPTR && trim_showing() && !shifted()) {
        if(cc==0x10 || cc==0x12){if(atomic_load(&focused_trim))give_back_knob_focus();return 0;}
        if(cc!=0x11 && cc!=0x64)return 0;
        void *s=trim_slider();
        (void)s;
        if (cc==0x64 && !atomic_load(&focused_trim)) return 0;
        unsigned pad;
        if (!note_native_pad_index(&pad)) return 1;
        int current=music_pad_colour(pad),next;
        if (cc==0x11) { if(!atomic_load(&focused_trim))take_knob_focus(); next=(int)((value&127)*MUSIC_SEQUENCE_COLOURS/128); }
        else {
            int step=value>64 ? (int)value-128 : (int)value;
            next=current+step;
            if (next<0) next=0;
            if (next>=MUSIC_SEQUENCE_COLOURS) next=MUSIC_SEQUENCE_COLOURS-1;
        }
        if (next!=current && music_set_pad_colour(pad,(unsigned)next)) { trim_repaint(); log_note("PAD_COLOUR_CHANGED project_setting"); }
        return 1;
    }
    if (!showing() || !shifted()) return 0;
    if (cc!=0x10 && cc!=0x64) return 0;
    /* Encoder belongs to Original note only while the same K1 is focused. */
    if (cc==0x64 && FIELD((void*)(base+0x723cde0),0,uintptr_t)
        != (uintptr_t)slider+0x1d8) return 0;
    if (!selection() || !FN(0x10f98d0,int(*)(void*))(slider)) return 1;
    int next;
    if (cc==0x10) next=(int)((value&127)*13/128)-1;
    else {
        int step=value>64 ? (int)value-128 : (int)value;
        next=original_note+step;
        if (next< -1) next=-1;
        if (next>11) next=11;
    }
    if (next!=original_note) {
        if (music_set_sample(selected_sample,MUSIC_ROOT,next)) {
            original_note=next; repaint(); log_note("ROOT_CHANGED pitch_untouched project_setting");
        }
    }
    return 1;
}

static void colour(void *g,uint32_t c) { FN(0x102d5e0,void(*)(void*,uint32_t))(g,c); }
static void rect(void *g,int x,int y,int w,int h)
{ FN(0x102e000,void(*)(void*,int,int,int,int))(g,x,y,w,h); }
static void text(void *g,const char *s,int x,int y,int w,int h,float height)
{
    uint64_t string=0,font=0;
    FN(0xf535c0,void(*)(uint64_t*,const char*))(&string,s);
    FN(0x1082810,void(*)(uint64_t*,int,float))(&font,0,height);
    FN(0x102dca0,void(*)(void*,const uint64_t*))(g,&font);
    FN(0x10817b0,void(*)(void*,const uint64_t*,int,int,int,int,int,int,float))
        (g,&string,x,y,w,h,0x24,1,0.75f);
    FN(0x1070d60,void(*)(uint64_t*))(&font);
    FN(0xf0a290,void(*)(uint64_t*))(&string);
}
/* Shift badge as the native K panel draws it (values_ui.S): a 10x12 frame
 * with "S", white, red while Shift is held. bx = column*106+94, top = y. */
static void shift_badge(void *g,int bx,int top,int on)
{
    colour(g,on ? 0xffff4747 : 0xffffffff);
    rect(g,bx,top,10,1); rect(g,bx,top+11,10,1); rect(g,bx,top,1,12); rect(g,bx+9,top,1,12);
    text(g,"S",bx+1,top+1,8,10,10.0f);
}

/* Mix K2 without Shift: Pad Colour of the selected pad (Stock = native). */
static void pad_colour_cell(void *g,int ox,int oy,int focused);
/* Pad Colour, K2 block at (ox,oy): S marks the native Kit Volume alternative. */
static void pad_colour_cell(void *g,int ox,int oy,int focused)
{
    unsigned pad; int valid=note_native_pad_index(&pad);
    int value=valid ? music_pad_colour(pad) : 0;
    uint32_t ink=!valid ? 0xff707070 : focused ? 0xffffc247 : 0xffffffff;
    colour(g,0xff000000); rect(g,ox,oy,106,38);
    colour(g,ink); text(g,"Pad Colour",ox+7,oy+2,82,12,12.0f);
    shift_badge(g,ox+94,oy+2,0);
    uint32_t swatch=0xff000000|music_sequence_colours[value];
    if (value==0) swatch=0xff9a9a9a;
    colour(g,swatch); rect(g,ox+6,oy+18,10,10);
    colour(g,ink); text(g,valid ? music_sequence_colour_names[value] : "--",ox+18,oy+14,84,18,16.0f);
    for (int i=0;i<MUSIC_SEQUENCE_COLOURS;++i) {
        int x=i*98/MUSIC_SEQUENCE_COLOURS, next=(i+1)*98/MUSIC_SEQUENCE_COLOURS;
        colour(g,i==value ? ink : 0xff3b3b3b);
        rect(g,ox+4+x,oy+35,next-x-2,2);
    }
    if (focused && valid) { colour(g,ink); rect(g,ox,oy,106,2); rect(g,ox,oy,2,32); rect(g,ox+104,oy,2,32); }
}
/* Sample screen paint: K2 has no native child without Shift on Mix. */
void note_native_sample_overlay(void *p,void *g)
{
    if (!p || p!=root || !trim_showing() || shifted()) return;
    pad_colour_cell(g,106,198,(int)atomic_load(&focused_trim));
}
int note_native_footer(void *p,void *g)
{
    /* Called only from the guarded mod footer renderer on the UI thread. */
    if (FIELD(p,0,uintptr_t)!=base+SLIDER_VPTR
        || FIELD(p,0x20,int)!=0 || FIELD(p,0x24,int)!=198
        || FIELD(p,0x28,int)!=106 || FIELD(p,0x2c,int)!=38) return 0;
    void *r=p;
    for (unsigned i=0;i<32 && r;++i) {
        if (FIELD(r,0,uintptr_t)==base+SAMPLE_VPTR) break;
        r=FIELD(r,0x18,void*);
    }
    if (r && FIELD(r,0,uintptr_t)==base+SAMPLE_VPTR) {
        note_native_observe_root(r);
    }
    if (!r || FIELD(r,0,uintptr_t)!=base+SAMPLE_VPTR
        || (uintptr_t)p-(uintptr_t)r!=SEMI_OFFSET) return 0;
    note_native_observe_root(r);
    if (!note_native_tick()) return 0;
    int shift=shifted();
    double semi=FN(0x11234b0,double(*)(void*))(p);
    if (!isfinite(semi) || semi< -96 || semi>96) return 0;
    int enabled=FN(0x10f98d0,int(*)(void*))(p) && selection_valid;
    int focused=FIELD((void*)(base+0x723cde0),0,uintptr_t)==(uintptr_t)p+0x1d8;
    uint32_t ink=!enabled ? 0xff707070 : focused ? 0xffffc247 : 0xffffffff;
    char value[40];
    if (shift) snprintf(value,sizeof(value),"%s",original_note<0 ? "--" : notes[original_note]);
    else if (original_note<0) snprintf(value,sizeof(value),"%+.0f",semi);
    else {
        int pitch=(original_note+(int)lround(semi))%12;
        if (pitch<0) pitch+=12;
        snprintf(value,sizeof(value),"%+.0f %s",semi,notes[pitch]);
    }
    colour(g,0xff000000); rect(g,0,0,106,38);
    colour(g,ink); text(g,shift ? "Orig Note" : "Semi Tune",7,2,82,12,12.0f);
    text(g,value,4,14,98,18,16.0f);
    shift_badge(g,94,2,shift);
    colour(g,0xff3b3b3b); rect(g,4,35,98,2);
    colour(g,ink);
    if (shift) {
        for (int i=0;i<13;++i) {
            int x=i*98/13, next=(i+1)*98/13;
            colour(g,i==original_note+1 ? ink : 0xff3b3b3b);
            rect(g,4+x,35,next-x-2,2);
        }
    } else {
        /* Native pinned Semi Tune range at 0x4661ee0 is -24..24. */
        double fraction=(semi+24.0)/48.0;
        if (fraction<0) fraction=0;
        if (fraction>1) fraction=1;
        int width=(int)(fraction*98.0);
        if (width) rect(g,4,35,width,2);
    }
    if (focused && enabled) {
        colour(g,ink); rect(g,0,0,106,2); rect(g,0,0,2,32); rect(g,104,0,2,32);
    }
    return 1;
}
