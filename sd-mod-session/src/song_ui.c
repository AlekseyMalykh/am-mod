#define _GNU_SOURCE
#include "song_ui.h"
#ifdef MPC_MIXER_LIVE
#include "mixer_stems.h"
#pragma weak mixer_stems_arm
#pragma weak mixer_stems_available
#pragma weak mixer_stems_disarm
#endif
#include "phone_events.h"
#include "music_settings.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

/* Pinned MPC Sample 1.3.0.12 addresses (virtual image offsets).
 * SongScreen: vptr 0x6e24a00; paintOverChildren +0xd8 is the shared no-op,
 * destructor/deleting destructor are its own. SongScreen+0x110 is the song
 * page controller: +0x260 sequencer, +0x1dc pad-selected sequence,
 * +0x292/+0x294 the "follow new selection" pair the stock Insert uses.
 * SongScreen+0x798 is the step list; 0x3b71e80 returns its selected row.
 * Sequencer: +0xb38 current song (<0: +0x1270), songs +0x1260/+0x126c.
 * Song: steps [+0x30,+0x38), 8 bytes each: int sequence, int repeats.
 * Stock edit builders (all queue an undoable command):
 *   0x19a0b50 editor context (8 bytes: *ctx = sequencer)
 *   0x19a2d20 Insert(ctx, song, long step, int seq, int repeats)
 *   0x19a1880 Delete(ctx, song, long step)
 *   0x19a29e0 SetSequence(ctx, song, long step, int seq)
 *   0x19a20d0 SetRepeats(ctx, song, long step, int repeats)
 * Selected step: Property<int> at controller+0x168 (value +0x1c = +0x184);
 *   0x14e19c0(property,int) sets it and notifies the list (stock 0x1575a04).
 * Song length: 0x29ba240(song, sequencer) = sum of repeats x bars, 0x2909be0(sequencer, seq)+0x200 bars,
 * stock limit total <= 0x3137 (check 0x15756a0). */
#define SONG_VPTR 0x6e24a00
#define SLOT_PAINT 0xd8
#define NATIVE_NOOP 0x10e2e10
#define NATIVE_DTOR 0x16a6540
#define NATIVE_DELETE 0x16a6d90
/* SongExportScreen (Song B1): same three slots, only for pad colours. */
#define EXPORT_VPTR 0x6e24618
#define EXPORT_DTOR 0x169f960
#define EXPORT_DELETE 0x169fef0
#define SONG_BAR_LIMIT 0x3137
#define REPEATS_MAX 99
#define FIELD(p,off,type) (*(type *)((char *)(p)+(off)))
#define FN(off,type) ((type)(base+(off)))

enum { B1=0x0c, B2=0x0d, B3=0x0e, PLUS=0x36, MINUS=0x37, K1=0x10, DATA_ENCODER=0x64 };

static uintptr_t base;
static void *screen;                     /* UI thread only */
static _Atomic unsigned shown;           /* producer reads */
static _Atomic unsigned owned_buttons;   /* bit per claimed button down */
static _Atomic int selected_sequence=-1; /* published by the UI Timer for the RGB layer */
static void *export_screen;              /* UI thread only */
/* Owner of the native Stems naming modal (JUCE UI thread only).
 * Its paintOverChildren runs after the name editor; ExportScreen painting
 * runs behind the modal and cannot display this footer reliably. */
static void *stems_modal_root;
static unsigned stems_modal_scan_logs;
static _Atomic unsigned pads_shown;      /* Song or Song Export visible: sequence pad colours */
static _Atomic unsigned export_shown;    /* Song Export window visible */
/* Local Export chooser on SongScreen. Main DATA encoder navigates;
 * B1=Cancel, B3=Do It. Native export starts only on confirmation. */
static _Atomic unsigned export_choice; /* 0 Mixdown, 1 Sequence, 2 Stems dry, 3 Stems with FX */
static _Atomic unsigned export_shift; /* Shift still labels Duplicate on Song steps */
static _Atomic unsigned export_paint_seen; /* vtable callback actually reached */
static unsigned export_shift_seen;       /* UI thread, to request the repaint */
/* Mix9: the choice screen is painted on SongScreen BEFORE any native B1.
 * The stock question is entered only after a confirmed choice, when we can
 * route to the pinned native actions. UI-thread-only transition state:
 * 0 idle, 1 waiting for the old question, 2 waiting for native ExportScreen. */
static _Atomic unsigned export_menu_active;
static unsigned export_route_stage,export_route_choice;
static uint64_t export_route_since;
static _Alignas(8) unsigned char export_route_template[16];
/* 0 none, 1 Mixdown naming, 2 Dry Stems naming, 3 Wet Stems naming. Both share the existing
 * TextEditor + pad keyboard; Sequence has no stock name dialog. */
static _Atomic unsigned export_name_kind;
/* Mixdown and Stems use the same native name editor. Its Shift/B2 mean
 * case/Erase. 0=idle, 1=opening, 2=open, 3=Do It. */
static _Atomic unsigned stems_name_stage;
/* The stock naming modal is separate from SongScreen. K1 footer is painted
 * over the modal itself; keep it until TextEditor destruction. */
static _Atomic unsigned stems_modal_visible;
static _Atomic unsigned stems_modal_pads;
static uint64_t stems_name_requested_ms; /* UI thread only */
void song_ui_shift_event(int down){atomic_store(&export_shift,down!=0);}
/* Compatibility with the native Shift input hook: Stems no longer uses it. */
void song_ui_stems_shift_up_forwarded(void){}
static void (*forward_native)(void*);
void song_ui_set_forward(void(*forward)(void*)){forward_native=forward;}
static int knob_last=-1;                 /* K1 relative baseline */
static int knob_accum;
static char toast[48];
static uint64_t toast_until;
static unsigned log_count;
static int pending_row=-1,pending_count;   /* select after the queued command lands */
static uint64_t pending_until;
static int last_row=-1;

