#define _GNU_SOURCE
#include "music_ui.h"
#include "sample_mute.h"
#include "music_settings.h"
#include "mod_build.h"
#ifdef MPC_PROJECT_RECOVERY
#include "project_recovery.h"
#endif
#include "pad_lighting_project.h"
#include "music_archive.h"
#include "note_native.h"
#include "tab_screens.h"
#include "workflow_ui.h"
#include "project_exit.h"
#ifdef MPC_CHORDS
#include "chords_ui.h"
#include "takeover_native.h"
#endif
#ifdef MPC_POWER_BUTTON
#include "power_control.h"
#endif
#ifdef MPC_PAD_EQ
#include "pad_eq.h"
#include "pad_eq_native.h"
#include "resample_ui.h"
static int eq_page;
static void eq_close(void){if(eq_page){pad_eq_native_report();
#ifdef MPC_CHORDS
 takeover_native_context_changed();
#endif
 }eq_page=0;}
static void eq_paint(void*,unsigned);
static int choice(unsigned,int,int,int,int);
#endif
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

/* Components and drawing are accessed only by the existing native UI Timer.
 * Producer callbacks read numeric atomic flags; menu callbacks only post an
 * identity request. No worker thread paints or changes a Component. */
static uintptr_t base;
static void *sample,*project;
static _Atomic(uintptr_t) sample_id,project_id,request;
static _Atomic unsigned dead_sample,dead_project,active,custom,held_count;
static int lights,key_page,focus,role,last_shift=-1,last_index=-2;
static uint64_t last_generation;
static char last_sample[MUSIC_NAME_BYTES];
static unsigned owned_buttons[112];
static unsigned log_count;
static _Atomic unsigned repeat_held,repeat_visible,dead_repeat;
static void *repeat_root;
static _Atomic unsigned seq_active,global_shift;
static _Atomic unsigned exit_blocked;
static int exit_presented,exit_started,lighting_save_error;
static unsigned mod_tab,mod_row,mod_edit,mod_reset_confirm;
enum { MOD_ABOUT_TAB=4, MOD_TAB_COUNT=5 };
static char exit_error[160];
int music_ui_exit_saving(void){return atomic_load(&exit_blocked)!=0;}
static void*seq_screen;
static void *knob_fx_root;
static _Atomic unsigned knob_fx_visible;
int music_ui_knob_fx_active(void){return atomic_load(&knob_fx_visible)!=0;}
static void sequence_paint(void*,void*);
static unsigned painted_headers[TAB_SCREEN_COUNT];
static void knob_underlay(void*,void*);
static void repeat_paint(void*,void*);
static int choice(unsigned,int,int,int,int);
#define OVERLAY_AMBER 0xffffb000u
#define FIELD(p,off,type) (*(type *)((char *)(p)+(off)))
#define FN(off,type) ((type)(base+(off)))
static void knob_fx_destroy(void*p){if(knob_fx_root==p){knob_fx_root=NULL;atomic_store(&knob_fx_visible,0);}FN(0x1627040,void(*)(void*))(p);}
static void knob_fx_delete(void*p){if(knob_fx_root==p){knob_fx_root=NULL;atomic_store(&knob_fx_visible,0);}FN(0x1628750,void(*)(void*))(p);}

static void sequence_destroy(void*p){if(seq_screen==p){seq_screen=NULL;atomic_store(&seq_active,0);}FN(0x169a0f0,void(*)(void*))(p);}
static void sequence_delete(void*p){if(seq_screen==p){seq_screen=NULL;atomic_store(&seq_active,0);}FN(0x169a6d0,void(*)(void*))(p);}
static void repeat_destroy(void*p){if(repeat_root==p){repeat_root=NULL;atomic_store(&repeat_visible,0);}FN(0x1639740,void(*)(void*))(p);}
static void repeat_delete(void*p){if(repeat_root==p){repeat_root=NULL;atomic_store(&repeat_visible,0);}FN(0x1639df0,void(*)(void*))(p);}
#define SAMPLE_VPTR 0x6e212a0
#define PROJECT_VPTR 0x6e1f788
static void report(const char *message)
{ if (log_count++<48) fprintf(stderr,"MUSIC_UI %s\n",message); }

void music_ui_init(uintptr_t b) { base=b; }
void music_ui_requested(unsigned code,unsigned down)
{ if(code==0x0b)atomic_store(&repeat_held,!!down);if(code==0x31)atomic_store(&global_shift,!!down); }
int music_ui_repeat_active(void){return atomic_load(&repeat_held)!=0||atomic_load(&repeat_visible)!=0;}
int music_ui_sequence_visible(void){return atomic_load(&seq_active);}
int music_ui_active(void) { return atomic_load(&active) || atomic_load(&held_count) || atomic_load(&seq_active); }
int music_ui_owns(unsigned cc)
{ return !music_ui_repeat_active() && ((music_pad_lighting_mode()&&atomic_load(&seq_active)&&atomic_load(&global_shift)&&cc==0x10) || (atomic_load(&custom) && (cc==0x10 || cc==0x11 || cc==0x12 || cc==0x64))); }
int music_ui_button_owns(unsigned code)
{
    if(atomic_load(&exit_blocked)&&code!=0x31)return 1;
    /* Native 0x159f330: note 0x6f -> InputController+0x2d50.
     * Binding 0x1659848 -> 0x16541a0 advances the native knob focus. */
    return !music_ui_repeat_active() && atomic_load(&custom) && code==0x6f;
}

/* These are per-class paintOverChildren slots, never the shared no-op entry.
 * Both tables are in the pinned ELF's GNU_RELRO mapping. */
