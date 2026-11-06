#define _GNU_SOURCE
#include "song_ui.h"
#include "phone_events.h"
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
#define SONG_BAR_LIMIT 0x3137
#define REPEATS_MAX 99
#define FIELD(p,off,type) (*(type *)((char *)(p)+(off)))
#define FN(off,type) ((type)(base+(off)))

enum { B2=0x0d, B3=0x0e, PLUS=0x36, MINUS=0x37, K1=0x10 };

static uintptr_t base;
static void *screen;                     /* UI thread only */
static _Atomic unsigned shown;           /* producer reads */
static _Atomic unsigned owned_buttons;   /* bit per claimed button down */
static _Atomic int selected_sequence=-1; /* published by the UI Timer for the RGB layer */
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
{ return code==B2?1u:code==B3?2u:code==PLUS?4u:code==MINUS?8u:0u; }

void song_ui_init(uintptr_t b){ base=b; }
int song_ui_active(void){ return atomic_load(&shown)!=0; }
int song_ui_selected_sequence(void){ return atomic_load(&selected_sequence); }

int song_ui_owns(unsigned kind,unsigned code,int shift,int down)
{
    if(kind==PHONE_CONTROLLER) return atomic_load(&shown) && code==K1;
    if(kind!=PHONE_BUTTON) return 0;
    unsigned bit=button_bit(code);
    if(!bit) return 0;
    if(!down) return (atomic_load(&owned_buttons)&bit)!=0;
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
    if(!screen||!atomic_load(&shown)) return 1;
    if(code==B3) replace_step();
    else if(code==B2) duplicate_step();
    else move_step(code==PLUS?1:-1);
    return 1;
}

/* ---- paint / lifetime ---- */
static void colour(void *g,uint32_t c){ FN(0x102d5e0,void(*)(void*,uint32_t))(g,c); }
static void rect(void *g,int x,int y,int w,int h){ FN(0x102e000,void(*)(void*,int,int,int,int))(g,x,y,w,h); }
static void text(void *g,const char *s,int x,int y,int w,int h,float height)
{
    uint64_t string=0,font=0;
    FN(0xf535c0,void(*)(uint64_t*,const char*))(&string,s);
    FN(0x1082810,void(*)(uint64_t*,int,float))(&font,0,height);
    FN(0x102dca0,void(*)(void*,const uint64_t*))(g,&font);
    FN(0x10817b0,void(*)(void*,const uint64_t*,int,int,int,int,int,int,float))(g,&string,x,y,w,h,0x24,1,0.70f);
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
static void paint(void *p,void *g)
{
    if(FIELD(p,0,uintptr_t)!=base+SONG_VPTR) return;
    screen=p;
    { struct song_ref pr; if(song_ref(&pr)&&pr.count>=2) probe(p); } /* rows exist only once the song has steps */
    struct song_ref r; int seq,rep;
    char line[128];
    if(!(toast_until&&now_ms()<toast_until)) return;
    if(song_ref(&r)&&step_at(&r,r.row,&seq,&rep))
        snprintf(line,sizeof(line),"%.47s   Step %d/%d x%d",toast,r.row+1,r.count,rep);
    else snprintf(line,sizeof(line),"%s",toast);
    /* Transient strip after an edit; the stock list is untouched otherwise. */
    colour(g,0xff000000); rect(g,0,224,320,16);
    colour(g,0xffffc247); text(g,line,4,224,312,16,12.0f);
}
static void forget(void *p){ if(screen==p){ screen=NULL; atomic_store(&shown,0); } }
static void destroy(void *p){ forget(p); FN(NATIVE_DTOR,void(*)(void*))(p); }
static void destroy_delete(void *p){ forget(p); FN(NATIVE_DELETE,void(*)(void*))(p); }

void song_ui_tick(void)
{
    int v=visible(screen);
    if(v!=(int)atomic_load(&shown)){ atomic_store(&shown,(unsigned)v); knob_last=-1; if(v)report("SONG_SCREEN_VISIBLE"); }
    if(toast_until&&now_ms()>=toast_until){ toast_until=0; repaint(screen); }
    if(!v){ pending_row=-1; last_row=-1; atomic_store(&selected_sequence,-1); return; }
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
    uintptr_t *slots[3]={(uintptr_t*)(base+SONG_VPTR+SLOT_PAINT),(uintptr_t*)(base+SONG_VPTR),(uintptr_t*)(base+SONG_VPTR+8)};
    const uintptr_t expected[3]={base+NATIVE_NOOP,base+NATIVE_DTOR,base+NATIVE_DELETE};
    const uintptr_t callbacks[3]={(uintptr_t)paint,(uintptr_t)destroy,(uintptr_t)destroy_delete};
    for(unsigned i=0;i<3;++i) if(*slots[i]!=expected[i]) return 0;
    if(sysconf(_SC_PAGESIZE)!=4096) return 0;
    void *page=(void*)((uintptr_t)slots[0]&~(uintptr_t)4095);
    for(unsigned i=1;i<3;++i) if(((uintptr_t)slots[i]&~(uintptr_t)4095)!=(uintptr_t)page) return 0;
    if(mprotect(page,4096,PROT_READ|PROT_WRITE)) return 0;
    for(unsigned i=0;i<3;++i) *slots[i]=callbacks[i];
    if(mprotect(page,4096,PROT_READ)) _exit(121);
    report("INSTALLED");
    return 1;
}