static uint64_t now_ms(void)
{ struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint64_t)t.tv_sec*1000u+(uint64_t)t.tv_nsec/1000000u; }
static void report(const char*m){ if(log_count++<64)fprintf(stderr,"SONG_UI %s\n",m); }
static int visible(void *p)
{ return p && FN(0x112fd50,int(*)(void*))(p) && !FN(0x112f6b0,int(*)(void*))(p); }
static void repaint(void *p)
{ if (p) FN(0x1149880,void(*)(void*,uint64_t,uint64_t))(p,0,FIELD(p,0x28,uint64_t)); }
static unsigned button_bit(unsigned code)
{ return code==B2?1u:code==B3?2u:code==PLUS?4u:code==MINUS?8u:code==B1?16u:0u; }

void *song_ui_stems_name_editor(void)
{
    if(atomic_load(&stems_name_stage)!=1u||!export_screen||
       FIELD(export_screen,0,uintptr_t)!=base+EXPORT_VPTR)return NULL;
    /* SongExportScreen dtor at 169fab4 proves the embedded TextEditor. */
    void *editor=(char*)export_screen+0xa38;
    return FIELD(editor,0,uintptr_t)==base+0x6e18e20&&visible(editor)?editor:NULL;
}
int song_ui_stems_name_pending(void){return atomic_load(&stems_name_stage)==1u;}
int song_ui_stems_modal_visible(void){return atomic_load(&stems_modal_visible)!=0u;}
/* Called from the existing K1 handler on the JUCE/UI thread. */
void song_ui_stems_input_mode(int pads)
{
    if(!atomic_load(&stems_modal_visible))return;
    unsigned next=pads!=0;
    if(atomic_exchange(&stems_modal_pads,next)!=next && stems_modal_root)
        repaint(stems_modal_root);
}
int song_ui_stems_name_active(void){unsigned s=atomic_load(&stems_name_stage);return s==2u||s==3u;}
void song_ui_stems_name_opened(void)
{
    if(atomic_load(&stems_name_stage)!=1u)return;
    atomic_store(&stems_name_stage,2u);
    atomic_store(&stems_modal_pads,0u);
    atomic_store(&stems_modal_visible,1u);
    /* The TextEditor painter will attach a guarded modal-root footer. */
    stems_modal_root=NULL;
    stems_modal_scan_logs=0;
    /* A B2-up may have reached the native modal before the input FIFO did.
     * It must not leave an owned bit which would swallow later stock Mixdown. */
    atomic_fetch_and(&owned_buttons,~button_bit(B2));
    fprintf(stderr,"SONG_EXPORT NAME_EDITOR_BOUND kind=%u (K1 Encoder/Pads)\n",
        atomic_load(&export_name_kind));
    if(atomic_load(&export_name_kind)>=2u)
        fprintf(stderr,"STEMS NAME_EDITOR_BOUND (K1 Encoder/Pads)\n");
}
void song_ui_stems_name_button(unsigned code,int down)
{
    if(!down||atomic_load(&stems_name_stage)!=2u)return;
    if(code==0x0c){ /* Cancel: never arm a later ordinary Mixdown. */
#ifdef MPC_MIXER_LIVE
        if(mixer_stems_disarm)mixer_stems_disarm();
#endif
        atomic_store(&stems_name_stage,0u);
        fprintf(stderr,"SONG_EXPORT NAME_CANCELLED kind=%u\n",atomic_load(&export_name_kind));
    }else if(code==0x0e){ /* Arm only after the user confirms the name. */
#ifdef MPC_MIXER_LIVE
        if(atomic_load(&export_name_kind)>=2u&&mixer_stems_arm)mixer_stems_arm(atomic_load(&export_name_kind)==3u);
#endif
        atomic_store(&stems_name_stage,3u);
        fprintf(stderr,"SONG_EXPORT NAME_CONFIRMED kind=%u (native dispatch)\n",
            atomic_load(&export_name_kind));
        if(atomic_load(&export_name_kind)>=2u)
            fprintf(stderr,"STEMS NAME_CONFIRMED (await native dispatch)\n");
    }
}
void song_ui_stems_name_closed(void)
{
    unsigned prev=atomic_exchange(&stems_name_stage,0u);
    atomic_store(&stems_modal_visible,0u);
    atomic_store(&stems_modal_pads,0u);
    /* Do not dereference a modal being destroyed. The class hook stays
     * installed, but paints nothing for unrelated native dialogs. */
    stems_modal_root=NULL;
#ifdef MPC_MIXER_LIVE
    if(prev==1u||prev==2u){if(mixer_stems_disarm)mixer_stems_disarm();}
#endif
    if(prev)fprintf(stderr,"SONG_EXPORT NAME_CLOSED kind=%u stage=%u\n",
        atomic_load(&export_name_kind),prev);
    atomic_store(&export_name_kind,0u);
}

void song_ui_init(uintptr_t b){ base=b; }
int song_ui_active(void){ return atomic_load(&shown)!=0; }
int song_ui_selected_sequence(void){ return atomic_load(&selected_sequence); }
int song_ui_pads_active(void){ return atomic_load(&pads_shown)!=0; }