int music_ui_install(void (*sp)(void*,void*),void (*pp)(void*,void*),void (*tp)(void*,void*))
{
    uintptr_t *slots[13]={(uintptr_t*)(base+SAMPLE_VPTR+0xd8),
        (uintptr_t*)(base+PROJECT_VPTR+0xd8),(uintptr_t*)(base+0x706e940+0xd8),
        (uintptr_t*)(base+0x6e1e258+0xd0),(uintptr_t*)(base+0x6e1f060+0xd0),
        (uintptr_t*)(base+0x6e23e38+0xd8),(uintptr_t*)(base+0x6e23e38),(uintptr_t*)(base+0x6e23e38+8),
        (uintptr_t*)(base+0x6e1f060+0xd8),(uintptr_t*)(base+0x6e1f060),(uintptr_t*)(base+0x6e1f060+8),(uintptr_t*)(base+0x6e1e258),(uintptr_t*)(base+0x6e1e258+8)};
    uintptr_t callbacks[13]={(uintptr_t)sp,(uintptr_t)pp,(uintptr_t)tp,(uintptr_t)knob_underlay,(uintptr_t)knob_underlay,(uintptr_t)sequence_paint,(uintptr_t)sequence_destroy,(uintptr_t)sequence_delete,(uintptr_t)repeat_paint,(uintptr_t)repeat_destroy,(uintptr_t)repeat_delete,(uintptr_t)knob_fx_destroy,(uintptr_t)knob_fx_delete};
    for (unsigned i=0;i<13;++i) if (*slots[i]!=base+(i==6?0x169a0f0:i==7?0x169a6d0:i==9?0x1639740:i==10?0x1639df0:i==11?0x1627040:i==12?0x1628750:0x10e2e10)) return 0;
    /* Frozen r16d footer code: only the Knob FX background MOV W9,#c247.
     * Constructor runs before native main/audio. Never change live code. */
    uint32_t*amber=(uint32_t*)(base+0x6d3ba84);
    if(*amber!=0x529848e9u || amber[1]!=0x72bfffe9u)return 0;
    if(memcmp((void*)(base+0x6d3b400),lighting_project_before,sizeof(lighting_project_before))||
       memcmp((void*)(base+0x1643908),lighting_dispatch_before,4)||
       memcmp((void*)(base+0x1644ff4),lighting_rows_before,4))return 0;
    long ps=sysconf(_SC_PAGESIZE);
    if (ps!=4096) return 0;
    void *pages[13]; unsigned writable=0;
    for (unsigned i=0;i<13;++i) {
        pages[i]=(void*)((uintptr_t)slots[i]&~(uintptr_t)4095);
        if (mprotect(pages[i],4096,PROT_READ|PROT_WRITE)) {
            for (unsigned j=0;j<writable;++j) if (mprotect(pages[j],4096,PROT_READ)) _exit(121);
            return 0;
        }
        ++writable;
    }
    void*code_pages[2]={(void*)(base+0x6d3b000),(void*)(base+0x1643000)};
    unsigned code_writable=0;
    for(unsigned i=0;i<2;++i){if(mprotect(code_pages[i],4096,PROT_READ|PROT_WRITE)){
        for(unsigned j=0;j<code_writable;++j)if(mprotect(code_pages[j],4096,PROT_READ|PROT_EXEC))_exit(121);
        for(unsigned j=0;j<13;++j)if(mprotect(pages[j],4096,PROT_READ))_exit(121);
        return 0;}++code_writable;}
    *amber=0x52800009u|(0xb000u<<5);
    memcpy((void*)(base+0x6d3b400),lighting_project_after,sizeof(lighting_project_after));
    memcpy((void*)(base+0x1643908),lighting_dispatch_after,4);
    for(unsigned i=0;i<2;++i){__builtin___clear_cache((char*)code_pages[i],(char*)code_pages[i]+4096);
        if(mprotect(code_pages[i],4096,PROT_READ|PROT_EXEC))_exit(121);}
    for (unsigned i=0;i<13;++i) *slots[i]=callbacks[i];
    for (unsigned i=0;i<13;++i) if (mprotect(pages[i],4096,PROT_READ)) _exit(121);
    return 1;
}
static int visible(void *p)
{ return p && FN(0x112fd50,int(*)(void*))(p) && !FN(0x112f6b0,int(*)(void*))(p); }
static int sample_pad_muted(unsigned pad)
{
    void *things=note_native_context();
    void *program=things?FIELD(things,0xb8,void*):NULL;
    void *instrument=program&&pad<128?FN(0x286bbe0,void*(*)(void*,int))(program,(int)pad):NULL;
    return sample_instrument_muted(instrument);
}
int music_ui_waveform_tint(uint32_t*rgb){
 unsigned pad;
 if(!rgb||!note_native_pad_index(&pad))return 0;
 if(sample_pad_muted(pad)){*rgb=SAMPLE_MUTED_TINT;return 1;}
 if(!music_pad_lighting_mode()||!music_mod_setting(MUSIC_SETTING_WAVE))return 0;
 *rgb=music_identity_rgb((unsigned)music_pad_colour(pad));return 1;
}
int music_ui_filter_spectrum_visible(void*p){
#ifdef MPC_PAD_EQ
    return p&&FIELD(p,0,uintptr_t)==base+SAMPLE_VPTR&&visible(p)&&!visible(project)
        &&!music_ui_repeat_active()&&!eq_page&&FIELD(p,0xe50+0x21c,int)==2
        &&FIELD(p,0xe50+0x218,int)>=0&&FIELD(p,0xe50+0x218,int)<=1;
#else
    (void)p;return 0;
#endif
}
int music_ui_chords_available(void){return visible(sample)&&!visible(project)&&!music_ui_repeat_active()&&!key_page;}
int music_ui_shift_active(void){return atomic_load(&global_shift)!=0;}
static void repaint(void *p)
{ if (p) FN(0x1149880,void(*)(void*,uint64_t,uint64_t))(p,0,FIELD(p,0x28,uint64_t)); }
static int shifted(void *p,int is_project)
{
    if (!p) return 0;
    void *c=FIELD(p,is_project ? 0x100 : 0x108,void*);
    return c && __atomic_load_n((unsigned char*)c+0x22a0,__ATOMIC_ACQUIRE)!=0;
}
void music_ui_destroyed(void *p)
{
    if(p==repeat_root){atomic_store(&dead_repeat,1);atomic_store(&repeat_visible,0);}
    if ((uintptr_t)p==atomic_load(&sample_id)) {
        atomic_store(&sample_id,0); atomic_store(&dead_sample,1);
        atomic_store(&active,0); atomic_store(&custom,0);
    }
    if ((uintptr_t)p==atomic_load(&project_id)) {
        atomic_store(&project_id,0); atomic_store(&dead_project,1);
        atomic_store(&active,0); atomic_store(&custom,0);
    }
}
void music_ui_request_project(void *p)
{ atomic_store(&request,(uintptr_t)p); }
int music_ui_tick(void)
{
    atomic_store(&knob_fx_visible,visible(knob_fx_root));
    if(atomic_exchange(&dead_repeat,0))repeat_root=NULL;
    atomic_store(&repeat_visible,repeat_root&&visible(repeat_root));
    if (atomic_exchange(&dead_sample,0)) { sample=0; lights=0; last_index=-2;
#ifdef MPC_PAD_EQ
        eq_close();
#endif
    }
    if (atomic_exchange(&dead_project,0)) { project=0; key_page=0; }
    uintptr_t requested=atomic_exchange(&request,0);
    if (requested && (requested&~(uintptr_t)3)==atomic_load(&project_id) && visible(project)) {
        key_page=(requested&2)?5:(requested&1)?2:1;lighting_save_error=0;mod_tab=mod_row=mod_edit=mod_reset_confirm=0; focus=0; repaint(project); report(key_page==2?"MOD_OFF_CONFIRM_OPEN":"PROJECT_KEY_PAGE_OPEN");
    }
    int sv=visible(sample)&&!music_ui_repeat_active(),pv=visible(project);
    /* Project is a native overlay; underlying Sample can remain showing.
     * Its hidden Lights controls must not consume the menu's encoder click. */
    if(pv)sv=0;
    static int last_mute;
    unsigned edited_pad;
    int muted=sv&&note_native_pad_index(&edited_pad)&&sample_pad_muted(edited_pad);
    if(muted!=last_mute){last_mute=muted;if(sv)repaint(sample);}
    atomic_store(&seq_active,!pv&&visible(seq_screen)&&!music_ui_repeat_active());
    if (!sv) lights=0;
#ifdef MPC_PAD_EQ
    if(sv&&FIELD(sample,0xe50+0x218,int)!=1)eq_close();
    /* Repaint-driven watching expires during silence. Keep capture alive
     * while a spectrum page is actually visible, and stop it on leaving. */
    unsigned spectrum_pad;
    if(sv&&(eq_page||music_ui_filter_spectrum_visible(sample))&&note_native_pad_index(&spectrum_pad))
        pad_eq_spectrum_watch((int)spectrum_pad);
    else pad_eq_spectrum_watch(-1);
#endif
    if (!pv&&(key_page<3||key_page==5)) key_page=0;
    if(key_page==3&&exit_presented){
        if(!exit_started){
            exit_started=1;
            project_exit_start(base,workflow_project_services());
        }
        int saved=project_exit_status();
        if(saved==2&&exit_started==1){
            int fd=open("/run/mpc-trial-app/mod-off.request",O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC|O_NOFOLLOW,0600);
            int ok=fd>=0&&write(fd,"1",1)==1;
            if(fd>=0)close(fd);
            if(ok){exit_started=2;exit_presented=0;report("MOD_OFF_COPY_SAVED_CONFIRMED");}
            else{snprintf(exit_error,sizeof(exit_error),"Could not request Mod Off");key_page=4;atomic_store(&exit_blocked,0);}
        }else if(saved==3){
            snprintf(exit_error,sizeof(exit_error),"%s",project_exit_message());
            key_page=4;atomic_store(&exit_blocked,0);report("MOD_OFF_SAVE_FAILED_STAYING_IN_MOD");
        }
        repaint(project);
    }
    int shift=shifted(key_page ? project : sample,key_page);
    int index=sample ? FIELD(sample,0x7f0+0x218,int) : -1;
    if (lights && index!=1) lights=0;
    uint64_t gen=music_generation();
    char selected[MUSIC_NAME_BYTES]={0};
    if (sample) note_native_current(selected,sizeof(selected));
    int changed=strcmp(selected,last_sample)!=0;
    if (changed) memcpy(last_sample,selected,strlen(selected)+1);
#ifdef MPC_PAD_EQ
    /* Rebind restored project settings before EQ is opened, too. Model
     * pointers are still resolved exclusively by the queued owner. */
    if(gen!=last_generation||changed)resample_ui_prime_eq();
#endif
    if (shift!=last_shift || index!=last_index || gen!=last_generation || changed) {
        last_shift=shift; last_index=index; last_generation=gen;
        if (sv) repaint(sample);
        if (pv && key_page) repaint(project);
        if(atomic_load(&seq_active))repaint(seq_screen);
    }
    atomic_store(&active,(unsigned)(sv || (pv && key_page)));
    atomic_store(&custom,(unsigned)((sv && lights) || (pv && key_page)
#ifdef MPC_PAD_EQ
        || (sv&&eq_page)
#endif
    ));
    return sample!=NULL || atomic_load(&seq_active) || (project!=NULL && pv);
}
static void colour(void *g,uint32_t c) { FN(0x102d5e0,void(*)(void*,uint32_t))(g,c); }
static void rect(void *g,int x,int y,int w,int h)
{ FN(0x102e000,void(*)(void*,int,int,int,int))(g,x,y,w,h); }
static void knob_underlay(void*p,void*g){
    uintptr_t type=FIELD(p,0,uintptr_t);
    if(type!=base+0x6e1e258)return;
    knob_fx_root=p;atomic_store(&knob_fx_visible,visible(p));
    if(!atomic_load(&knob_fx_visible))return;
    /* This overlay's stock paint is empty. Cover the complete footer before
     * children paint, including gaps and the parent screen's focus bracket. */
    colour(g,OVERLAY_AMBER);rect(g,0,198,320,42);
}
static void text(void *g,const char *s,int x,int y,int w,int h,float height)
{
    uint64_t string=0,font=0;
    FN(0xf535c0,void(*)(uint64_t*,const char*))(&string,s);
    FN(0x1082810,void(*)(uint64_t*,int,float))(&font,0,height);
    FN(0x102dca0,void(*)(void*,const uint64_t*))(g,&font);
    FN(0x10817b0,void(*)(void*,const uint64_t*,int,int,int,int,int,int,float))
        (g,&string,x,y,w,h,0x24,1,0.70f);
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
extern void step_pad_string(void*,uint64_t*,uintptr_t);
static int header_toggled(void *button){
    /* JUCE Button::toggleState is a Value at +168. Native TextButton paint
     * reads Value::getValue -> var::operator bool, then destroys the var.
     * +1d4 is the listener-array size, not a toggle or cached Boolean.
     * step_pad_string is the generic nontrivial C++ return-in-x8 adapter. */
    if(!FIELD(button,0x168,void*))return 0;
    uint64_t value[2]={0,0};
    step_pad_string((char*)button+0x168,value,base+0xfb5580);
    int result=FN(0xf31660,int(*)(void*))(value);
    FN(0xf04380,void(*)(void*))(value);
    return !!result;
}
/* The native Note Repeat rate, as the toggled overlay button shows it,
 * mapped onto the Chords rate list (1/4 1/8 1/8T 1/16 1/16T 1/32). */
static _Atomic int repeat_rate=-1;
static void remember_repeat_rate(const char*title){
    static const char*const names[]={"1/4","1/8","1/8T","1/16","1/16T","1/32"};
    char t[16];unsigned n=0;
    for(const char*s=title;*s&&n<sizeof(t)-1;++s)if(*s!=' ')t[n++]=(char)((*s>='a'&&*s<='z')?*s-32:*s);
    t[n]=0;
    for(int i=0;i<6;++i)if(!strcmp(t,names[i])){atomic_store(&repeat_rate,i);return;}
}
int music_ui_repeat_rate(void){return atomic_load(&repeat_rate);}
static void repeat_paint(void*p,void*g){
    if(FIELD(p,0,uintptr_t)!=base+0x6e1f060)return;
    repeat_root=p;unsigned shown=(unsigned)visible(p);atomic_store(&repeat_visible,shown);
    if(!shown)return; /* A queued hidden paint cannot reclaim the header after release. */
    /* Native1638da0 lays out six alternative buttons, two per column.
     * Cover their entire38px hit areas after child paint: this also removes
     * native focus arms. The underlying Sample footer is never covered. */
    colour(g,OVERLAY_AMBER);rect(g,0,5,320,33);
    const unsigned offsets[]={0x1c0,0x3e0,0x600,0x820,0xa40,0xc60};
    for(unsigned i=0;i<6;++i){
        void*b=(char*)p+offsets[i];
        if(FIELD(b,0,uintptr_t)!=base+0x706e940||!FN(0x112fd50,int(*)(void*))(b))continue;
        const char*title=FIELD(b,0xb8,const char*);
        int x=FIELD(b,0x20,int),w=FIELD(b,0x28,int);
        if(!title||!*title||strnlen(title,48)>=48||x<0||w<50||x+w>320)continue;
        colour(g,0xff000000);text(g,title,x+4,7,w-8,24,16);
        if(header_toggled(b)){rect(g,x+10,32,w-20,3);remember_repeat_rate(title);}
    }
    colour(g,0xff805800);rect(g,106,6,1,29);rect(g,213,6,1,29);
}
/* Shift colour groups on the Lights page. Scale: Tonic, Scale, Other, Original.
 * Degrees: I, key notes, IV, V, Original. */
static unsigned role_count(const struct music_sample *s){return s->mode==MUSIC_DEGREES?5u:4u;}
static int role_field(const struct music_sample *s,int r){
    static const int scale[]={MUSIC_COLOUR0,MUSIC_COLOUR1,MUSIC_COLOUR2,MUSIC_ORIGINAL_COLOUR};
    static const int degrees[]={MUSIC_COLOUR0,MUSIC_COLOUR1,MUSIC_FOURTH_COLOUR,MUSIC_FIFTH_COLOUR,MUSIC_ORIGINAL_COLOUR};
    unsigned n=role_count(s);if(r<0)r=0;if((unsigned)r>=n)r=(int)n-1;
    return s->mode==MUSIC_DEGREES?degrees[r]:scale[r];
}
static int role_value(const struct music_sample *s,int field){
    return field==MUSIC_ORIGINAL_COLOUR?s->original_colour:field==MUSIC_FIFTH_COLOUR?s->fifth_colour:field==MUSIC_FOURTH_COLOUR?s->fourth_colour:s->colour[field-MUSIC_COLOUR0];
}
static int role_default(int field){
    return field==MUSIC_COLOUR0?MUSIC_DEFAULT_TONIC_COLOUR:field==MUSIC_COLOUR1?1:field==MUSIC_COLOUR2?10:field==MUSIC_FIFTH_COLOUR?MUSIC_DEFAULT_FIFTH_COLOUR:field==MUSIC_FOURTH_COLOUR?MUSIC_DEFAULT_FOURTH_COLOUR:4;
}
static void cell(void *g,int i,const char *name,const char *value,int shift,int badge,int enabled,int selected,int options)
{
    int x=i==2 ? 213 : i*106,w=i==2 ? 107 : 106;
    uint32_t ink=!enabled ? 0xff707070 : focus==i ? 0xffffc247 : 0xffffffff;
    colour(g,0xff000000); rect(g,x,198,w,42);
    colour(g,ink); text(g,name,x+4,200,badge ? w-22 : w-8,12,12);
    text(g,value,x+4,214,w-8,19,16);
    if (badge) shift_badge(g,i*106+94,200,shift);
    if (focus==i && enabled) {
        colour(g,ink); rect(g,x,198,w,2); rect(g,x,198,2,34); rect(g,x+w-2,198,2,34);
    }
    if (options>0 && options<=49) {
        int width=w-8;
        for (int n=0;n<options;++n) {
            int left=n*width/options,right=(n+1)*width/options;
            int zero=options==49 && n==24;
            colour(g,enabled && n==selected ? ink : zero ? 0xff808080 : 0xff3b3b3b);
            int segment=right-left-1;
            if (segment<1) segment=1;
            rect(g,x+4+left,zero ? 234 : 235,segment,zero ? 4 : 2);
        }
    }
}
static int settings(char *name,struct music_sample *s)
{ if (!note_native_current(name,MUSIC_NAME_BYTES)) return 0; music_get_sample(name,s); return 1; }
static int sequence_index(void){
    void*things=note_native_context(),*seq=things?FIELD(things,0x1c8,void*):NULL;
    int index=seq?FIELD(seq,0x480,int):-1;
    return index>=0&&index<128?index:-1;
}
int music_ui_sequence_selected(void){return sequence_index();}
static int clip_map_has_events(void *header,unsigned *budget){
    void *node=FIELD(header,0x10,void*);
    while(node&&node!=header&&*budget){
        --*budget;
        void *clip=FIELD(node,0x28,void*);
        if(clip&&FIELD(clip,0x710,uint64_t))return 1;
        node=FN(0xb51538,void*(*)(void*))(node);
    }
    return 0;
}
static int sequence_has_events(void *seq){
    /* Native Sequence has a global EventList at670 and per-track clip maps
     * at620 (2957030, 2957260). The used flag490 can also be set by changing
     * a name/length; it does not mean there is a recorded event. */
    if(FIELD(seq,0x6f8,uint64_t))return 1;
    /* Track zero uses the separate map628, not vector[0]: stock2957330
     * and29572e0. Normal pad recordings live here. */
    unsigned budget=4096;
    if(clip_map_has_events((char*)seq+0x630,&budget))return 1;
    void *maps=FIELD(seq,0x620,void*);if(!maps)return 0;
    uintptr_t first=FIELD(maps,0,uintptr_t),end=FIELD(maps,8,uintptr_t);
    if(end<first||(end-first)%0xe0||end-first>0xe0*1024u)return 0;
    for(uintptr_t track=first;track<end;track+=0xe0){
        if(clip_map_has_events((void*)(track+0xb8),&budget))return 1;
        if(!budget)return 0;
    }
    return 0;
}
unsigned music_ui_sequence_presence(unsigned bank){
    void *project=note_native_context();
    if(bank>=8||!project||FIELD(project,0,uintptr_t)!=base+0x6ecc648)return 0;
    int count=FIELD(project,0x125c,int);
    if(count<0||count>1024||(count&&!FIELD(project,0x1250,void*)))return 0;
    unsigned mask=0;
    for(unsigned i=0;i<16&&bank*16+i<(unsigned)count;++i){
        void *seq=FN(0x2909be0,void*(*)(void*,int))(project,(int)(bank*16+i));
        if(seq&&FIELD(seq,0,uintptr_t)==base+0x6ecdff0&&sequence_has_events(seq))mask|=1u<<i;
    }
    return mask;
}
static void sequence_paint(void*p,void*g){
    seq_screen=p;music_ui_tick();
#ifdef MPC_POWER_BUTTON
    if(power_control_busy()){power_control_paint(p,g);return;}
#endif
    if(!atomic_load(&seq_active))return;
    int index=sequence_index();if(index<0)return;
    if(!music_pad_lighting_mode())return;
    int c=music_sequence_colour((unsigned)index);
    colour(g,0xff000000|music_identity_rgb(c));rect(g,304,43,7,24);
    if(atomic_load(&global_shift)){
        int saved=focus;focus=0;
        cell(g,0,"Seq Colour","",1,1,1,(int)music_colour_choice_index((unsigned)c),MUSIC_IDENTITY_CHOICES);
        /* Swatch beside the name, as in Pad Colour. */
        colour(g,0xff000000|music_identity_rgb(c));rect(g,6,218,11,11);
        colour(g,0xffffc247);text(g,music_identity_name((unsigned)c),18,214,84,19,16);
        focus=saved;
    }
}
#ifdef MPC_PAD_EQ
static void eq_rect(void*g,int x,int y,int w,int h,uint32_t ink){colour(g,ink);rect(g,x,y,w,h);}
static void eq_text(void*g,const char*s,int x,int y,int w,int h,unsigned size,uint32_t ink,int centre){
    (void)centre;colour(g,ink);text(g,s,x,y,w,h,(float)size);
}
static void eq_paint(void*g,unsigned pad){struct resample_painter p={g,eq_rect,eq_text};pad_eq_paint(music_get_pad_eq(pad),pad,sample_pad_muted(pad),shifted(sample,0),focus,&p);}
static int eq_control(unsigned cc,unsigned value){
    unsigned pad;if(!note_native_pad_index(&pad))return 1;
    int rel=cc==0x64;if(!rel)focus=(int)cc-0x10;
    int shift=shifted(sample,0);if(shift&&focus==2)return 1;
    uint64_t v=music_get_pad_eq(pad),next=v;if(!v)v=pad_eq_default();
    if(focus==2){
        int current=(v&(UINT64_C(1)<<24))?0:(int)((v>>21)&7)+1;
        int selected=choice(value,current,0,PAD_EQ_TYPES,rel);
        if(!selected)next=pad_eq_edit(v,PAD_EQ_BYPASS,127,0);
        else next=pad_eq_edit(v,PAD_EQ_TYPE,(unsigned)(selected-1)*128/PAD_EQ_TYPES+1,0);
    }else{
        int field=focus==0?PAD_EQ_FREQUENCY:shift?PAD_EQ_Q:PAD_EQ_GAIN;
        unsigned type=(v>>21)&7;if(field==PAD_EQ_GAIN&&(type==PAD_EQ_LOW_PASS||type==PAD_EQ_HIGH_PASS||type==PAD_EQ_NOTCH))return 1;
        next=pad_eq_edit(v,field,value,rel);
    }
    if(next!=music_get_pad_eq(pad))music_set_pad_eq(pad,next);
    repaint(sample);return 1;
}
#endif
void music_ui_tab(void *p,void *g)
{
    if (FIELD(p,0,uintptr_t)!=base+0x706e940) return;
    int mode=FIELD(p,0x21c,int);
    if (mode<0 || mode>2) return;
    int count=mode==2 ? FIELD(p,0x214,int) : 0,index=mode==2 ? FIELD(p,0x218,int) : -1;
    int w=FIELD(p,0x28,int),h=FIELD(p,0x2c,int);
    if ((mode==2 && (count<1 || count>12 || index< -1 || index>=count))
        || w<50 || w>107 || h<18 || h>45) return;
    /* Shared top-row button inside the already audited Sample UI screens.
     * Physical hit areas and native action/toggle/tab bindings are untouched. */
    void *root=p; int x=0,y=0,found=0; unsigned screen_index=0;
    for (unsigned depth=0;depth<32 && root;++depth) {
        uintptr_t vptr=FIELD(root,0,uintptr_t);
        for (unsigned i=0;i<TAB_SCREEN_COUNT;++i) if (vptr==base+tab_screens[i]) { screen_index=i; found=1; break; }
        if (found) break;
        x+=FIELD(root,0x20,int); y+=FIELD(root,0x24,int);
        root=FIELD(root,0x18,void*);
    }
    if (!found || x<0 || x+w>320 || y<0 || y+h>45) return;
    /* QuestionScreen supplies answer captions through its own native binding.
     * Button+b8 can still be the identity Answer1/Answer2 here. Do not paint
     * over the native answer labels, including Erase+pad confirmation. */
    if (FIELD(root,0,uintptr_t)==base+0x6e1fe48) return;
    const char *title=FIELD(p,0xb8,const char*);
    if (!title || !*title || strnlen(title,96)>=96) return;
    if(FIELD(root,0,uintptr_t)==base+SAMPLE_VPTR&&(uintptr_t)p-(uintptr_t)root==0xe50&&shifted(root,0)&&!music_ui_repeat_active()){
        title="Stereo > Mono";count=0;index=-1;
    }
    if (FIELD(root,0,uintptr_t)==base+SAMPLE_VPTR && (uintptr_t)p-(uintptr_t)root==0x7f0) {
        count=3;
        if (root==sample && lights && index==1) { index=2; title="Lights"; }
        else title=index==1 ? "Play" : "Tune";
    }
#ifdef MPC_PAD_EQ
    if(FIELD(root,0,uintptr_t)==base+SAMPLE_VPTR&&(uintptr_t)p-(uintptr_t)root==0xe50&&!shifted(root,0)){count=3;if(root==sample&&eq_page&&index==1){index=2;title="EQ";}}
#endif
    unsigned column=(unsigned)((x+w/2)*3/320);
    if (column<3 && !(painted_headers[screen_index]&(1u<<column))) {
        painted_headers[screen_index]|=1u<<column;
        fprintf(stderr,"HEADER_TABS screen=%u column=%u mode=%d count=%d\n",screen_index,column,mode,count);
    }
    int enabled=FN(0x10f98d0,int(*)(void*))(p);
    int selected=index>=0 || (mode==1 && header_toggled(p));
    uint32_t ink=0xffffffff;
    if (mode==0) {
        /* TextButton's own textColourOffId: keep native action colours such
         * as red Cancel on the common black header background. */
        ink=FN(0x11353f0,uint32_t(*)(void*,int))(p,0x1000102);
        if (!(ink&0xffffff)) ink=0xffffffff;
    }
    if (!enabled) ink=0xff707070;
    else if (selected) ink=0xffffc247;
    int content=h<29 ? h : 29;
    colour(g,0xff000000); rect(g,0,0,w,h);
    colour(g,ink); text(g,title,4,1,w-8,content-7,14);
    if (count<2) {
        if (selected || (mode==0 && FIELD(p,0x160,int)==2)) { colour(g,ink); rect(g,8,content-4,w-16,1); }
        return;
    }
    int total=count*5+(count-1)*2,left=(w-total)/2;
    for (int i=0;i<count;++i) {
        int current=enabled && i==index;
        colour(g,current ? ink : 0xff606060);
        rect(g,left+i*7,content-4,5,current ? 3 : 1);
    }
}
void music_ui_sample(void *p,void *g)
{
    if (FIELD(p,0,uintptr_t)!=base+SAMPLE_VPTR) return;
    if (atomic_exchange(&dead_sample,0)) { sample=0; lights=0;
#ifdef MPC_PAD_EQ
        eq_close();
#endif
    }
    if (sample!=p) { sample=p; lights=0;
#ifdef MPC_PAD_EQ
        eq_close();
#endif
        atomic_store(&sample_id,(uintptr_t)p); report("SAMPLE_ROOT_OBSERVED"); }
    note_native_observe_root(p);
    music_ui_tick();
#ifdef MPC_CHORDS
    if(visible(p)&&chords_ui_paint(p,g))return;
#endif
#ifdef MPC_PAD_EQ
    if(visible(p)&&eq_page){unsigned pad=0;if(note_native_pad_index(&pad))eq_paint(g,pad);return;}
    /* The native sample rasteriser is suppressed separately on these pages.
     * Never clear this graph after native paint: that would erase the filter
     * and envelope curves as well. Dim translucent bars keep them visible. */
    if(music_ui_filter_spectrum_visible(p)){
        unsigned pad;if(note_native_pad_index(&pad)){
            struct resample_painter painter={g,eq_rect,eq_text};
            pad_eq_spectrum_paint(pad,sample_pad_muted(pad),15,94,289,94,0x605a4419u,&painter);
        }
    }
#endif
    void *group=(char*)p+0x7f0;
#ifdef MPC_PROJECT_RECOVERY
    const char *restored=project_recovery_notice();
    if(visible(p)&&!visible(project)&&!music_ui_repeat_active()&&restored){
        colour(g,0xff000000);rect(g,0,178,320,19);colour(g,0xffffc247);text(g,restored,4,179,312,17,13);
    }
#endif
    int index=FIELD(group,0x218,int);
    if (!visible(p) || music_ui_repeat_active() || index<0 || index>1) return;
    if (!lights) return;
    char name[MUSIC_NAME_BYTES],value[48]; struct music_sample s;
    int ok=settings(name,&s),shift=shifted(p,0);
    if (!ok) { memset(&s,0,sizeof(s)); s.root=-1; s.tonic=-1; s.scale=-1; }
    const char *modes[MUSIC_LAYOUT_CHOICES]={"Monochrome","Piano","Scale","Degrees"};
    static const char *const scale_roles[]={"Tonic","Scale","Other","Original"};
    static const char *const degree_roles[]={"I Tonic","Key notes","IV","V","Original"};
    const char *const *roles=s.mode==MUSIC_DEGREES?degree_roles:scale_roles;
    int shown_role=role<(int)role_count(&s)?role:(int)role_count(&s)-1;
    if (shift) {
        snprintf(value,sizeof(value),"%+d",s.transpose); cell(g,0,"Transpose16",value,1,1,ok,s.transpose+24,49);
        int c=role_value(&s,role_field(&s,shown_role));
        cell(g,1,"Role",roles[shown_role],1,1,ok && s.mode!=MUSIC_MONO,shown_role,(int)role_count(&s));
        cell(g,2,"Colour",music_colour_names[c],1,1,ok && s.mode!=MUSIC_MONO,c,12);
    } else {
        int project_tonic,project_scale;char tonic_text[32],scale_text[64];
        music_get_key(&project_tonic,&project_scale);
        snprintf(tonic_text,sizeof(tonic_text),s.tonic<0 ? "%s (P)" : "%s",music_note_names[s.tonic<0 ? project_tonic : s.tonic]);
        snprintf(scale_text,sizeof(scale_text),s.scale<0 ? "%s (P)" : "%s",music_scale_names[s.scale<0 ? project_scale : s.scale]);
        cell(g,0,"Layout",modes[music_mode_ui_index(s.mode)],0,1,ok,music_mode_ui_index(s.mode),MUSIC_LAYOUT_CHOICES);
        cell(g,1,"Tonic",tonic_text,0,1,ok,s.tonic+1,13);
        cell(g,2,"Scale",scale_text,0,1,ok,s.scale+1,9);
    }
    if (!ok || s.root<0) {
        colour(g,0xff000000); rect(g,0,180,320,16); colour(g,0xffff4040);
        text(g,"Set Orig Note: Tune > Shift+K1",2,180,316,15,11);
    }
}
static unsigned mod_rows(void){return mod_tab==MOD_ABOUT_TAB?0:mod_tab==2?MUSIC_SERVICE_COUNT:mod_tab==1?3:1;}
static unsigned mod_field(unsigned row){return mod_tab==1?(row==2?MUSIC_SETTING_TAIL:MUSIC_SETTING_WAVE+row):mod_tab==3?MUSIC_SETTING_BLINK:MUSIC_SETTING_LIGHTING;}
static unsigned mod_value(void){return mod_tab==2?music_service_colour(mod_row):(unsigned)music_mod_setting(mod_field(mod_row));}
static void mod_change(unsigned value,int relative){
 if(mod_tab==MOD_ABOUT_TAB||mod_row==mod_rows())return;
 unsigned field=mod_field(mod_row);
 unsigned hi=mod_tab==2?MUSIC_PALETTE_SIZE-1:field==MUSIC_SETTING_TAIL?MUSIC_TAIL_CHOICES-1:mod_tab==3?3:1;
 unsigned next=(unsigned)choice(value,(int)mod_value(),0,(int)hi,relative);
 lighting_save_error=!(mod_tab==2?music_set_service_colour(mod_row,next):music_set_mod_setting(field,next));
 repaint(project);
}
static void text_aligned(void *g,const char *s,int x,int y,int w,int h,float height,int align)
{
    uint64_t string=0,font=0;
    FN(0xf535c0,void(*)(uint64_t*,const char*))(&string,s);
    FN(0x1082810,void(*)(uint64_t*,int,float))(&font,0,height);
    FN(0x102dca0,void(*)(void*,const uint64_t*))(g,&font);
    FN(0x10817b0,void(*)(void*,const uint64_t*,int,int,int,int,int,int,float))
        (g,&string,x,y,w,h,align,1,0.70f);
    FN(0x1070d60,void(*)(uint64_t*))(&font);
    FN(0xf0a290,void(*)(uint64_t*))(&string);
}
/* Mod Settings uses the stock list style of Project / Input Config: label on
 * the left, value on the right, grey fill on the selected row. */
static void mod_settings_paint(void*g){
 static const char*const tabs[]={"Pads","Sample","Colours","Cursor","About"};
 static const char*const titles[]={"Pad lighting","Sample","System colours","Selected pad","About"};
 static const char*const sample_labels[]={"Waveform tint","Spectrum tint","Preview from end"};
 static const char*const services[]={"Cursor / selection","Free target","Occupied target","Original value","Play position","Muted"};
 static const char*const speeds[]={"Steady","Slow","Normal","Fast"};
 colour(g,0xff000000);rect(g,0,0,320,240);colour(g,0xffffffff);
 if(mod_reset_confirm){
  /* Stock confirmation box: B1 Cancel, B3 confirms. */
  text(g,"Cancel",0,6,106,23,14);colour(g,0xff3aa0ffu);text(g,"Restore",214,6,106,23,14);
  colour(g,0xffffffff);rect(g,6,51,308,145);colour(g,0xff000000);rect(g,7,52,306,143);
  colour(g,0xffffffff);text_aligned(g,"Restore recommended",24,66,272,26,18,0x21);rect(g,24,100,272,1);
  colour(g,0xffffc247);text_aligned(g,titles[mod_tab],24,112,272,24,18,0x21);
  colour(g,0xffffffff);text_aligned(g,mod_tab==2?"The whole palette is reset.":"Only this section is reset.",24,144,272,20,13,0x21);
  return;
 }
 char prev[24],next[24];
 snprintf(prev,sizeof(prev),"< %s",tabs[(mod_tab+MOD_TAB_COUNT-1)%MOD_TAB_COUNT]);snprintf(next,sizeof(next),"%s >",tabs[(mod_tab+1)%MOD_TAB_COUNT]);
 text(g,"Back",0,6,106,23,14);text(g,prev,107,6,106,23,14);text(g,next,214,6,106,23,14);
 colour(g,0xffffc247);text_aligned(g,titles[mod_tab],12,34,220,22,16,0x21);
 for(unsigned t=0;t<MOD_TAB_COUNT;++t){colour(g,t==mod_tab?0xffffc247:0xff4a4a4a);rect(g,252+(int)t*12,43,t==mod_tab?9:6,3);}
 if(mod_tab==MOD_ABOUT_TAB){
  colour(g,0xffffffff);text(g,"Am MOD",8,78,304,34,28);
  colour(g,0xffffc247);text(g,"Version " AM_MOD_VERSION,8,116,304,25,17);
  colour(g,0xffa0a0a0);text(g,"Source and updates",8,158,304,18,12);
  colour(g,0xffffffff);text(g,AM_MOD_REPO_URL,5,178,310,23,12);
  return;
 }
 unsigned first=mod_row<5?0:mod_row-4;
 char value_text[24]="";const char*k1_label="",*k1_value="";
 for(unsigned i=first;i<=mod_rows()&&i<first+5;++i){
  int y=62+22*(int)(i-first),current=i==mod_row;
  if(current){colour(g,0xff2c2c2c);rect(g,6,y,308,21);if(mod_edit){colour(g,0xffffc247);rect(g,6,y,3,21);}}
  if(i==mod_rows()){
   colour(g,current?0xffffffff:0xffa0a0a0);text_aligned(g,"Restore recommended",14,y,220,21,13,0x21);
   colour(g,0xffa0a0a0);text_aligned(g,"Press",240,y,66,21,12,0x22);
   if(current){k1_label="Restore";k1_value="Press";}
   continue;
  }
  const char*label=mod_tab==0?"Pad lighting":mod_tab==1?sample_labels[i]:mod_tab==2?services[i]:"Cursor pulse";
  unsigned field=mod_field(i);
  unsigned value=mod_tab==2?music_service_colour(i):(unsigned)music_mod_setting(field);
  char duration[16];
  if(field==MUSIC_SETTING_TAIL)snprintf(duration,sizeof(duration),"%u ms",music_tail_ms(value));
  const char*name=mod_tab==0?(value?"Am MOD":"Standard"):field==MUSIC_SETTING_TAIL?duration:mod_tab==1?(value?"Pad colour":"Original"):mod_tab==2?music_palette_name(value):speeds[value];
  colour(g,0xffffffff);text_aligned(g,label,14,y,190,21,13,0x21);
  colour(g,current&&mod_edit?0xffffc247:0xffffffff);text_aligned(g,name,196,y,110,21,13,0x22);
  if(current){k1_label=label;snprintf(value_text,sizeof(value_text),"%s",name);k1_value=value_text;}
 }
 colour(g,0xffa0a0a0);
 text_aligned(g,mod_tab==2?"Choosing a used colour swaps the two":mod_tab==1?"Hold STOP and tap a pad to hear its end":mod_tab==3?"Sequence pulses still follow tempo":"Standard keeps Piano, Scale and Degrees",12,172,300,13,11,0x21);
 colour(g,lighting_save_error?0xffff4040:0xffa0a0a0);
 text_aligned(g,lighting_save_error?"Could not save settings":"Saved in the project and as card default",12,185,300,13,11,0x21);
 /* K1 changes the selected row directly; the encoder selects and edits. */
 colour(g,0xff000000);rect(g,0,198,320,42);
 colour(g,mod_edit?0xffffc247:0xffffffff);text(g,k1_label,4,200,98,12,12);text(g,k1_value,4,214,98,19,16);
 colour(g,0xffa0a0a0);
 text_aligned(g,mod_row==mod_rows()?"Press: review":mod_edit?"Turn: change   Press: done":"Turn: row   Press: edit",116,206,196,20,13,0x21);
}
void music_ui_project(void *p,void *g)
{
    if (FIELD(p,0,uintptr_t)!=base+PROJECT_VPTR) return;
    if (atomic_exchange(&dead_project,0)) { project=0; key_page=0; }
    if (project!=p) { project=p; key_page=0; atomic_store(&project_id,(uintptr_t)p); report("PROJECT_ROOT_OBSERVED"); }
    music_ui_tick();
    if (!visible(p)) return;
    if(!key_page)return; /* Six native rows: four stock commands, Key / Scale and Mod Settings. */
    if(key_page==5){
        mod_settings_paint(g);return;
    }
    if(key_page==3||key_page==4){
        colour(g,0xff000000);rect(g,0,0,320,240);
        colour(g,key_page==4?0xffff4040:0xffffc247);
        text(g,key_page==4?"Project copy failed":"Saving project copy",4,65,312,28,20);
        colour(g,0xffffffff);
        if(key_page==3){
            text(g,"SD / Projects / Mod-Exit-Last",4,108,312,22,15);
            text(g,"Please keep the card inserted",4,143,312,22,14);
            text(g,"The mod will close after saving",4,177,312,22,13);
            exit_presented=1;
        }else{
            text(g,"Back",0,6,106,23,14);text(g,"Retry",214,6,106,23,14);
            text(g,exit_error,4,109,312,36,12);
            text(g,"The mod is still running",4,170,312,22,14);
        }
        return;
    }
    if(key_page==2){
        colour(g,0xff000000);rect(g,0,0,320,240);colour(g,0xffffffff);text(g,"Cancel",0,6,106,23,14);
        colour(g,0xffff4040);text(g,"Mod Off",214,6,106,23,14);text(g,"Exit the mod?",8,65,304,25,20);
        colour(g,0xffffffff);text(g,"Return to the stock application",8,105,304,20,14);
        text(g,"A project copy is saved to SD as",8,139,304,20,13);text(g,"Projects/Mod-Exit-Last (replaced).",8,163,304,20,13);
        text(g,"B1: cancel  |  B3: confirm",8,202,304,20,13);return;
    }
    int tonic,scale; music_get_key(&tonic,&scale);
    colour(g,0xff000000); rect(g,0,0,320,240);
    colour(g,0xffffffff); text(g,"Back",4,1,98,22,14); text(g,"Key / Scale",30,55,260,28,22);
    char key[48]; music_key_text(tonic,scale,key,sizeof(key));
    colour(g,0xffffc247); text(g,key,10,95,300,26,20);
    colour(g,0xffffffff); text(g,"Default for sample Lights",10,137,300,18,13);
    text(g,"Save the project to keep changes",6,158,308,18,12);
    if (music_archive_status()==2) {
        colour(g,0xffff4040); text(g,"Mod settings could not serialize",2,178,316,16,11);
    }
    cell(g,0,"Tonic",music_note_names[tonic],0,0,1,tonic,12);
    cell(g,1,"Scale",music_scale_names[scale],0,0,1,scale,8);
    cell(g,2,"","",0,0,0,0,0);
}
static int choice(unsigned value,int current,int lo,int hi,int relative)
{
    int n=relative ? current+(value>64 ? (int)value-128 : (int)value) : lo+(int)((value&127)*(unsigned)(hi-lo+1)/128);
    return n<lo ? lo : n>hi ? hi : n;
}
int music_ui_control(void *event)
{ return music_ui_control_at(event,atomic_load(&global_shift)); }
int music_ui_control_at(void *event,unsigned event_shift)
{
    music_ui_tick();
    if(music_pad_lighting_mode()&&atomic_load(&seq_active)&&event_shift&&FIELD(event,8,unsigned char)==0x10){
        int index=sequence_index();if(index>=0)music_set_sequence_colour((unsigned)index,music_colour_choice(FIELD(event,9,unsigned char)*MUSIC_IDENTITY_CHOICES/128u));
        repaint(seq_screen);return 1;
    }
    if (!atomic_load(&custom)) return 0;
    if(key_page==5){
        if(mod_tab==MOD_ABOUT_TAB)return 1;
        if(mod_reset_confirm)return 1;
        unsigned cc=FIELD(event,8,unsigned char),value=FIELD(event,9,unsigned char);
        if(cc==0x10||(cc==0x64&&mod_edit))mod_change(value,cc==0x64);
        else if(cc==0x64){mod_row=(unsigned)choice(value,(int)mod_row,0,(int)mod_rows(),1);repaint(project);}
        return 1;
    }
    if(key_page>=2)return 1;
    unsigned cc=FIELD(event,8,unsigned char),v=FIELD(event,9,unsigned char);
    if (cc!=0x10 && cc!=0x11 && cc!=0x12 && cc!=0x64) return 0;
#ifdef MPC_PAD_EQ
    if(eq_page&&!key_page)return eq_control(cc,v);
#endif
    int rel=cc==0x64;
    if (!rel) focus=(int)cc-0x10;
    if (key_page) {
        if (focus==2) return 1;
        int tonic,scale; music_get_key(&tonic,&scale);
        if (focus==0) tonic=choice(v,tonic,0,11,rel);
        if (focus==1) scale=choice(v,scale,0,7,rel);
        music_set_key(tonic,scale); repaint(project); report("PROJECT_KEY_EDITED"); return 1;
    }
    char name[MUSIC_NAME_BYTES]; struct music_sample s;
    if (!settings(name,&s)) return 1;
    int field,current,lo=0,hi;
    if (shifted(sample,0)) {
        if (focus==0) { field=MUSIC_TRANSPOSE; current=s.transpose; lo=-24; hi=24; }
        else {
            if (s.mode==MUSIC_MONO) return 1;
            int last=(int)role_count(&s)-1;if(role>last)role=last;
            if (focus==1) { role=choice(v,role,0,last,rel); repaint(sample); return 1; }
            field=role_field(&s,role); current=role_value(&s,field); hi=11;
        }
    } else {
        if (focus==0) { field=MUSIC_MODE; current=music_mode_ui_index(s.mode); hi=MUSIC_LAYOUT_CHOICES-1; }
        else if (focus==1) { field=MUSIC_LOCAL_TONIC; current=s.tonic; lo=-1; hi=11; }
        else { field=MUSIC_LOCAL_SCALE; current=s.scale; lo=-1; hi=7; }
    }
    int next=choice(v,current,lo,hi,rel);
    if(field==MUSIC_MODE)next=music_mode_from_ui_index(next);
    if (!music_set_sample(name,field,next)) report("SAMPLE_SETTING_REJECTED");
    note_native_tick(); repaint(sample); return 1;
}
int music_ui_reset_knob(unsigned column,unsigned shift){
 if(column>2||music_ui_repeat_active())return 0;
 if(music_pad_lighting_mode()&&atomic_load(&seq_active)&&shift&&column==0){
  int index=sequence_index();if(index>=0)music_set_sequence_colour((unsigned)index,0);repaint(seq_screen);return 1;
 }
 if(!atomic_load(&custom))return 0;
 if(key_page>=2)return 1; /* No reset of menus, confirmation or About. */
#ifdef MPC_PAD_EQ
 if(eq_page&&!key_page){
  unsigned pad;if(!note_native_pad_index(&pad))return 1;
  uint64_t v=music_get_pad_eq(pad),defaults=pad_eq_default();if(!v)v=defaults;
  if(column==2&&shift)return 1;
  unsigned offset=column==0?0:column==1?(shift?14:7):21;
  uint64_t mask=(column==2?UINT64_C(7):UINT64_C(127))<<offset;
  if(column==2)mask|=UINT64_C(1)<<24; /* Type includes the Bypass choice. */
  music_set_pad_eq(pad,(v&~mask)|(defaults&mask));repaint(sample);return 1;
 }
#endif
 if(key_page==1){int tonic,scale;music_get_key(&tonic,&scale);if(column==0)tonic=0;else if(column==1)scale=0;music_set_key(tonic,scale);repaint(project);return 1;}
 if(lights){
  char name[MUSIC_NAME_BYTES];struct music_sample s;if(!settings(name,&s))return 1;
  int field,value;
  if(shift){if(column==0){field=MUSIC_TRANSPOSE;value=0;}else if(column==1){role=0;repaint(sample);return 1;}else{field=role_field(&s,role);value=role_default(field);}}
  else{field=column==0?MUSIC_MODE:column==1?MUSIC_LOCAL_TONIC:MUSIC_LOCAL_SCALE;value=column==0?MUSIC_MONO:-1;}
  music_set_sample(name,field,value);note_native_tick();repaint(sample);return 1;
 }
 return 0;
}
int music_ui_button(void *event)
{
    unsigned code=FIELD(event,8,unsigned char),down=FIELD(event,9,unsigned char)!=0;
    if (code>=112) return 0;
    if (owned_buttons[code]) {
        if (!down) { owned_buttons[code]=0; atomic_fetch_sub(&held_count,1); }
        return 1;
    }
    music_ui_tick();
    if(key_page==3)return code==0x31?0:1;
    if (!down) return 0;
    if(key_page==4&&visible(project)){
        if(code==0x31)return 0;
        if(code==0x0c){key_page=0;repaint(project);music_ui_tick();}
        else if(code==0x0e){key_page=3;exit_presented=exit_started=0;atomic_store(&exit_blocked,1);repaint(project);}
        return 1;
    }
    /* Note Repeat's B buttons must never open underlying Sample pages. */
    if(music_ui_repeat_active())return 0;
    if(code==0x51||code==0x52)return 0;
    if(code==0x0e&&visible(sample)&&shifted(sample,0)&&!key_page){
        if(workflow_mono_open(sample)){
            return 1; /* The new workflow owns the matching B3 release. */
        }
    }
#ifdef MPC_PAD_EQ
    if(visible(sample)&&!key_page){
        if(eq_page&&(code==0x0c||code==0x0d||code==0x0e)){eq_close();repaint(sample);music_ui_tick();return 0;}
        if(!eq_page&&code==0x0e&&!shifted(sample,0)&&FIELD(sample,0xe50+0x218,int)==1){
            eq_page=1;lights=0;focus=0;owned_buttons[code]=1;atomic_fetch_add(&held_count,1);resample_ui_prime_eq();repaint(sample);music_ui_tick();report("PAD_EQ_OPEN");return 1;
        }
    }
#endif
    if(key_page==5&&visible(project)){
        if(code==0x31)return 0;
        if(mod_reset_confirm){
            if(code==0x0c)mod_reset_confirm=0;
            else if(code==0x0e){lighting_save_error=!music_mod_restore_recommended(mod_tab);mod_reset_confirm=0;}
            repaint(project);
        }
        else if(code==0x0c){key_page=0;repaint(project);music_ui_tick();}
        else if(code==0x0d||code==0x0e){mod_tab=(mod_tab+(code==0x0e?1:MOD_TAB_COUNT-1))%MOD_TAB_COUNT;mod_row=mod_edit=0;repaint(project);}
        else if(code==0x6f&&mod_tab!=MOD_ABOUT_TAB){if(mod_row==mod_rows()){mod_reset_confirm=1;mod_edit=0;}else mod_edit=!mod_edit;repaint(project);}
        owned_buttons[code]=1;atomic_fetch_add(&held_count,1);return 1;
    }
    if(key_page==2&&visible(project)){
        if(code==0x31)return 0;
        if(code==0x0c){key_page=0;repaint(project);music_ui_tick();report("MOD_OFF_CANCEL");}
        else if(code==0x0e){
            key_page=3;exit_presented=exit_started=0;exit_error[0]=0;
            atomic_store(&exit_blocked,1);repaint(project);report("MOD_OFF_SAVE_COPY_REQUESTED");
        }
        owned_buttons[code]=1;atomic_fetch_add(&held_count,1);return 1;
    }
    if (atomic_load(&custom) && code==0x6f) {
        int shift=shifted(key_page ? project : sample,key_page);
        int count=key_page ? 2 : 3;
        if (!key_page && shift
#ifdef MPC_PAD_EQ
            && !eq_page
#endif
        ) {
            char name[MUSIC_NAME_BYTES]; struct music_sample s;
            if (!settings(name,&s) || s.mode==MUSIC_MONO) count=1;
        }
        focus=(focus+(shift ? count-1 : 1))%count;
        owned_buttons[code]=1; atomic_fetch_add(&held_count,1);
        repaint(key_page ? project : sample);
        report(shift ? "FOCUS_PREVIOUS" : "FOCUS_NEXT");
        return 1;
    }
    if (key_page && visible(project)) {
        if (code==0x31) return 0; /* native Shift remains a real modifier */
        if (code==0x0c) { key_page=0; repaint(project); music_ui_tick(); }
        owned_buttons[code]=1; atomic_fetch_add(&held_count,1); return 1;
    }
    if (!visible(sample)) return 0;
    if (lights && code!=0x31 && code!=0x0d) {
        if (code==0x0c || code==0x0e) { lights=0; repaint(sample); music_ui_tick(); return 0; }
        /* Screen and transport buttons keep their native behaviour. */
        return 0;
    }
    /* Layout/Transpose16 settings remain available with Standard lighting.
     * Pad/Seq identity-colour controls have their own mode guards. */
    if (code!=0x0d || shifted(sample,0)) return 0;
    int index=FIELD(sample,0x7f0+0x218,int);
    if (lights) { lights=0; repaint(sample); music_ui_tick(); return 0; }
    if (index!=1) return 0;
    lights=1; focus=0; owned_buttons[code]=1; atomic_fetch_add(&held_count,1); report("LIGHTS_PAGE_OPEN");
    repaint(sample); music_ui_tick(); return 1;
}