int song_ui_owns(unsigned kind,unsigned code,int shift,int down)
{
    if(kind==PHONE_CONTROLLER){
        if(atomic_load(&shown) && code==K1) return 1;
        /* The main DATA encoder (not K1) scrolls Export choices only when
         * our menu is active. The native name dialog keeps its own encoder. */
        return code==DATA_ENCODER && atomic_load(&export_menu_active);
    }
    if(kind!=PHONE_BUTTON) return 0;
    unsigned bit=button_bit(code);
    if(!bit) return 0;
    if(!down) return (atomic_load(&owned_buttons)&bit)!=0;
    /* Bounded diagnostic of the exact context seen by the input thread.
     * It is intentionally passive: never claim/export based on a guess. */
    if(code==B2){
        static _Atomic unsigned b2_reports;
        if(atomic_fetch_add(&b2_reports,1)<32)
            fprintf(stderr,"SONG_UI B2_ROUTE shift=%d song=%u export=%u captured=%u held=%u\n",
                shift,atomic_load(&shown),atomic_load(&export_shown),
                atomic_load(&export_paint_seen),atomic_load(&export_shift));
    }
    /* Export chooser is on SongScreen, so the native B1 popup is NOT
     * opened yet. Capture B1=Cancel, B2=no action, B3=Confirm. */
    if(atomic_load(&export_menu_active) &&
       (code==B1||code==B2||code==B3)){
        atomic_fetch_or(&owned_buttons,bit);return 1;
    }
    if(code==B1 && atomic_load(&shown) && !shift &&
       !atomic_load(&stems_name_stage) && !atomic_load(&stems_modal_visible)){
        atomic_fetch_or(&owned_buttons,bit);return 1;
    }
    if(code==B1) return 0; /* Shift+B1 remains a stock action. */
    if(!atomic_load(&shown)||!shift) return (atomic_load(&owned_buttons)&bit)!=0;
    atomic_fetch_or(&owned_buttons,bit);
    return 1;
}

/* ---- native model access (UI thread) ---- */
struct song_ref { void *controller,*sequencer,*song; int row,count; };
static int song_ref(struct song_ref *r)
{
    memset(r,0,sizeof(*r));
    if(!screen) return 0;
    r->controller=FIELD(screen,0x110,void*);
    if(!r->controller) return 0;
    r->sequencer=FIELD(r->controller,0x260,void*);
    if(!r->sequencer) return 0;
    int index=FIELD(r->sequencer,0xb38,int);
    if(index<0) r->song=FIELD(r->sequencer,0x1270,void*);
    else if((unsigned)index<FIELD(r->sequencer,0x126c,unsigned))
        r->song=FIELD(r->sequencer,0x1260,void**)[index];
    if(!r->song) return 0;
    char *begin=FIELD(r->song,0x30,char*),*end=FIELD(r->song,0x38,char*);
    if(!begin||end<begin||(end-begin)%8) return 0;
    r->count=(int)((end-begin)/8);
    r->row=FN(0x3b71e80,int(*)(void*))((char*)screen+0x798);
    return 1;
}
static int step_at(const struct song_ref *r,int i,int *seq,int *repeats)
{
    if(i<0||i>=r->count) return 0;
    const int *s=(const int*)(FIELD(r->song,0x30,char*)+8*i);
    *seq=s[0]; *repeats=s[1];
    return 1;
}
static int sequence_bars(const struct song_ref *r,int seq)
{
    void *sequence=FN(0x2909be0,void*(*)(void*,int))(r->sequencer,seq);
    return sequence?FIELD(sequence,0x200,int):0;
}
static int fits(const struct song_ref *r,int extra_bars)
{ return FN(0x29ba240,int(*)(void*,void*))(r->song,r->sequencer)+extra_bars<=SONG_BAR_LIMIT; }
static void say(const char *m)
{ snprintf(toast,sizeof(toast),"%s",m); toast_until=now_ms()+1500; repaint(screen); report(m); }

static void trace(const char*what,const struct song_ref*r,int a,int b)
{ char m[96]; snprintf(m,sizeof(m),"%s row=%d count=%d a=%d b=%d",what,r->row,r->count,a,b); report(m); }
static void set_repeats(const struct song_ref *r,int step,int repeats)
{
    void *ctx; FN(0x19a0b50,void(*)(void**,void*))(&ctx,r->sequencer);
    FN(0x19a20d0,void(*)(void**,void*,long,int))(&ctx,r->song,(long)step,repeats);
}
static void insert_step(const struct song_ref *r,int step,int seq,int repeats)
{
    /* Same selection-follow handshake as the stock Insert (0x15754e0). */
    unsigned char follow=FIELD(r->controller,0x292,unsigned char);
    FIELD(r->controller,0x292,unsigned char)=1;
    FIELD(r->controller,0x294,int)=step;
    void *ctx; FN(0x19a0b50,void(*)(void**,void*))(&ctx,r->sequencer);
    FN(0x19a2d20,void(*)(void**,void*,long,int,int))(&ctx,r->song,(long)step,seq,repeats);
    FIELD(r->controller,0x292,unsigned char)=follow;
    /* Lab: the list fell back to row 0 after our insert. Select the new
     * step explicitly once the queued command has changed the song. */
    pending_row=step; pending_count=r->count+1; pending_until=now_ms()+1500;
}
static void delete_step(const struct song_ref *r,int step)
{
    void *ctx; FN(0x19a0b50,void(*)(void**,void*))(&ctx,r->sequencer);
    FN(0x19a1880,void(*)(void**,void*,long))(&ctx,r->song,(long)step);
}
static void set_sequence(const struct song_ref *r,int step,int seq)
{
    void *ctx; FN(0x19a0b50,void(*)(void**,void*))(&ctx,r->sequencer);
    FN(0x19a29e0,void(*)(void**,void*,long,int))(&ctx,r->song,(long)step,seq);
}

static int change_repeats(int delta)
{
    struct song_ref r; int seq,rep;
    if(!song_ref(&r)||!step_at(&r,r.row,&seq,&rep)) return 0;
    int next=rep+delta;
    if(next<1) next=1;
    if(next>REPEATS_MAX) next=REPEATS_MAX;
    if(next==rep) return 1;
    if(next>rep&&!fits(&r,sequence_bars(&r,seq)*(next-rep))){ say("Song would be too long"); return 1; }
    trace("REPEATS",&r,rep,next);
    set_repeats(&r,r.row,next);
    char m[32]; snprintf(m,sizeof(m),"Repeats x%d",next); say(m);
    return 1;
}
static int replace_step(void)
{
    struct song_ref r; int seq,rep;
    if(!song_ref(&r)||!step_at(&r,r.row,&seq,&rep)) return 0;
    int chosen=FIELD(r.controller,0x1dc,int);
    if(chosen<0||chosen==seq) return 1;
    int diff=(sequence_bars(&r,chosen)-sequence_bars(&r,seq))*rep;
    if(diff>0&&!fits(&r,diff)){ say("Song would be too long"); return 1; }
    trace("REPLACE",&r,seq,chosen);
    set_sequence(&r,r.row,chosen);
    say("Step replaced");
    return 1;
}
static int duplicate_step(void)
{
    struct song_ref r; int seq,rep;
    if(!song_ref(&r)||!step_at(&r,r.row,&seq,&rep)) return 0;
    trace("DUPLICATE_BEGIN",&r,seq,rep);
    if(!fits(&r,sequence_bars(&r,seq)*rep)){ say("Song would be too long"); return 1; }
    trace("DUPLICATE_FITS",&r,seq,rep);
    insert_step(&r,r.row+1,seq,rep);
    trace("DUPLICATE_INSERTED",&r,seq,rep);
    say("Step duplicated");
    return 1;
}
static int move_step(int direction)
{
    struct song_ref r; int seq,rep;
    if(!song_ref(&r)||!step_at(&r,r.row,&seq,&rep)) return 0;
    int target=r.row+direction;
    if(target<0||target>=r.count) return 1;
    /* Two stock commands (Undo restores in two steps). */
    trace("MOVE",&r,seq,target);
    delete_step(&r,r.row);
    insert_step(&r,target,seq,rep);
    pending_count=r.count;   /* delete + insert: same length */
    say(direction<0?"Step moved up":"Step moved down");
    return 1;
}

int song_ui_control(void *e)
{
    unsigned cc=FIELD(e,8,unsigned char); int value=FIELD(e,9,unsigned char);
    if(cc==DATA_ENCODER && atomic_load(&export_menu_active)){
        /* Native DATA encoder is relative: 1..63=clockwise,
         * 65..127=counterclockwise (127 is -1). Clamp at each end. */
        int delta=value>64?value-128:value;
        if(delta && delta!=64){
            int previous=(int)atomic_load(&export_choice);
            int next=previous+delta;
            if(next<0) next=0;
            if(next>3) next=3;
            if(next!=previous){
                atomic_store(&export_choice,(unsigned)next);
                repaint(screen);
                fprintf(stderr,"SONG_EXPORT ENCODER selected=%d delta=%d\n",next,delta);
            }
        }
        return 1;
    }
    if(cc!=K1||!screen||!atomic_load(&shown)) return 0;
    /* K1 is an absolute knob: count movement, 4 positions per repeat.
     * The baseline resets when the Song screen appears, never on a timer. */
    if(knob_last<0){ knob_last=value; knob_accum=0; return 1; }
    knob_accum+=value-knob_last; knob_last=value;
    int steps=knob_accum/4; knob_accum-=steps*4;
    if(steps) change_repeats(steps);
    return 1;
}
int song_ui_button(void *e)
{
    unsigned code=FIELD(e,8,unsigned char),down=FIELD(e,9,unsigned char)!=0,bit=button_bit(code);
    if(!bit) return 0;
    if(!down){ atomic_fetch_and(&owned_buttons,~bit); return 1; }
    if(atomic_load(&export_menu_active)){
        if(code==B1){
            atomic_store(&export_menu_active,0u);
            repaint(screen);
            fprintf(stderr,"SONG_EXPORT MENU_CANCEL\n");
            return 1;
        }
        if(code==B2)return 1;
        if(code==B3){
            struct song_ref song;
            if(!song_ref(&song)||!song.count)return 1;
            unsigned selected=atomic_load(&export_choice);
#ifdef MPC_MIXER_LIVE
            if(selected>=2u && (!mixer_stems_available || !mixer_stems_available() || !mixer_stems_arm)){
                fprintf(stderr,"SONG_EXPORT STEMS_UNAVAILABLE\n");return 1;
            }
#else
            if(selected>=2u){fprintf(stderr,"SONG_EXPORT STEMS_UNAVAILABLE\n");return 1;}
#endif
            if(!forward_native)return 1;
            /* Exit our custom menu BEFORE entering the native workflow.
             * Sequence will use its original native handler (no naming modal). */
            atomic_store(&export_menu_active,0u);
            export_route_choice=selected;
            export_route_stage=1u;
            export_route_since=now_ms();
            memcpy(export_route_template,e,sizeof(export_route_template));
            /* Pin both edges. Unlike Mix7, native B1 is NOT sent on Export. */
            _Alignas(8) unsigned char ev[16];
            memcpy(ev,e,sizeof(ev));
            ev[8]=B1;ev[9]=1;forward_native(ev);
            ev[9]=0;forward_native(ev);
            fprintf(stderr,"SONG_EXPORT MENU_CONFIRMED selected=%u route=1\n",selected);
            return 1;
        }
    }
    if(code==B1 && atomic_load(&shown) &&
       !atomic_load(&stems_name_stage) && !atomic_load(&stems_modal_visible)){
        /* Song B1 opens our menu ONLY. No native dialog = no initial flash. */
        if(export_route_stage)return 1;
        atomic_store(&export_choice,0u);
        atomic_store(&export_menu_active,1u);
        repaint(screen);
        fprintf(stderr,"SONG_EXPORT MENU_OPEN\n");
        return 1;
    }
    if(!screen||!atomic_load(&shown)) return 1;
    if(code==B3) replace_step();
    else if(code==B2) duplicate_step();
    else move_step(code==PLUS?1:-1);
    return 1;
}

/* ---- paint / lifetime ---- */
static void colour(void *g,uint32_t c){ FN(0x102d5e0,void(*)(void*,uint32_t))(g,c); }
static void rect(void *g,int x,int y,int w,int h){ FN(0x102e000,void(*)(void*,int,int,int,int))(g,x,y,w,h); }
static void text_at(void *g,const char *s,int x,int y,int w,int h,float height,int align);
static void text(void *g,const char *s,int x,int y,int w,int h,float height){ text_at(g,s,x,y,w,h,height,0x24); }
static void text_at(void *g,const char *s,int x,int y,int w,int h,float height,int align)
{
    uint64_t string=0,font=0;
    FN(0xf535c0,void(*)(uint64_t*,const char*))(&string,s);
    FN(0x1082810,void(*)(uint64_t*,int,float))(&font,0,height);
    FN(0x102dca0,void(*)(void*,const uint64_t*))(g,&font);
    FN(0x10817b0,void(*)(void*,const uint64_t*,int,int,int,int,int,int,float))(g,&string,x,y,w,h,align,1,0.70f);
    FN(0x1070d60,void(*)(uint64_t*))(&font);
    FN(0xf0a290,void(*)(uint64_t*))(&string);
}
/* One-time probe (lab/device log): locate the step list rows so the sequence
 * colour can be drawn in each row. Reads only Component parent/bounds/child
 * array; every child pointer must point back to its parent. */
static int plausible(const void *p){ uintptr_t v=(uintptr_t)p; return v>0x10000&&!(v&7)&&v<((uintptr_t)1<<48); }
static int child_offset=-1;
static int children(void *c,void ***list)
{
    if(child_offset<0||!plausible(c)) return 0;
    void **data=FIELD(c,child_offset,void**); int n=FIELD(c,child_offset+12,int),cap=FIELD(c,child_offset+8,int);
    if(n<=0||n>64||cap<n||!plausible(data)) return 0;
    for(int i=0;i<n;++i) if(!plausible(data[i])||FIELD(data[i],0x18,void*)!=c) return 0;
    *list=data; return n;
}
static void probe_tree(void *c,int depth,int x0,int y0,unsigned *lines)
{
    void **list; int n=children(c,&list);
    for(int i=0;i<n&&*lines<90;++i){
        void *k=list[i]; int x=x0+FIELD(k,0x20,int),y=y0+FIELD(k,0x24,int);
        char m[128]; snprintf(m,sizeof(m),"PROBE d=%d off=%lx vptr=%lx xy=%d,%d wh=%d,%d vis=%d",depth,
            (unsigned long)((char*)k-(char*)screen),(unsigned long)(FIELD(k,0,uintptr_t)-base),x,y,
            FIELD(k,0x28,int),FIELD(k,0x2c,int),FN(0x112fd50,int(*)(void*))(k));
        fprintf(stderr,"SONG_UI %s\n",m); ++*lines;
        if(depth<6) probe_tree(k,depth+1,x,y,lines);
    }
}
static void probe(void *p)
{
    static int done; if(done) return; done=1;
    const int offsets[]={0x40,0x30,0x38,0x48,0x50,0x58};
    for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);++i){
        child_offset=offsets[i]; void **list;
        if(children(p,&list)){ fprintf(stderr,"SONG_UI PROBE child_offset=0x%x list_off=0x798\n",child_offset);
            unsigned lines=0; probe_tree(p,0,0,0,&lines); return; }
    }
    child_offset=-1; fprintf(stderr,"SONG_UI PROBE no child array found\n");
}
/* Step list rows (device probe 06.11): SongScreen+0x798 list -> ... ->
 * viewport content -> row components (vptr 0x6d84cf0), 320x33 each, row y
 * inside the content = index*33; the viewport shows y 38..236. */
#define ROW_VPTR 0x6d84cf0
#define ROW_HEIGHT 33
#define LIST_TOP 38
#define LIST_BOTTOM 236
struct row_hit { void *row; int y; };
static unsigned find_rows(void *c,int y0,int depth,struct row_hit *out,unsigned n)
{
    void **list; int k=children(c,&list);
    for(int i=0;i<k&&n<16;++i){
        void *x=list[i]; int y=y0+FIELD(x,0x24,int);
        if(FIELD(x,0,uintptr_t)==base+ROW_VPTR){ out[n].row=x; out[n].y=y; ++n; }
        else if(depth<6) n=find_rows(x,y,depth+1,out,n);
    }
    return n;
}
static void paint_rows(void *p,void *g,const struct song_ref *r)
{
    if(child_offset<0||!music_pad_lighting_mode()) return;
    void *listbox=(char*)p+0x798;
    struct row_hit rows[16]; unsigned n=find_rows(listbox,FIELD(listbox,0x24,int),0,rows,0);
    for(unsigned i=0;i<n;++i){
        void *row=rows[i].row; int ry=FIELD(row,0x24,int);
        if(ry<0||ry%ROW_HEIGHT||!FN(0x112fd50,int(*)(void*))(row)) continue;
        int index=ry/ROW_HEIGHT,seq,rep;
        if(!step_at(r,index,&seq,&rep)) continue;
        int top=rows[i].y,bottom=top+ROW_HEIGHT;
        if(top<LIST_TOP) top=LIST_TOP;
        if(bottom>LIST_BOTTOM) bottom=LIST_BOTTOM;
        if(bottom-top<4) continue;
        uint32_t rgb=0xff000000u|music_identity_rgb((unsigned)music_sequence_colour((unsigned)seq));
        /* Colour bar on every row; the selected row keeps the stock highlight. */
        colour(g,rgb); rect(g,0,top+3,4,bottom-top-6);
        if(index==r->row||rows[i].y<LIST_TOP||rows[i].y+ROW_HEIGHT>LIST_BOTTOM) continue;
        char name[16]; snprintf(name,sizeof(name),"Seq %c%02d",'A'+seq/16,seq%16+1);
        colour(g,0xff000000); rect(g,8,rows[i].y+4,100,ROW_HEIGHT-8);
        colour(g,rgb); text_at(g,name,25,rows[i].y,90,ROW_HEIGHT,15.0f,0x21); /* left, v-centred */
        if(rep>1){
            char bars[24]; snprintf(bars,sizeof(bars),"%d bars x%d",sequence_bars(r,seq),rep);
            colour(g,0xff000000); rect(g,200,rows[i].y+4,100,ROW_HEIGHT-8);
            colour(g,0xffffffff); text_at(g,bars,200,rows[i].y,96,ROW_HEIGHT,15.0f,0x22); /* right, v-centred */
        }
    }
}
static void paint(void *p,void *g)
{
    if(FIELD(p,0,uintptr_t)!=base+SONG_VPTR) return;
    screen=p;
    { struct song_ref pr; if(song_ref(&pr)&&pr.count>=1) probe(p); } /* rows exist only once the song has steps */
    struct song_ref r; int seq,rep;
    char line[128];
    if(atomic_load(&export_menu_active)){
        /* Paint after SongScreen children. Cover all of the song list/buttons;
         * the old native Cancel/Mixdown/Sequence modal was never entered. */
        const unsigned selected=atomic_load(&export_choice)%4u;
        int can_export=song_ref(&r)&&r.count>0;
        const char *items[]={"Mixdown","Sequence","Stems without FX","Stems with FX"};
        const char *detail[]={"One stereo WAV","Combine into a sequence",
                              "Separate WAVs without mixer FX","Separate WAVs with mixer FX"};
        colour(g,0xff000000u);rect(g,0,0,320,240);
        colour(g,0xffffffffu);text(g,"Back",0,1,106,23,14.0f);
        colour(g,can_export?0xffffc247u:0xff777777u);text(g,"Export",213,1,107,23,14.0f);
        colour(g,0xff252525u);rect(g,4,25,312,1);
        colour(g,0xffffffffu);text_at(g,"Song Export",14,31,292,28,20.0f,0x21);
        for(unsigned i=0;i<4;++i){
            int y=67+(int)i*31;
            int current=i==selected;
            colour(g,current?0xff302412u:0xff161616u);rect(g,12,y,296,28);
            if(current){colour(g,0xffffc247u);rect(g,12,y,3,28);}
            colour(g,current?0xffffc247u:0xffeeeeeeu);
            text_at(g,items[i],23,y+1,283,16,14.0f,0x21);
            colour(g,0xffb5b5b5u);
            text_at(g,detail[i],23,y+16,283,11,10.0f,0x21);
        }
        colour(g,0xff252525u);rect(g,12,198,296,1);
        colour(g,0xffbbbbbbu);text(g,can_export?"Turn DATA to select   /   B3 to export":"Insert a sequence before exporting",12,204,296,22,12.0f);
        return;
    }
    { struct song_ref pr; if(song_ref(&pr)) paint_rows(p,g,&pr); }
    /* On the Song step list B2=Remove; Shift+B2=Duplicate. This label
     * is distinct from Song Export's Mixdown/Stems, below. */
    if(atomic_load(&export_shift)){
        colour(g,0xff000000u); rect(g,109,2,103,34);
        colour(g,0xff00baffu); text_at(g,"Duplicate",109,2,103,34,15.0f,0x24);
    }
    if(!(toast_until&&now_ms()<toast_until)) return;
    if(song_ref(&r)&&step_at(&r,r.row,&seq,&rep))
        snprintf(line,sizeof(line),"%.47s   Step %d/%d x%d",toast,r.row+1,r.count,rep);
    else snprintf(line,sizeof(line),"%s",toast);
    /* Transient strip after an edit; the stock list is untouched otherwise. */
    colour(g,0xff000000); rect(g,0,224,320,16);
    colour(g,0xffffc247); text(g,line,4,224,312,16,12.0f);
}
static void forget(void *p){ if(screen==p){ screen=NULL; atomic_store(&shown,0); } }
/* The native Create name dialog is a separate JUCE component tree.
 * Attach only to an ancestor whose paintOverChildren is the pinned native
 * no-op. A different firmware/class or an already-hooked slot is untouched.
 * The callback runs after the dialog's children, unlike ExportScreen paint. */
static void stems_modal_footer_paint(void *p,void *g)
{
    if(p!=stems_modal_root||!atomic_load(&stems_modal_visible))return;
    int w=FIELD(p,0x28,int),h=FIELD(p,0x2c,int);
    if(w<200||w>320||h<150||h>240)return;
    /* Align the panel with the native dialog's bottom-left, not the text
     * entry rectangle. Leave the stock Save-to-Internal control untouched. */
    const int x=(w>=300)?38:8;
    const int y=(h>=230)?180:h-44;
    const unsigned pads=atomic_load(&stems_modal_pads);
    colour(g,0xff000000u);rect(g,x,y,75,43);
    colour(g,0xffffffffu);
    text_at(g,"Input",x,y+3,75,13,12.0f,0x24);
    text_at(g,pads?"Pads":"Encoder",x,y+17,75,19,17.0f,0x24);
    colour(g,0xff3b3b3bu);rect(g,x+7,y+39,61,2);
    colour(g,0xffffffffu);rect(g,x+7+(pads?30:0),y+39,31,2);
}

/* Called only from the existing TextEditor::paint hook on the JUCE thread.
 * Component::parentComponent is +0x18, bounds are +0x28/+0x2c. Limit the
 * walk, validate the native vptr and never patch non-noop callbacks. */
void song_ui_stems_name_modal_attach(void *editor)
{
    if(!editor||!atomic_load(&stems_modal_visible)||stems_modal_root)return;
    void *candidate=FIELD(editor,0x18,void*);
    for(unsigned depth=0;candidate&&depth<12;depth++){
        int w=FIELD(candidate,0x28,int),h=FIELD(candidate,0x2c,int);
        uintptr_t vp=FIELD(candidate,0,uintptr_t);
        if(w>=200&&w<=320&&h>=150&&h<=240&&
           vp>=base+0x6000000u&&vp<base+0x8000000u){
            uintptr_t *slot=(uintptr_t*)(vp+SLOT_PAINT);
            uintptr_t orig=*slot;
            if(stems_modal_scan_logs<4){
                fprintf(stderr,"STEMS MODAL_CANDIDATE depth=%u wh=%dx%d vptr=%lx paint=%lx\n",
                    depth,w,h,(unsigned long)(vp-base),(unsigned long)(orig-base));
                ++stems_modal_scan_logs;
            }
            if(orig==base+NATIVE_NOOP||orig==(uintptr_t)stems_modal_footer_paint){
                if(orig==base+NATIVE_NOOP){
                    if(sysconf(_SC_PAGESIZE)!=4096)return;
                    void *page=(void*)((uintptr_t)slot&~(uintptr_t)4095);
                    if(mprotect(page,4096,PROT_READ|PROT_WRITE))return;
                    *slot=(uintptr_t)stems_modal_footer_paint;
                    if(mprotect(page,4096,PROT_READ))_exit(121);
                }
                stems_modal_root=candidate;
                fprintf(stderr,"STEMS MODAL_FOOTER_ATTACHED depth=%u wh=%dx%d\n",depth,w,h);
                repaint(stems_modal_root);
                return;
            }
        }
        void *parent=FIELD(candidate,0x18,void*);
        if(parent==candidate)break;
        candidate=parent;
    }
    if(stems_modal_scan_logs<5){
        fprintf(stderr,"STEMS MODAL_FOOTER_UNAVAILABLE (no verified native modal painter)\n");
        stems_modal_scan_logs=5;
    }
}

static void export_paint(void *p,void *g)
{
    (void)g;
    /* Native ExportScreen is now a hidden routing step, not our chooser. */
    if(FIELD(p,0,uintptr_t)!=base+EXPORT_VPTR)return;
    export_screen=p;
    if(!atomic_exchange(&export_paint_seen,1u))
        fprintf(stderr,"SONG_UI EXPORT_PAINT_SEEN\n");
}
static void export_forget(void *p){ if(export_screen==p) export_screen=NULL; }
static void export_destroy(void *p){ export_forget(p); FN(EXPORT_DTOR,void(*)(void*))(p); }
static void export_delete(void *p){ export_forget(p); FN(EXPORT_DELETE,void(*)(void*))(p); }
static void destroy(void *p){ forget(p); FN(NATIVE_DTOR,void(*)(void*))(p); }
static void destroy_delete(void *p){ forget(p); FN(NATIVE_DELETE,void(*)(void*))(p); }

void song_ui_tick(void)
{
    int v=visible(screen);
    int exporting=visible(export_screen);
    unsigned shift=atomic_load(&export_shift);
    /* Log transitions only. If the Export screen is visible but this
     * predicate stays zero, the resulting trace shows the exact failure. */
    static int last_exporting=-1,last_song=-1,last_capture=-1;
    int captured=(int)atomic_load(&export_paint_seen);
    if(exporting!=last_exporting||v!=last_song||captured!=last_capture){
        fprintf(stderr,"SONG_UI VISIBLE song=%d export=%d paint_seen=%d\n",v,exporting,captured);
        last_exporting=exporting;last_song=v;last_capture=captured;
    }
    /* Shift edges are received on the MIDI/input thread. Schedule the
     * safe on-UI repaint here, including when Shift is released. */
    if(shift!=export_shift_seen && v)repaint(screen);
    export_shift_seen=shift;
    /* Native export routing begins ONLY after our Song-screen chooser is
     * confirmed. Never synthesize B2 while Song is still visible. */
    if(export_route_stage){
        uint64_t elapsed=now_ms()-export_route_since;
        if(elapsed>5000u){
            fprintf(stderr,"SONG_EXPORT ROUTE_TIMEOUT stage=%u song=%d export=%d\n",
                export_route_stage,v,exporting);
            export_route_stage=0;
        }else if(export_route_stage==1u && !v && elapsed>=120u && forward_native){
            unsigned selected=export_route_choice;
            /* The first native B1 created its original question. The native
             * B2/B3 below is the real action for that question. */
            export_route_stage=(selected==1u)?0u:2u;
            export_route_since=now_ms();
            _Alignas(8) unsigned char ev[16];
            memcpy(ev,export_route_template,sizeof(ev));
            ev[8]=(unsigned char)(selected==1u?B3:B2);
            ev[9]=1;forward_native(ev);
            ev[9]=0;forward_native(ev);
            fprintf(stderr,"SONG_EXPORT ROUTE_NATIVE selected=%u code=%02x\n",selected,ev[8]);
            if(selected==1u)
                fprintf(stderr,"SONG_EXPORT SEQUENCE_USES_NATIVE_SONG_NAME (no editor)\n");
        }else if(export_route_stage==2u && exporting && forward_native){
            unsigned selected=export_route_choice;
            export_route_stage=0u;
            atomic_store(&stems_name_stage,1u);
            atomic_store(&export_name_kind,selected>=2u?selected:1u);
            stems_name_requested_ms=now_ms();
#ifdef MPC_MIXER_LIVE
            if(mixer_stems_disarm)mixer_stems_disarm();
#endif
            /* Second native B2 enters the same stock Create name dialog for
             * both Mixdown and Stems. The existing phone pad bridge is
             * activated by song_ui_stems_name_pending() for either type. */
            _Alignas(8) unsigned char ev[16];
            memcpy(ev,export_route_template,sizeof(ev));
            ev[8]=B2;
            ev[9]=1;forward_native(ev);
            ev[9]=0;forward_native(ev);
            fprintf(stderr,"SONG_EXPORT NAME_REQUESTED kind=%u\n",atomic_load(&export_name_kind));
        }
    }
    unsigned stage=atomic_load(&stems_name_stage);
    if(stage==1u && now_ms()-stems_name_requested_ms>20000u){
        /* If native Mixdown failed to open a name dialog, don't leave an
         * armed export waiting to steal the user's next normal Mixdown. */
#ifdef MPC_MIXER_LIVE
        if(mixer_stems_disarm)mixer_stems_disarm();
#endif
        atomic_store(&stems_name_stage,0u);
        atomic_fetch_and(&owned_buttons,~button_bit(B2));
        if(exporting)repaint(export_screen);
        fprintf(stderr,"SONG_EXPORT NAME_OPEN_TIMEOUT kind=%u stage=%u\n",
            atomic_load(&export_name_kind),stage);
        atomic_store(&export_name_kind,0u);
    }
    atomic_store(&export_shown,(unsigned)exporting);
    atomic_store(&pads_shown,(unsigned)(v||exporting));
    /* If the Song page was closed externally while our local selector was
     * open, discard the selector rather than drawing it on a stale screen. */
    if(!v && atomic_load(&export_menu_active))atomic_store(&export_menu_active,0u);
    if(v!=(int)atomic_load(&shown)){ atomic_store(&shown,(unsigned)v); knob_last=-1; if(v)report("SONG_SCREEN_VISIBLE"); }
    if(toast_until&&now_ms()>=toast_until){ toast_until=0; repaint(screen); }
    if(!v){ pending_row=-1; last_row=-1; return; } /* keep the last selection for Song Export */
    struct song_ref r;
    if(!song_ref(&r)) return;
    { int chosen=FIELD(r.controller,0x1dc,int); atomic_store(&selected_sequence,chosen>=0&&chosen<128?chosen:-1); }
    if(pending_row>=0&&(r.count==pending_count||now_ms()>=pending_until)){
        if(pending_row<r.count){
            FN(0x14e19c0,void(*)(void*,int))((char*)r.controller+0x168,pending_row);
            r.row=pending_row;
        }
        pending_row=-1;
    }
    /* Browsing: show the selected step's repeats for a moment (the stock
     * list shows only the sequence length). */
    int seq,rep;
    if(r.row!=last_row){
        int was=last_row; last_row=r.row;
        if(was>=0&&!toast_until&&step_at(&r,r.row,&seq,&rep)&&rep>1){
            char m[24]; snprintf(m,sizeof(m),"Repeats x%d",rep);
            snprintf(toast,sizeof(toast),"%s",m); toast_until=now_ms()+1500; repaint(screen);
        }
    }
}

int song_ui_install(void)
{
    uintptr_t *slots[6]={(uintptr_t*)(base+SONG_VPTR+SLOT_PAINT),(uintptr_t*)(base+SONG_VPTR),(uintptr_t*)(base+SONG_VPTR+8),
        (uintptr_t*)(base+EXPORT_VPTR+SLOT_PAINT),(uintptr_t*)(base+EXPORT_VPTR),(uintptr_t*)(base+EXPORT_VPTR+8)};
    const uintptr_t expected[6]={base+NATIVE_NOOP,base+NATIVE_DTOR,base+NATIVE_DELETE,base+NATIVE_NOOP,base+EXPORT_DTOR,base+EXPORT_DELETE};
    const uintptr_t callbacks[6]={(uintptr_t)paint,(uintptr_t)destroy,(uintptr_t)destroy_delete,
        (uintptr_t)export_paint,(uintptr_t)export_destroy,(uintptr_t)export_delete};
    for(unsigned i=0;i<6;++i) if(*slots[i]!=expected[i]) return 0;
    if(sysconf(_SC_PAGESIZE)!=4096) return 0;
    void *page=(void*)((uintptr_t)slots[0]&~(uintptr_t)4095);
    for(unsigned i=1;i<6;++i) if(((uintptr_t)slots[i]&~(uintptr_t)4095)!=(uintptr_t)page) return 0;
    if(mprotect(page,4096,PROT_READ|PROT_WRITE)) return 0;
    for(unsigned i=0;i<6;++i) *slots[i]=callbacks[i];
    if(mprotect(page,4096,PROT_READ)) _exit(121);
    report("INSTALLED");
    return 1;
}
