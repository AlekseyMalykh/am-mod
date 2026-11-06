#define _GNU_SOURCE
/* ARM contract check. Real firmware factories, event containers, comparisons,
 * tree traversal and time-signature conversion execute. Only GUI and command
 * execution boundaries are replaced; this is not a device/audio test. */
#include <assert.h>
#include <dlfcn.h>
#include <elf.h>
#include <signal.h>
#include <stdlib.h>
#include <ucontext.h>
#include "src/step_native.c"
#include "src/music_settings.c"
#include "phone_hooks.h"
#ifndef STEP_BRIDGE_PATH
#define STEP_BRIDGE_PATH "build/phone_bridges.bin"
#endif

static unsigned char surface[0x2200],control[0x400],things[0x700],program[0x2900];
static unsigned char seq[0x400],list_storage[0x200],ui_model[0x600],factory[0x80];
static unsigned char undo_manager;
static unsigned char time_list[0x60],sig_node[0x20],clock_object[0x268];
static unsigned char engine[0x800],native_position[0x140],instrument[0x2300],layer[0x1660],sample_data[0x200],sample_refs[0x20],second_refs[0x20],slice_data[0x90];
static uintptr_t slice_objects[8];
static void *instruments[128];
static unsigned char play_facade[0x190],record_state[0xa00],wrong_facade[0x190],wrong_state[0xa00];
static unsigned char bank_control[0x178],bank_list[0x160],playing_interface[0x200];
static uintptr_t playing_vtable[4],midi_vtable[3];
static unsigned char midi_sender[0x78],midi_output[8],last_led[3];
static void *button_property;static unsigned button_down;
static void capture_button_property(void *p,unsigned value){button_property=p;button_down=value;}
static void capture_midi(void *p,void *message,int max)
{
    (void)max;assert(p==midi_output && FIELD(message,0x10,int)==3);
    memcpy(last_led,message,3);
}
static struct step_event window[512],old_command,new_command;
static uintptr_t guard=0x10203040;
static int add_calls,change_calls,delete_calls,group_delete_calls,group_mute_calls,refresh_calls,selected_index;
static unsigned audition_starts,audition_stops;static struct step_event audition_event;
static void audition_on(void*p,const struct step_event*e){assert(p==(void*)atomic_load(&native_audition));audition_event=*e;++audition_starts;}
static void audition_off(void*p){assert(p==(void*)atomic_load(&native_audition));++audition_stops;}
int workflow_event_preview(void*p,const void*e,unsigned key,unsigned mode){assert(p==program&&key<16&&mode<10);audition_event=*(const struct step_event*)e;++audition_starts;return 1;}
void workflow_event_preview_stop(int cancel){assert(cancel==0||cancel==1);++audition_stops;}
unsigned workflow_event_preview_pending(void){return 0;}
static int execution_kind;
static void *last_control_block;
static const char *phase="load";
static struct native_tree *tree(void) {return (struct native_tree*)(list_storage+0x60);}
static void insert_note(const struct step_event *e)
{FN(0x1574460,void(*)(void*,const void*))(tree(),e);}
static int erase_note(const struct step_event *e)
{
    void *end=&tree()->colour;
    void *n=FN(0x27d00f0,void*(*)(void*,int64_t))(tree(),e->time);
    while(n!=end) {
        struct step_event *v=(void*)((char*)n+0x20);
        if(v->time!=e->time) break;
        if(FN(0x19722e0,int(*)(const void*,const void*))(v,e)&255) {
            extern void check_erase_node(void*,void*);
            check_erase_node(tree(),n);return 1;
        }
        n=FN(0xb51738,void*(*)(void*))(n);
    }
    return 0;
}
static void refresh_window(int64_t time)
{
    int64_t unit=FIELD(control,0x78,int64_t),centre=(time/unit)*unit;
    FIELD(control,0x70,int64_t)=centre;
    unsigned n=0;
    void *end=&tree()->colour,*p=FN(0x27d00f0,void*(*)(void*,int64_t))(tree(),centre-unit/2);
    while(p!=end && n<512) {
        struct step_event *e=(void*)((char*)p+0x20);
        if(e->time>centre+unit/2-1) break;
        window[n++]=*e;p=FN(0xb51738,void*(*)(void*))(p);
    }
    FIELD(control,0x280,void*)=window;FIELD(control,0x288,void*)=window+n;
    if(selected_index>=(int)n) selected_index=n ? 0 : -1;
    FIELD(control,0x124,int)=selected_index;
}
static void move_cursor(void *p,int64_t time) {assert(p==control);refresh_window(time);}
static int normalize_index(void *p,const int *v) {(void)p;return *v;}
static void select_index(void *p,int n)
{assert(p==control+0x108);selected_index=n;FIELD(control,0x124,int)=n;}
static int component_visible(void *p) {return p==surface;}
static int component_unblocked(void *p) {(void)p;return 0;}
static void invalidate_screen(void *p,uint64_t a,uint64_t b)
{(void)a;(void)b;assert(p==surface);++refresh_calls;}
static void ctor_change(void *p,void *l,const struct step_event *a,const struct step_event *b,int flags)
{(void)p;assert(l==list_storage && flags==0);old_command=*a;new_command=*b;execution_kind=2;}
static void ctor_delete(void *p,void *l,const struct step_event *a,int flags)
{(void)p;assert(l==list_storage && flags==0);old_command=*a;execution_kind=3;}
static int dispatch_command(void *p,uintptr_t result[2])
{
    assert(result[0] && result[1]);
    void *command=(void*)result[0];
    if(FIELD(command,0,uintptr_t)==base+0x6e41e40){
        assert(p==&undo_manager&&FIELD(command,0x50,void*)==seq);
        FIELD(seq,0x200,int)=FIELD(command,0x58,int);
        return 0;
    }
    last_control_block=(void*)result[1];
    assert(erase_note(&old_command));
    if(execution_kind==2) {insert_note(&new_command);++change_calls;}
    else {assert(execution_kind==3);++delete_calls;}
    return 0;
}
void mute_dispatch(void*p,void*l,void*opaque,uint64_t*title,uint64_t*merge,void*unused,int flags)
{
    (void)p;(void)unused;assert(l==list_storage&&title&&merge&&flags==0);
    struct native_tree*changes=opaque;
    assert(changes->size);void*end=&changes->colour;
    for(void*n=changes->left;n!=end;n=FN(0xb51738,void*(*)(void*))(n)) {
        struct step_event*a=(void*)((char*)n+0x20),*b=(void*)((char*)n+0x58);
        assert(erase_note(a));insert_note(b);
    }
    ++group_mute_calls;
}
extern void check_mute_dispatch(void);
static void release_shared(void *p)
{
    if (p==sample_refs) {assert(FIELD(p,8,int)==2);FIELD(p,8,int)=1;return;}
    assert(p==last_control_block);
    int *refs=(int*)((char*)p+8);assert(*refs>=1);
    if(!--*refs) {free(p);last_control_block=NULL;}
}
void add_dispatch(void *f,void *l,struct native_tree *removed,struct native_tree *added,
                         struct native_tree *changed,uint64_t *title,uint64_t *merge,int flags)
{
    assert(f==factory && l==list_storage && !changed->size);
    assert(title && merge && flags==0);
    if (removed->size) {
        assert(!added->size||added->size==removed->size||(removed->size==1&&added->size<=16));
        void *end=&removed->colour,*p=removed->left;
        while (p!=end) {
            struct step_event e;memcpy(&e,(char*)p+0x20,sizeof(e));assert(erase_note(&e));
            p=FN(0xb51738,void*(*)(void*))(p);
        }
        if(added->size){
            end=&added->colour;p=added->left;
            while(p!=end){struct step_event e;memcpy(&e,(char*)p+0x20,sizeof(e));insert_note(&e);p=FN(0xb51738,void*(*)(void*))(p);}
            ++group_mute_calls;
        }else ++group_delete_calls;
        return;
    }
    assert(added->size>=1&&added->size<=16);
    void *end=&added->colour;
    for(void*n=added->left;n!=end;n=FN(0xb51738,void*(*)(void*))(n))insert_note((void*)((char*)n+0x20));
    ++add_calls;
}
extern void check_add_dispatch(void);
static int fixture_sample_present(void*p){return p!=NULL;}
static void stub(uintptr_t off,void *fn)
{
    uint32_t words[]={0x58000050,0xd61f0200};
    memcpy((void*)(base+off),words,8);memcpy((void*)(base+off+8),&fn,8);
}
static void fault(int sig,siginfo_t *info,void *ctx)
{
    ucontext_t *u=ctx;
    fprintf(stderr,"FAIL %s signal=%d pc=%lx native=%lx addr=%p\n",phase,sig,
        (unsigned long)u->uc_mcontext.pc,(unsigned long)(u->uc_mcontext.pc-base),info->si_addr);
    _exit(1);
}
static void load_elf(const char *path,uintptr_t address)
{
    FILE *f=fopen(path,"rb");assert(f);Elf64_Ehdr h;assert(fread(&h,1,sizeof(h),f)==sizeof(h));
    unsigned char *memory=mmap((void*)address,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(memory==(void*)address);base=address;
    Elf64_Phdr ph[16];assert(h.e_phnum<=16);fseek(f,(long)h.e_phoff,SEEK_SET);assert(fread(ph,sizeof(*ph),h.e_phnum,f)==h.e_phnum);
    Elf64_Dyn *dynamic=NULL;
    for(unsigned i=0;i<h.e_phnum;++i) if(ph[i].p_type==PT_LOAD) {
        assert(ph[i].p_vaddr+ph[i].p_memsz<0x7500000);fseek(f,(long)ph[i].p_offset,SEEK_SET);
        assert(fread(memory+ph[i].p_vaddr,1,ph[i].p_filesz,f)==ph[i].p_filesz);
    }
    for(unsigned i=0;i<h.e_phnum;++i) if(ph[i].p_type==PT_DYNAMIC) dynamic=(void*)(memory+ph[i].p_vaddr);
    assert(dynamic);uintptr_t rela=0,relasz=0,jmp=0,jmpsz=0,str=0,sym=0;
    for(Elf64_Dyn *d=dynamic;d->d_tag;++d) switch(d->d_tag) {
        case DT_RELA:rela=d->d_un.d_ptr;break;case DT_RELASZ:relasz=d->d_un.d_val;break;
        case DT_JMPREL:jmp=d->d_un.d_ptr;break;case DT_PLTRELSZ:jmpsz=d->d_un.d_val;break;
        case DT_STRTAB:str=d->d_un.d_ptr;break;case DT_SYMTAB:sym=d->d_un.d_ptr;break;
    }
    uintptr_t starts[2]={rela,jmp},sizes[2]={relasz,jmpsz};
    for(unsigned k=0;k<2;++k) for(size_t i=0;i<sizes[k]/sizeof(Elf64_Rela);++i) {
        Elf64_Rela *r=(void*)(memory+starts[k]+i*sizeof(*r));uintptr_t *out=(void*)(memory+r->r_offset);
        if(ELF64_R_TYPE(r->r_info)==R_AARCH64_RELATIVE) *out=base+r->r_addend;
        else {
            Elf64_Sym *s=(void*)(memory+sym+ELF64_R_SYM(r->r_info)*sizeof(*s));
            const char *name=(void*)(memory+str+s->st_name);void *fn=dlsym(RTLD_DEFAULT,name);
            if(fn) *out=(uintptr_t)fn+r->r_addend;
        }
    }
    fclose(f);FIELD((void*)base,0x7164398,void*)=&guard;
    stub(0x2804760,fixture_sample_present);
    stub(0x15814a0,move_cursor);stub(0x14e19c0,select_index);
    stub(0x112fd50,component_visible);stub(0x112f6b0,component_unblocked);
    stub(0x1149880,invalidate_screen);
    stub(0x19d81c0,ctor_change);stub(0x19d8ba0,ctor_delete);
    stub(0x19d0a00,dispatch_command);stub(0x123bca0,release_shared);
    stub(0x19500e0,check_add_dispatch);stub(0x194fd60,check_mute_dispatch);
    __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
}
static struct step_event make_note(int pad,int64_t time,float vel)
{
    struct step_event e=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))
        ((unsigned)pad,time,240,100,0,vel);
    return e;
}
static void event_control(unsigned value)
{unsigned char e[16]={0};e[8]=0x64;e[9]=(unsigned char)value;assert(step_native_control(e,step_native_ticket()));}
static void button(unsigned code,int down)
{unsigned char e[16]={0};e[8]=(unsigned char)code;e[9]=(unsigned char)down;step_native_button(e,step_native_ticket());}
static void s_pad(unsigned slot,uint64_t t) {step_native_pad((unsigned)pad_step(slot),t);}
static FILE *draw_file;
static uint32_t draw_ink;
static float draw_height;
static unsigned draw_bars;
static void draw_colour(void *g,uint32_t ink) {(void)g;draw_ink=ink;}
static void draw_rect(void *g,int x,int y,int w,int h)
{
    (void)g;assert(x>=0 && y>=5 && w>=0 && h>=0 && x+w<=320 && y+h<=240);
    assert(!(w==34 && h==25)); /* no mirrored 4x4 cells */
    if(x==217 && y==234 && h==3) ++draw_bars;
    fprintf(draw_file,"R %d %d %d %d %08x\n",x,y,w,h,draw_ink);
}
static void draw_font_make(uint64_t *f,int style,float h) {(void)style;*f=0;draw_height=h;}
static void draw_font_use(void *g,const uint64_t *f) {(void)g;(void)f;}
static void draw_font_end(uint64_t *f) {(void)f;}
static void draw_text(void *g,const uint64_t *s,int x,int y,int w,int h,int flags,int lines,float scale)
{
    (void)g;(void)flags;(void)lines;(void)scale;
    fprintf(draw_file,"T %d %d %d %d %08x %.1f @%x %s\n",x,y,w,h,draw_ink,draw_height,flags,(const char*)(uintptr_t)*s);
}
static void draw_preview(const char *path)
{
    draw_file=fopen(path,"w");assert(draw_file);
    stub(0x102d5e0,draw_colour);stub(0x102e000,draw_rect);
    stub(0x1082810,draw_font_make);stub(0x102dca0,draw_font_use);
    stub(0x1070d60,draw_font_end);stub(0x10817b0,draw_text);
    __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
    steps=want_steps=1;menu=0;focus=3;message[0]=0;step_native_tick();
    step_native_paint(surface,(void*)1);assert(draw_bars==2);
    fclose(draw_file);draw_file=NULL;
    button(0x0d,1);button(0x0d,0);assert(!steps && !want_steps);
}
static void setup(void)
{
    tree_init(tree());root=surface;controller=control;ui_known=1;ui_thread=pthread_self();
    FIELD(surface,0,uintptr_t)=base+SCREEN_VPTR;FIELD(surface,0x108,void*)=control;
    FIELD(surface,0x28,uint64_t)=320|((uint64_t)240<<32);
    FIELD(control,0,uintptr_t)=base+CONTROLLER_VPTR;FIELD(control,0x248,void*)=things;
    FIELD(control,0xc0,void*)=list_storage;FIELD(control,0x240,void*)=factory;
    FIELD(factory,0x60,void*)=&undo_manager;
    FIELD(control,0x250,void*)=ui_model;FIELD(control,0x78,int64_t)=240;
    FIELD(control,0x138,void*)=(void*)1;FIELD(control,0x140,uintptr_t)=(uintptr_t)normalize_index;
    FIELD(things,0x1c8,void*)=seq;FIELD(things,0xb8,void*)=program;FIELD(things,0x88,void*)=wrong_facade;FIELD(wrong_facade,0x188,void*)=wrong_state;
    FIELD(seq,0x348,int64_t)=7680;FIELD(ui_model,0x578,void*)=time_list;
    FIELD(time_list,0x30,void*)=sig_node;FIELD(sig_node,0,void*)=time_list+0x30;
    FIELD(sig_node,0x10,int)=4;FIELD(sig_node,0x14,int)=960;FIELD(sig_node,0x18,int)=0;
    for(int i=0;i<128;++i) {FIELD(program,0x12a8+i*4,int)=i;FIELD(program,0x14a8+i*4,int)=i;}
    atomic_store(&root_identity,(uintptr_t)root);selected_index=-1;refresh_window(0);assert(step_native_tick());
}
int main(int argc,char **argv)
{
    assert(argc==3);struct sigaction a={0};a.sa_sigaction=fault;a.sa_flags=SA_SIGINFO;
    sigaction(SIGSEGV,&a,NULL);sigaction(SIGBUS,&a,NULL);sigaction(SIGILL,&a,NULL);
    load_elf(argv[1],0x1800000000ULL);setup();
    phase="physical Chop input routing, separate from Sample Save";
    _Alignas(8) unsigned char global_controls[0x3000]={0},input_context[0x18]={0},input_event[16]={0};
    FIELD(input_context,0x10,void*)=global_controls;FIELD(input_event,0,void*)=input_context;
    input_event[8]=0x4b;input_event[9]=127;stub(0x13904f0,capture_button_property);
    FN(0x159f2f0,void(*)(void*))(input_event);
    assert(button_property==global_controls+0x2510&&button_down==1&&BUTTON_CHOP==0x4b);
    input_event[8]=0x24;FN(0x159f2f0,void(*)(void*))(input_event);
    assert(button_property==global_controls+0x27d0); /* Save must stay native. */
    phase="generated event label bridge native x24/x8 ABI";
    FILE *bridge_file=fopen(STEP_BRIDGE_PATH,"rb");assert(bridge_file);
    size_t bridge_capacity=PHONE_TABLE_VA-0x6d3d000;
    size_t bridge_size=fread((void*)(base+0x6d3d000),1,bridge_capacity,bridge_file);fclose(bridge_file);
    assert(bridge_size<bridge_capacity&&bridge_size>H_EVENT_LABEL*96);
    FIELD((void*)base,PHONE_TABLE_VA+64+H_EVENT_LABEL*16+8,uintptr_t)=(uintptr_t)step_native_event_label;
    __builtin___clear_cache((char*)base+0x6d3d000,(char*)base+0x6d3d000+bridge_size);
    struct step_event label_event=make_note(2,0,.7f);event_choice_set(&label_event,0,4);
    uint64_t bridge_label=0;extern void check_event_label_bridge(int,const void*,void*,uintptr_t);
    check_event_label_bridge(2,&label_event,&bridge_label,base+0x6d3d000+H_EVENT_LABEL*96);
    assert(!strcmp((const char*)(uintptr_t)bridge_label,"A03 L"));FN(0xf0a290,void(*)(void*))(&bridge_label);
    FIELD((void*)base,PHONE_TABLE_VA+64+H_EVENT_LABEL*16+8,uintptr_t)=0;
    check_event_label_bridge(2,&label_event,&bridge_label,base+0x6d3d000+H_EVENT_LABEL*96);
    assert(!strcmp((const char*)(uintptr_t)bridge_label,"A03"));FN(0xf0a290,void(*)(void*))(&bridge_label);
    phase="native event factory";
    struct step_event e=make_note(2,0,.7f);assert(e.type==3 && e.data[32]==2 && FIELD(&e,0x28,int64_t)==240);
    e.data[0x23]=72; /* Non-default articulation must survive every command. */
    insert_note(&e);assert(tree()->size==1);refresh_window(0);selected_index=0;FIELD(control,0x124,int)=0;
    struct slot page_slots[16];assert(scan_page(0,2,2,page_slots) && page_slots[0].count==1);
    phase="native event move / shared_ptr x8 ABI";
    step_native_requested(0x31,1);event_control(1);
    assert(change_calls==1 && tree()->size==1 && window[0].time==240 && window[0].data[0x23]==72);
    assert(FIELD(&window[0],0x28,int64_t)==240 && FIELD(&window[0],0x10,float)==.7f);
    /* Another note on the target tick remains; the moved event stays selected. */
    struct step_event another=make_note(2,480,.2f);insert_note(&another);
    event_control(1);assert(tree()->size==2 && change_calls==2);
    struct step_event chosen_note;assert(selected_event(&chosen_note) && chosen_note.time==480 && chosen_note.data[0x23]==72);
    phase="wrap across sequence edges";
    refresh_window(0);selected_index=-1;FIELD(control,0x124,int)=-1;event_control(127);
    assert(FIELD(control,0x70,int64_t)==7440);
    struct step_event tail=make_note(5,7440,.8f);insert_note(&tail);refresh_window(7440);
    selected_index=0;FIELD(control,0x124,int)=0;event_control(1);
    assert(selected_event(&chosen_note) && chosen_note.time==0 && chosen_note.data[32]==5);
    event_control(127);assert(selected_event(&chosen_note) && chosen_note.time==7440);
    phase="native additive command / multievent slot";
    struct step_event copy=e;copy.time=480;copy.data[0x23]=75;
    assert(add_event(&copy));assert(add_calls==1 && tree()->size==4);
    assert(scan_page(0,2,2,page_slots) && page_slots[2].count==3);
    phase="physical pads and coherent tickets";
    step_native_requested(0x31,0);button(0x0d,1);button(0x0d,0);assert(step_native_custom());
    sound=2;hold(4);step_native_tick();uint64_t captured=step_native_ticket();
    uint64_t unchanged=tree()->size;s_pad(4,captured);assert(tree()->size==unchanged && cursor==4);
    unsigned rec_led,button_ink;assert(step_native_record_light(&rec_led) && rec_led==0);
    button(BUTTON_RECORD,1);button(BUTTON_RECORD,0);assert(step_native_record_light(&rec_led) && rec_led==1);
    hold(0);button(BUTTON_PLUS,1);button(BUTTON_PLUS,0);assert(page==1 && steps);
    button(BUTTON_MINUS,1);button(BUTTON_MINUS,0);assert(page==0 && steps);
    button(BUTTON_PLAY,1);button(BUTTON_PLAY,0);assert(steps);
    button(BUTTON_STOP,1);button(BUTTON_STOP,0);assert(steps);
    step_native_requested(0x31,1);assert(!step_native_button_owns(BUTTON_PLUS) && !step_native_button_owns(BUTTON_MINUS));
    step_native_requested(0x31,0);hold(4);step_native_tick();captured=step_native_ticket();
    page=1;step_native_tick();s_pad(4,captured); /* original page, not new page */
    assert(scan_page(0,2,2,page_slots) && page_slots[4].count==1);
    s_pad(4,step_native_ticket());assert(delete_calls==0 && scan_page(0,2,2,page_slots) && page_slots[4].count==1);
    step_native_requested(BUTTON_ERASE,1);button(BUTTON_ERASE,1);
    uint64_t erase_light;step_native_tick();assert(step_native_light(pad_step(4),&erase_light) && ((uint32_t)erase_light==PAD_DANGER||(uint32_t)erase_light==PAD_SELECT));
    uint64_t erase_ticket=step_native_ticket();step_native_requested(BUTTON_ERASE,0);button(BUTTON_ERASE,0);
    s_pad(4,erase_ticket);assert(delete_calls==1 && scan_page(0,2,2,page_slots) && !page_slots[4].count);
    s_pad(4,step_native_ticket());assert(scan_page(0,2,2,page_slots) && page_slots[4].count==1);
    uint64_t before=tree()->size;step_native_requested(0x0c,1);button(0x0c,1);
    uint64_t sound_ticket=step_native_ticket();step_native_requested(0x0c,0);button(0x0c,0);
    step_native_pad(7,sound_ticket);assert(sound==7 && tree()->size==before);
    step_native_requested(0x31,1);uint64_t shortcut=step_native_ticket();
    assert(step_native_pad_route(shortcut)==2 && step_native_light(7,&erase_light));
    step_native_pad(7,shortcut);assert(sound==7 && tree()->size==before);
    step_native_requested(0x31,0);assert(step_native_shortcut(shortcut) && want_steps);
    step_native_tick();assert(steps);s_pad(5,captured);assert(tree()->size==before);
    /* Note Repeat and Pad Bank do not change the logical Steps mode. */
    button(0x0b,1);button(0x0b,0);button(0x23,1);button(0x23,0);assert(steps && want_steps);
    step_native_requested(0x31,0);hold(2);sound=2;step_native_tick();
    before=tree()->size;s_pad(2,step_native_ticket());assert(tree()->size==before); /* multi-note not erased */
    /* K1 no longer changes sound; Shift+K1 is deliberately unassigned. */
    unsigned char knob[16]={0};knob[8]=0x10;knob[9]=1;
    int before_bank=browse_bank;
    FIELD(seq,0x200,int)=1;
    step_native_control(knob,step_native_ticket());assert(sound==2&&FIELD(seq,0x200,int)==2);
    knob[9]=127;
    step_native_requested(0x31,1);step_native_control(knob,step_native_ticket());assert(sound==2&&browse_bank==before_bank&&FIELD(seq,0x200,int)==2);
    step_native_requested(0x31,0);
    sound=15;step_native_tick();
    int before_grid=grid,before_velocity=velocity;int old_cursor=cursor;
    event_control(1);assert(sound==15 && grid==before_grid && velocity==before_velocity && cursor==old_cursor+1);
    button(0x6f,1);button(0x6f,0);event_control(127);assert(sound==15 && cursor==old_cursor);
    knob[8]=0x11;knob[9]=127;step_native_control(knob,step_native_ticket());assert(grid==4);
    step_native_requested(0x31,1);step_native_control(knob,step_native_ticket());assert(grid==9 && grid_ticks[grid]==40);
    step_native_requested(0x31,0);knob[9]=64;step_native_control(knob,step_native_ticket());assert(grid==7);
    step_native_requested(0x31,1);knob[9]=0;step_native_control(knob,step_native_ticket());assert(grid==2);
    step_native_requested(0x31,0);sound=2;hold(4);step_native_tick();
    menu=1;menu_index=0;operate();assert(clip_valid && clip_sound==2);
    before=tree()->size;hold(2);step_native_tick();menu=1;menu_index=1;operate();
    assert(tree()->size==before+1 && scan_page(0,2,2,page_slots) && page_slots[2].count==4);
    struct step_event offgrid=make_note(2,7679,.3f);insert_note(&offgrid);
    assert(scan_page(1,2,2,page_slots) && page_slots[15].count==1);
    button(0x0d,1);button(0x0d,0);assert(choose_exact(&offgrid));
    step_native_requested(0x31,1);event_control(1);
    assert(selected_event(&chosen_note) && chosen_note.time==239 && FIELD(&chosen_note,0x10,float)==.3f);
    event_control(127);assert(selected_event(&chosen_note) && chosen_note.time==7679);
    step_native_requested(0x31,0);button(0x0d,1);button(0x0d,0);sound=2;
    phase="Play is independent of record and Steps Write arm";
    hold(4);FIELD(clock_object,0,uintptr_t)=base+NOW_VPTR;FIELD(clock_object,0x258,void*)=engine;
    FIELD(engine,0x790,void*)=native_position;FIELD(engine,0x768,void*)=play_facade;
    FIELD(play_facade,0x188,void*)=record_state;FIELD(play_facade,0x148,void*)=playing_interface;
    playing_vtable[3]=base+0x1445640;FIELD(playing_interface,0,void*)=playing_vtable;
    FIELD(playing_interface,0x1f0,unsigned char)=1;FIELD(record_state,0x9e8,int)=0;
    FIELD(native_position,0x130,int64_t)=960;
    /* The old query is record mode None: true during ordinary Play.
     * Execute both real getters, with no replacement of their semantics. */
    assert(FN(0x2133870,int(*)(void*))(play_facade)==1);
    assert(FN(0x2133800,int(*)(void*))(play_facade)==0);
    step_native_clock(clock_object);step_native_tick();uint64_t light;assert(was_playing);
    assert(step_native_light(pad_step(4),&light) && (uint32_t)light==PAD_PLAYHEAD);
    FIELD(native_position,0x130,int64_t)=1200;step_native_clock(clock_object);step_native_tick();
    assert(step_native_light(pad_step(5),&light) && (uint32_t)light==PAD_PLAYHEAD);
    assert(!atomic_load(&write_armed));uint64_t no_write=tree()->size;
    s_pad(10,step_native_ticket());assert(cursor==10 && tree()->size==no_write);
    FIELD(record_state,0x9e8,int)=1;step_native_clock(clock_object);step_native_tick();assert(was_playing);
    s_pad(10,step_native_ticket());assert(tree()->size==no_write); /* native record is not Write */
    FIELD(playing_interface,0x1f0,unsigned char)=0;
    step_native_clock(clock_object);step_native_tick();assert(!was_playing);
    for(unsigned i=0;i<16;++i) assert(step_native_light(i,&light) && (uint32_t)light!=PAD_PLAYHEAD);
    button(BUTTON_RECORD,1);button(BUTTON_RECORD,0);assert(atomic_load(&write_armed));
    s_pad(10,step_native_ticket());assert(tree()->size==no_write+1); /* stopped Write On */
    s_pad(10,step_native_ticket());assert(tree()->size==no_write+1);
    FIELD(record_state,0x9e8,int)=0;FIELD(playing_interface,0x1f0,unsigned char)=1;
    FIELD(native_position,0x130,int64_t)=1440;step_native_clock(clock_object);step_native_tick();assert(was_playing);
    s_pad(11,step_native_ticket());assert(tree()->size==no_write+2); /* playing Write On */
    button(BUTTON_RECORD,1);button(BUTTON_RECORD,0);assert(!atomic_load(&write_armed));
    s_pad(12,step_native_ticket());assert(tree()->size==no_write+2);
    FIELD(playing_interface,0x1f0,unsigned char)=0;step_native_clock(clock_object);step_native_tick();assert(!was_playing);
    phase="native record LED MIDI message";
    midi_vtable[2]=(uintptr_t)capture_midi;FIELD(midi_output,0,void*)=midi_vtable;FIELD(midi_sender,0x70,void*)=midi_output;
    assert(step_native_record_light(&rec_led) && rec_led==0);
    FN(0x23b0de0,void(*)(void*,unsigned,unsigned))(midi_sender,rec_led,0x50);
    assert(last_led[0]==0xb0 && last_led[1]==0x50 && last_led[2]==0);
    button(BUTTON_RECORD,1);button(BUTTON_RECORD,0);assert(step_native_record_light(&rec_led) && rec_led==1);
    FN(0x23b0de0,void(*)(void*,unsigned,unsigned))(midi_sender,rec_led,0x50);
    assert(last_led[0]==0xb0 && last_led[1]==0x50 && last_led[2]==1);
    button(0x0d,1);button(0x0d,0);assert(!step_native_custom() && !step_native_light(pad_step(4),&light));
    before=tree()->size;s_pad(5,captured);assert(tree()->size==before);
    phase="native program instrument and shared sample name";
    FIELD(program,0x2808,void*)=instruments;FIELD(program,0x2810,void*)=instruments+128;
    sound=2;char label[513];read_sample_name(label);assert(!strcmp(label,"Empty"));
    /* A03 is instrument2 but standard MIDI note38. Name lookup must
     * not read instrument38 (empty), even with a nonidentity pad map. */
    FIELD(program,0x12a8+2*4,int)=38;FIELD(program,0x14a8+38*4,int)=2;
    instruments[2]=instrument;FIELD(instrument,0x22e8,void*)=layer;FIELD(instrument,0x22f0,void*)=layer+sizeof(layer);
    FIELD(layer,0x4a8,void*)=sample_data;FIELD(layer,0x4b0,void*)=sample_refs;FIELD(sample_refs,8,int)=1;
    FN(0xf535c0,void(*)(uint64_t*,const char*))((uint64_t*)(sample_data+0xa8),"Kick 01");
    read_sample_name(label);assert(!strcmp(label,"Kick 01") && FIELD(sample_refs,8,int)==1);
    browse_bank=0;assert(read_sound_presence()==(1u<<2));
    FIELD(sample_refs,8,int)=0;FIELD(layer,0xb30+0x4b0,void*)=second_refs;FIELD(second_refs,8,int)=1;
    assert(read_sound_presence()==(1u<<2)); /* populated second layer, empty first */
    FIELD(second_refs,8,int)=0;FIELD(sample_refs,8,int)=1;
    steps=want_steps=1;sound=7;step_native_requested(0x0c,1);step_native_tick();
    assert(step_native_light(2,&light)&&(uint32_t)light==PAD_CONTENT);
    assert(step_native_light(3,&light)&&(uint32_t)light==0x303030);
    assert(step_native_light(7,&light)&&((uint32_t)light==PAD_SELECT||(uint32_t)light==PAD_EMPTY));
    step_native_requested(0x0c,0);steps=want_steps=0;step_native_tick();
    draw_preview(argv[2]);
    FIELD(sample_refs,8,int)=0;read_sample_name(label);assert(!strcmp(label,"Empty"));
    FN(0xf0a290,void(*)(void*))((void*)(sample_data+0xa8));
    phase="shared native bank selection property";
    FN(0x236f0d0,void(*)(void*))(bank_list+0x108);
    FIELD(bank_list,0x50,int)=8;FIELD(bank_list,0xa8,int)=2;FIELD(bank_list,0x158,int)=0;
    FIELD(bank_control,0,uintptr_t)=base+0x6d8c6f0;FIELD(bank_control,0x140,void*)=bank_list;
    step_native_bank_created(bank_control);assert(native_bank()==0);
    steps=want_steps=1;sound=7;step_native_tick();assert(sound==7);
    FN(0x1a36330,void(*)(void*,int))(bank_list,3);step_native_tick();assert(sound==7 && browse_bank==3 && native_bank()==3);
    step_native_requested(0x0c,1);button(0x0c,1);step_native_pad(2,step_native_ticket());
    step_native_requested(0x0c,0);button(0x0c,0);assert(sound==50); /* explicit D03 selection */
    set_sound_bank(5);step_native_tick();assert(sound==50 && browse_bank==5 && native_bank()==5);
    step_native_requested(0x0c,1);button(0x0c,1);step_native_pad(0,step_native_ticket());
    step_native_requested(0x0c,0);button(0x0c,0);assert(sound==80); /* explicit Sound+Pad selects F01 */
    step_native_bank_destroyed(bank_control);assert(native_bank()==-1);
    FN(0x236f110,void(*)(void*))(bank_list+0x108);
    button(0x0d,1);button(0x0d,0);assert(!steps);
    phase="chronological physical pad order";
    steps=want_steps=1;grid=2;sound=4;hold(10);step_native_tick();
    phase="Erase group: duplicates, off-grid, other sound and neighboring slot";
    struct step_event group=make_note(4,2400,.6f),near=make_note(4,2440,.8f);
    struct step_event neighbour=make_note(4,2640,.5f),other_sound=make_note(3,2400,.4f);
    insert_note(&group);insert_note(&group);insert_note(&near);insert_note(&neighbour);insert_note(&other_sound);
    assert(scan_page(0,4,2,page_slots) && page_slots[10].count==3);
    before=tree()->size;step_native_requested(BUTTON_ERASE,1);s_pad(10,step_native_ticket());
    assert(group_delete_calls==1 && tree()->size==before-3);
    assert(scan_page(0,4,2,page_slots) && !page_slots[10].count && page_slots[11].count==1);
    assert(scan_page(0,3,2,page_slots) && page_slots[10].count==1);
    s_pad(10,step_native_ticket());assert(group_delete_calls==1 && tree()->size==before-3);
    /* A single unquantized event is removed by its complete original value. */
    insert_note(&near);s_pad(10,step_native_ticket());assert(tree()->size==before-3);
    step_native_requested(BUTTON_ERASE,0);
    struct add_context incomplete={0};incomplete.page=0;incomplete.sound=3;incomplete.grid=2;
    incomplete.slot=10;incomplete.expected=2;before=tree()->size;
    assert(!run_set_command(&incomplete) && tree()->size==before && group_delete_calls==1);
    phase="Mute latched/hold and native tag-bit9 grouped replacement";
    /* Execute the real native LED converter: mock RGB receivers otherwise
     * see white's RGB channels without accounting for its dim intensity. */
    uint64_t context_light=pad_light(PAD_CONTEXT,PAD_BRIGHT_CONTEXT);
    uint64_t own_light=pad_light(PAD_CONTENT,PAD_BRIGHT_CONTENT);
    uint64_t cursor_light=pad_light(PAD_SELECT,.8f),empty_light=pad_light(0,0);
    uint32_t context_rgb=FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&context_light);
    uint32_t own_rgb=FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&own_light);
    uint32_t cursor_rgb=FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&cursor_light);
    assert(context_rgb==0x080808&&FN(0x23b00c0,uint32_t(*)(const uint64_t*))(&empty_light)==0);
    assert((own_rgb&255)>10*(context_rgb&255)&&(cursor_rgb&255)>20*(context_rgb&255));
    printf("PASS native LED conversion: other sounds=%06x, own=%06x, cursor=%06x, empty=000000\n",context_rgb,own_rgb,cursor_rgb);
    for(int mute=0;mute<2;++mute)for(int erase=0;erase<2;++erase){
      uint32_t base_colour=erase?PAD_DANGER:mute?PAD_FREE:PAD_CONTENT;
      uint32_t muted_colour=(erase||mute)?PAD_DANGER:PAD_MUTED;
      assert((uint32_t)step_palette_value(1,0,mute,erase,0,0,0)==base_colour);
      assert((uint32_t)step_palette_value(1,1,mute,erase,0,0,0)==muted_colour);
      assert((uint32_t)step_palette_value(1,1,mute,erase,1,0,0)==((erase||mute)?PAD_DANGER:PAD_SELECT));
      assert((uint32_t)step_palette_value(1,1,mute,erase,1,0,400)==muted_colour);
      assert((uint32_t)step_palette_value(1,1,mute,erase,1,1,0)==muted_colour);
      assert((uint32_t)step_palette_value(1,1,mute,erase,1,1,200)==muted_colour);
      assert((uint32_t)step_palette_value(1,0,mute,erase,1,1,0)==(erase?PAD_DANGER:mute?PAD_FREE:PAD_PLAYHEAD));
      assert((uint32_t)step_palette_value(2,1,mute,erase,0,0,0)==base_colour);
      assert((uint32_t)step_palette_value(2,1,mute,erase,0,0,400)==muted_colour);
      if(erase){float on,off;uint64_t a=step_palette_value(1,0,mute,1,0,0,0),b=step_palette_value(1,0,mute,1,0,0,400);
        uint32_t x=(uint32_t)(a>>32),y=(uint32_t)(b>>32);memcpy(&on,&x,4);memcpy(&off,&y,4);assert(on>off);} /* erase blinks */
    }
    struct step_event mute_a=make_note(4,2400,.7f),mute_b=make_note(4,2440,.8f);
    mute_b.type|=0x200;insert_note(&mute_a);insert_note(&mute_b);sound=4;hold(10);step_native_tick();
    assert(scan_page(0,4,2,page_slots)&&page_slots[10].count==2&&page_slots[10].muted==1);
    before=tree()->size;step_native_requested(BUTTON_MUTE,1);button(BUTTON_MUTE,1);
    step_native_requested(BUTTON_MUTE,0);button(BUTTON_MUTE,0);assert(mute_latched&&atomic_load(&mute_active));
    assert(step_native_button_light(BUTTON_MUTE,&button_ink)&&button_ink==2);
    s_pad(10,step_native_ticket());assert(group_mute_calls==1&&tree()->size==before);
    assert(scan_page(0,4,2,page_slots)&&page_slots[10].muted==2);
    palette();assert((uint32_t)atomic_load(&lights[pad_step(10)])==PAD_DANGER);
    int saved_playing=was_playing;int64_t saved_play_tick=play_tick;was_playing=1;play_tick=2400;palette();
    assert(atomic_load(&lights[pad_step(10)])==pad_light(PAD_DANGER,1.f));was_playing=saved_playing;play_tick=saved_play_tick;

    hold(9);step_native_tick();assert(step_native_light(pad_step(10),&light)&&(uint32_t)light==PAD_DANGER);
    s_pad(10,step_native_ticket());assert(group_mute_calls==2&&tree()->size==before);
    assert(scan_page(0,4,2,page_slots)&&page_slots[10].muted==0);
    s_pad(14,step_native_ticket());assert(tree()->size==before&&group_mute_calls==2); /* Write cannot add in mute. */
    step_native_requested(BUTTON_MUTE,1);button(BUTTON_MUTE,1);step_native_requested(BUTTON_MUTE,0);button(BUTTON_MUTE,0);
    assert(!mute_latched&&!atomic_load(&mute_active));
    step_native_requested(BUTTON_MUTE,1);button(BUTTON_MUTE,1);s_pad(10,step_native_ticket());
    step_native_requested(BUTTON_MUTE,0);button(BUTTON_MUTE,0);assert(!mute_latched&&!atomic_load(&mute_active));
    assert(group_mute_calls==3&&tree()->size==before);
    assert(scan_page(0,4,2,page_slots)&&page_slots[10].muted==2);
    /* All properties except the native mute bit remain exactly equal. */
    struct step_event expected=mute_a;expected.type|=0x200;assert(choose_exact(&expected));
    expected=mute_b;assert(choose_exact(&expected));
    assert(scan_page(0,3,2,page_slots)&&!page_slots[10].muted);
    step_native_requested(BUTTON_MUTE,1);button(BUTTON_MUTE,1);mute_down_ms-=301;
    step_native_requested(BUTTON_MUTE,0);button(BUTTON_MUTE,0);assert(!atomic_load(&mute_active));
    step_native_requested(BUTTON_MUTE,1);button(BUTTON_MUTE,1);
    step_native_requested(BUTTON_MUTE,0);button(BUTTON_MUTE,0);assert(mute_latched);
    step_native_requested(BUTTON_MUTE,1);button(BUTTON_MUTE,1);mute_down_ms-=301;
    step_native_requested(BUTTON_MUTE,0);button(BUTTON_MUTE,0);assert(!mute_latched&&!atomic_load(&mute_active));
    unsigned char mute_event[16]={0};mute_event[8]=BUTTON_MUTE;mute_event[9]=1;
    step_native_button_at(mute_event,step_native_ticket(),1000);
    uint64_t stale_mute=step_native_ticket();mute_event[9]=0;
    step_native_button_at(mute_event,step_native_ticket(),1401);assert(!atomic_load(&mute_active)&&!mute_latched);
    int old_mutes=group_mute_calls;s_pad(10,stale_mute);assert(group_mute_calls==old_mutes);
    phase="Events staged Tune/Chop pad picker and Shift K3";
    button(0x0d,1);button(0x0d,0);assert(!steps);
    struct step_event tuned=make_note(6,3120,.7f);
    event_choice_set(&tuned,0,5);assert(event_choice_value(&tuned,0)==5 && event_choice_kind(&tuned)==0);
    insert_note(&tuned);assert(choose_exact(&tuned));before=tree()->size;
    int changes=change_calls;
    button(0x0e,0);assert(!menu && change_calls==changes&&tree()->size==before);
    assert(step_native_button_light(BUTTON_LEVELS,&button_ink) && button_ink==2);
    assert(step_native_button_light(BUTTON_CHOP,&button_ink) && button_ink==0);
    assert(!step_native_button_light(0,&button_ink)); /* native mode/SEQ indicators untouched */
    button(BUTTON_LEVELS,1);button(BUTTON_LEVELS,0);assert(menu==3 && picker_mode==1);
    uint64_t pad_label=0;step_native_event_label(6,&tuned,&pad_label);
    assert(!strcmp((const char*)(uintptr_t)pad_label,"A07 L"));FN(0xf0a290,void(*)(void*))(&pad_label);
    char picker_draw[1024];snprintf(picker_draw,sizeof(picker_draw),"%s.picker",argv[2]);
    draw_file=fopen(picker_draw,"w");assert(draw_file);step_native_paint(surface,(void*)1);fclose(draw_file);draw_file=NULL;
    assert(step_native_pad_route(step_native_ticket())==1);
    int chosen_pitch=picker_base+9;step_native_pad(9,step_native_ticket());
    assert(change_calls==changes && tree()->size==before && event_choice_value(&detail_edit,0)==chosen_pitch);
    assert(selected_event(&chosen_note) && event_choice_value(&chosen_note,0)==5);
    button(0x0e,1);button(0x0e,0);assert(!menu && change_calls==changes+1 && tree()->size==before);
    assert(selected_event(&chosen_note) && event_choice_value(&chosen_note,0)==chosen_pitch);
    assert(chosen_note.time==3120 && FIELD(&chosen_note,0x10,float)==.7f && FIELD(&chosen_note,0x28,int64_t)==240);
    button(BUTTON_LEVELS,1);button(BUTTON_LEVELS,0);step_native_pad(0,step_native_ticket());
    button(0x0c,1);button(0x0c,0);assert(!menu && change_calls==changes+1);
    phase="Tune chord draft toggles pads, Cancel preserves, Apply creates independent events";
    struct step_event before_chord=chosen_note;
    button(BUTTON_LEVELS,1);button(BUTTON_LEVELS,0);
    step_native_pad(0,step_native_ticket());step_native_pad(4,step_native_ticket());step_native_pad(7,step_native_ticket());
    assert(picker_note_count==3&&tree()->size==before);
    step_native_pad(4,step_native_ticket());assert(picker_note_count==2&&tree()->size==before);
    step_native_pad(4,step_native_ticket());assert(picker_note_count==3);
    struct step_event chord_notes[3];memcpy(chord_notes,picker_notes,sizeof(chord_notes));
    button(0x0e,1);button(0x0e,0);assert(!menu&&tree()->size==before+2);
    for(unsigned i=0;i<3;++i){assert(choose_exact(chord_notes+i)&&selected_event(&chosen_note));assert(chosen_note.time==3120&&chosen_note.data[0x20]==tuned.data[0x20]);}
    for(unsigned i=0;i<3;++i)assert(erase_note(chord_notes+i));
    insert_note(&before_chord);assert(choose_exact(&before_chord));
    phase="Empty Steps Tune chord draft: no temporary event, Apply inserts separate notes";
    int saved_cursor=cursor,saved_page=page,saved_steps=steps,saved_want=want_steps;
    unsigned saved_write=atomic_load(&write_armed);steps=want_steps=1;atomic_store(&write_armed,1);
    int empty=-1;struct slot empty_slots[16];
    for(int i=0;i<count();++i){assert(scan_page(i/16,sound,grid,empty_slots));if(!empty_slots[i%16].count){empty=i;break;}}
    assert(empty>=0);hold(empty);step_native_tick();before=tree()->size;
    button(BUTTON_LEVELS,1);button(BUTTON_LEVELS,0);assert(menu==3&&picker_insert&&tree()->size==before);
    step_native_pad(2,step_native_ticket());step_native_pad(5,step_native_ticket());assert(picker_note_count==2&&tree()->size==before);
    struct step_event inserted_chord[2];memcpy(inserted_chord,picker_notes,sizeof(inserted_chord));
    button(0x0e,1);button(0x0e,0);assert(!menu&&tree()->size==before+2);
    for(unsigned i=0;i<2;++i){assert(choose_exact(inserted_chord+i));assert(inserted_chord[i].time==(int64_t)empty*grid_ticks[grid]);assert(erase_note(inserted_chord+i));}
    hold(empty);step_native_tick();button(BUTTON_LEVELS,1);button(BUTTON_LEVELS,0);assert(picker_insert);
    step_native_pad(3,step_native_ticket());button(0x0c,1);button(0x0c,0);assert(!menu&&tree()->size==before);
    steps=saved_steps;want_steps=saved_want;cursor=saved_cursor;page=saved_page;atomic_store(&write_armed,saved_write);
    assert(choose_exact(&before_chord));step_native_tick();
    before=tree()->size;
    phase="single-choice and non-Tune picker stay compatible";
    button(BUTTON_LEVELS,1);button(BUTTON_LEVELS,0);
    unsigned char type_knob[16]={0};type_knob[8]=0x10;type_knob[9]=0;
    step_native_control(type_knob,step_native_ticket());assert(menu==3&&picker_mode==0);
    step_native_pad(4,step_native_ticket());assert(change_calls==changes+1&&FIELD(&detail_edit,0x10,float)!=.7f);
    button(0x0c,1);button(0x0c,0);assert(!menu&&selected_event(&chosen_note)&&FIELD(&chosen_note,0x10,float)==.7f);
    unsigned char quick[16]={0};quick[8]=0x12;quick[9]=64;
    step_native_requested(0x31,1);assert(step_native_owns(0x12));
    step_native_control(quick,step_native_ticket());step_native_requested(0x31,0);
    assert(selected_event(&chosen_note) && event_choice_value(&chosen_note,0)==5);
    struct step_event chopped=make_note(7,3360,.9f);
    event_choice_set(&chopped,15,3);assert(event_choice_kind(&chopped)==15 && event_choice_value(&chopped,15)==3);
    insert_note(&chopped);assert(choose_exact(&chopped));changes=change_calls;
    step_native_event_label(7,&chopped,&pad_label);assert(!strcmp((const char*)(uintptr_t)pad_label,"A08 C"));
    FN(0xf0a290,void(*)(void*))(&pad_label);
    struct step_event both=chopped;event_choice_set(&both,0,5);step_native_event_label(7,&both,&pad_label);
    assert(!strcmp((const char*)(uintptr_t)pad_label,"A08 CL"));FN(0xf0a290,void(*)(void*))(&pad_label);
    step_native_tick();assert(step_native_button_light(BUTTON_CHOP,&button_ink) && button_ink==2);
    assert(step_native_button_light(BUTTON_LEVELS,&button_ink) && button_ink==0);
    button(BUTTON_CHOP,1);button(BUTTON_CHOP,0);assert(menu==3 && picker_mode==8);
    step_native_pad(6,step_native_ticket());assert(event_choice_value(&detail_edit,15)==7 && change_calls==changes);
    button(0x0e,1);button(0x0e,0);assert(!menu && change_calls==changes+1);
    assert(selected_event(&chosen_note) && event_choice_value(&chosen_note,15)==7 && chosen_note.data[0x23]==chopped.data[0x23]);
    /* Slice availability on the source does not label an ordinary event. */
    instruments[7]=instrument;FIELD(sample_refs,8,int)=1;FIELD(sample_data,0x130,void*)=slice_data;
    FIELD(slice_data,0x70,void*)=slice_objects;FIELD(slice_data,0x78,void*)=slice_objects+8;
    struct step_event no_slice=make_note(7,3600,.5f);insert_note(&no_slice);assert(choose_exact(&no_slice));
    step_native_tick();assert(event_slices(&no_slice)==8&&step_native_button_light(BUTTON_CHOP,&button_ink)&&button_ink==0);
    assert(step_native_button_light(BUTTON_LEVELS,&button_ink)&&button_ink==0);
    assert(step_native_button_owns(BUTTON_CHOP)&&step_native_button_owns(BUTTON_LEVELS));
    step_native_event_label(7,&no_slice,&pad_label);assert(!strcmp((const char*)(uintptr_t)pad_label,"A08"));
    FN(0xf0a290,void(*)(void*))(&pad_label);
    button(BUTTON_CHOP,1);button(BUTTON_CHOP,0);assert(!menu);
    button(BUTTON_LEVELS,1);button(BUTTON_LEVELS,0);assert(!menu);
    struct step_event sliced=no_slice;event_choice_set(&sliced,15,2);insert_note(&sliced);assert(choose_exact(&sliced));
    step_native_tick();button(BUTTON_CHOP,1);button(BUTTON_CHOP,0);assert(menu==3&&picker_mode==8&&picker_chop);
    struct step_event bounded=detail_edit;step_native_pad(15,step_native_ticket());
    assert(!memcmp(&bounded,&detail_edit,sizeof(bounded)));
    button(0x0c,1);button(0x0c,0);assert(!menu);
    instruments[7]=NULL;FIELD(sample_refs,8,int)=0;
    phase="Steps contextual C/L picker, selection list, stale source and bounds";
    struct step_event step_tune=make_note(11,1200,.6f);event_choice_set(&step_tune,0,4);insert_note(&step_tune);
    struct step_event step_chop=make_note(11,1440,.7f);event_choice_set(&step_chop,15,2);insert_note(&step_chop);
    struct step_event step_plain=make_note(11,1680,.5f);insert_note(&step_plain);
    struct step_event mix_tune=step_tune;mix_tune.time=1920;insert_note(&mix_tune);
    struct step_event mix_chop=step_chop;mix_chop.time=1920;insert_note(&mix_chop);
    struct step_event mix_plain=step_plain;mix_plain.time=1920;insert_note(&mix_plain);
    button(0x0d,1);button(0x0d,0);sound=11;grid=2;hold(5);step_native_tick();assert(steps);
    assert(step_native_button_light(BUTTON_LEVELS,&button_ink)&&button_ink==2);
    assert(step_native_button_light(BUTTON_CHOP,&button_ink)&&button_ink==0);
    changes=change_calls;button(BUTTON_LEVELS,1);button(BUTTON_LEVELS,0);
    assert(steps&&menu==3&&(step_native_ticket()&T_PICKER)&&detail_old.time==1200);
    step_native_pad(9,step_native_ticket());button(0x0e,1);button(0x0e,0);
    assert(steps&&!menu&&change_calls==changes+1&&cursor==5&&sound==11);
    struct step_event second_tune=step_tune;second_tune.type|=0x200;event_choice_set(&second_tune,0,9);insert_note(&second_tune);
    step_native_tick();button(BUTTON_LEVELS,1);button(BUTTON_LEVELS,0);
    assert(menu==4&&picker_source_count==2&&steps);
    before=tree()->size;step_native_pad(0,step_native_ticket());assert(tree()->size==before&&menu==4);
    event_control(1);assert(picker_source_index==1);
    char chooser_draw[1024];snprintf(chooser_draw,sizeof(chooser_draw),"%s.chooser",argv[2]);
    draw_file=fopen(chooser_draw,"w");assert(draw_file);step_native_paint(surface,(void*)1);fclose(draw_file);draw_file=NULL;
    struct step_event selected_source=picker_sources[1];changes=change_calls;
    button(0x0e,1);button(0x0e,0);assert(menu==3&&!memcmp(&detail_old,&selected_source,sizeof(selected_source)));
    step_native_pad(2,step_native_ticket());button(0x0c,1);button(0x0c,0);
    assert(steps&&!menu&&change_calls==changes&&cursor==5&&sound==11);
    button(BUTTON_LEVELS,1);button(BUTTON_LEVELS,0);assert(menu==4);
    struct step_event untouched=picker_sources[0];event_control(1);selected_source=picker_sources[1];
    button(0x0e,1);button(0x0e,0);step_native_pad(12,step_native_ticket());
    struct step_event applied_source=detail_edit;
    button(0x0e,1);button(0x0e,0);assert(steps&&!menu&&change_calls==changes+1);
    assert(choose_exact(&untouched)&&choose_exact(&applied_source)&&event_muted(&applied_source)==event_muted(&selected_source));
    assert(FIELD(&applied_source,0x10,float)==FIELD(&selected_source,0x10,float)&&applied_source.time==selected_source.time);
    changes=change_calls;
    button(BUTTON_LEVELS,1);button(BUTTON_LEVELS,0);assert(menu==4);
    selected_source=picker_sources[0];assert(erase_note(&selected_source));
    button(0x0e,1);button(0x0e,0);assert(!menu&&change_calls==changes); /* stale snapshot cannot edit another */
    hold(6);step_native_tick();assert(step_native_button_light(BUTTON_CHOP,&button_ink)&&button_ink==2);
    button(BUTTON_CHOP,1);button(BUTTON_CHOP,0);assert(menu==3&&picker_chop&&detail_old.time==1440);
    button(0x0d,1);button(0x0d,0);assert(steps&&!menu&&cursor==6); /* cancel stays in Steps */
    hold(7);step_native_tick();assert(!atomic_load(&event_edit_mask));
    button(BUTTON_CHOP,1);button(BUTTON_CHOP,0);button(BUTTON_LEVELS,1);button(BUTTON_LEVELS,0);assert(steps&&!menu);
    hold(8);step_native_tick();assert(atomic_load(&event_edit_mask)==3);
    /* Follow may show page 2 while edit selection remains on page 1. */
    follow=1;atomic_store(&clock_running,1);atomic_store(&clock_valid,1);atomic_store(&clock_tick,6000);
    step_native_tick();assert(page==1&&cursor==8&&atomic_load(&event_edit_mask)==3);
    assert(step_native_button_light(BUTTON_CHOP,&button_ink)&&button_ink==2);
    assert(step_native_button_light(BUTTON_LEVELS,&button_ink)&&button_ink==2);
    assert(collect_picker_sources(15)&&picker_source_count==1&&picker_sources[0].time==1920);
    assert(collect_picker_sources(0)&&picker_source_count==1&&picker_sources[0].time==1920);
    button(BUTTON_CHOP,1);button(BUTTON_CHOP,0);assert(menu==3&&detail_old.time==1920);
    button(0x0c,1);button(0x0c,0);assert(!menu);
    hold(7);step_native_tick();assert(!atomic_load(&event_edit_mask));
    assert(step_native_button_light(BUTTON_CHOP,&button_ink)&&!button_ink);
    assert(step_native_button_light(BUTTON_LEVELS,&button_ink)&&!button_ink);
    follow=0;atomic_store(&clock_running,0);hold(8);step_native_tick();
    button(BUTTON_CHOP,1);button(BUTTON_CHOP,0);assert(menu==3&&detail_old.time==1920&&event_categories(&detail_old)==1);
    button(0x0c,1);button(0x0c,0);
    button(BUTTON_LEVELS,1);button(BUTTON_LEVELS,0);assert(menu==3&&event_categories(&detail_old)==2);
    button(0x0c,1);button(0x0c,0);
    for(unsigned i=0;i<65;++i){struct step_event many=step_tune;many.time=2160;FIELD(&many,0x10,float)=.1f+i*.001f;insert_note(&many);}
    hold(9);step_native_tick();before=tree()->size;button(BUTTON_LEVELS,1);button(BUTTON_LEVELS,0);
    assert(!menu&&tree()->size==before&&change_calls==changes);
    struct step_event picker_tail=step_chop;picker_tail.time=7679;insert_note(&picker_tail);hold(count()-1);step_native_tick();
    assert(collect_picker_sources(15)&&picker_source_count==1&&picker_sources[0].time==7679);
    button(0x0d,1);button(0x0d,0);assert(!steps);assert(choose_exact(&sliced));
    phase="Events B3 row deletion vs native Erase+pad confirmation handoff";
    int deletes=delete_calls;before=tree()->size;
    step_native_requested(BUTTON_ERASE,1);button(BUTTON_ERASE,1);
    uint64_t chord=step_native_ticket();assert(step_native_pad_route(chord)==3 && step_native_shortcut(chord));
    step_native_requested(BUTTON_ERASE,0);button(BUTTON_ERASE,0);
    assert(delete_calls==deletes && tree()->size==before); /* native chord owns whole-sequence action */
    step_native_requested(BUTTON_ERASE,1);button(BUTTON_ERASE,1);
    assert(delete_calls==deletes); /* do not delete before chord can form */
    step_native_requested(BUTTON_ERASE,0);button(BUTTON_ERASE,0);
    assert(delete_calls==deletes && tree()->size==before); /* Erase tap is only the native modifier. */
    button(0x0e,1);button(0x0e,0);assert(delete_calls==deletes+1&&tree()->size==before-1);
    button(0x0e,0);assert(delete_calls==deletes+1); /* Release cannot delete a second row. */
    /* Moving selection while Erase is held never deletes a row either. */
    struct step_event stale=tuned;event_choice_set(&stale,0,25);assert(!choose_exact(&stale));
    assert(choose_exact(&neighbour));step_native_requested(BUTTON_ERASE,1);button(BUTTON_ERASE,1);
    event_control(1);step_native_requested(BUTTON_ERASE,0);button(BUTTON_ERASE,0);
    assert(delete_calls==deletes+1);
    phase="All native 16 Levels modes, not Tune-only";
    struct step_event plain=make_note(6,4000,.8f);
    for(int m=0;m<10;++m) {
        struct step_event candidate;level_candidate(&plain,&candidate,m,6,m==1 ? -3 : m==8 ? 1 : 0);
        step_native_levels_state(1,(unsigned)m,3);
        assert(event_level_mode(&candidate)==m);
        assert(candidate.time==plain.time && candidate.data[0x20]==plain.data[0x20]);
        assert(FIELD(&candidate,0x28,int64_t)==FIELD(&plain,0x28,int64_t));
        if(m) assert(FIELD(&candidate,0x10,float)==FIELD(&plain,0x10,float));
        if(m!=9) assert(candidate.data[0x23]==plain.data[0x23]);
        assert(level_nearest(&candidate,m,m==1 ? -3 : m==8 ? 1 : 0)>=0);
    }
    phase="Ordinary 16 Levels playback lights without a Step Edit component";
    _Alignas(8) unsigned char play_clip[0x900]={0},play_instrument[0x80]={0};
    struct native_tree *play_tree=(void*)(play_clip+0x688+0x60);tree_init(play_tree);
    struct step_event sounding=make_note(6,100,.8f);event_choice_set(&sounding,0,2);
    FN(0x1574460,void(*)(void*,const void*))(play_tree,&sounding);
    void *saved_root=root,*saved_controller=controller;root=controller=NULL;showing=0;steps=1;ui_known=0;atomic_store(&ticket,0);
    instruments[6]=play_instrument;FIELD(things,0x668,void*)=play_instrument;FIELD(things,0x3d0,void*)=play_clip;
    atomic_store(&clock_running,1);atomic_store(&clock_valid,1);atomic_store(&clock_tick,0);
    step_native_levels_state(1,1,3);step_native_playback_context(things,0,1);
    atomic_store(&clock_tick,105);step_native_playback_context(things,0,1);
    assert(atomic_load(&playback_mask)==(1u<<5));
    assert(ui_known); /* No Step Edit paint was needed to initialize UI. */
    assert(step_native_light(5,&light) && (uint32_t)light==PAD_PLAYHEAD);
    phase="Simultaneous Tune events illuminate every chord note";
    step_native_native_mute(1); /* Native Mute LED can be lit while Levels is active. */
    struct step_event simultaneous=sounding;event_choice_set(&simultaneous,0,6);
    FN(0x1574460,void(*)(void*,const void*))(play_tree,&simultaneous);
    atomic_store(&clock_running,0);step_native_playback_context(things,0,1);
    atomic_store(&clock_running,1);atomic_store(&clock_tick,90);step_native_playback_context(things,0,1);
    atomic_store(&clock_tick,105);step_native_playback_context(things,0,1);
    assert(atomic_load(&playback_mask)==((1u<<5)|(1u<<9)));
    extern void check_erase_node(void*,void*);
    void*chord_node=FN(0x27d00f0,void*(*)(void*,int64_t))(play_tree,100);
    while(chord_node!=&play_tree->colour){
        struct step_event*e=(void*)((char*)chord_node+0x20);
        if(event_choice_value(e,0)==6){check_erase_node(play_tree,chord_node);break;}
        chord_node=FN(0xb51738,void*(*)(void*))(chord_node);
    }
    atomic_store(&clock_running,0);step_native_playback_context(things,0,1);
    assert(!atomic_load(&playback_mask) && !step_native_light(5,&light));
    struct step_event vel_event;level_candidate(&plain,&vel_event,0,9,0);vel_event.time=200;
    FN(0x1574460,void(*)(void*,const void*))(play_tree,&vel_event);
    atomic_store(&clock_running,1);atomic_store(&clock_tick,150);step_native_levels_state(1,0,3);
    step_native_playback_context(things,0,1);atomic_store(&clock_tick,201);step_native_playback_context(things,0,1);
    assert(atomic_load(&playback_mask)==(1u<<9));
    struct step_event chop_event=make_note(6,300,.8f);event_choice_set(&chop_event,15,4);
    FN(0x1574460,void(*)(void*,const void*))(play_tree,&chop_event);
    step_native_levels_state(1,8,3);atomic_store(&clock_tick,250);step_native_playback_context(things,0,1);
    atomic_store(&clock_tick,301);step_native_playback_context(things,0,1);assert(atomic_load(&playback_mask)==(1u<<3));
    phase="ordinary Sample bank pulses and each native Levels mode over wrap";
    FIELD(bank_list,0x158,int)=0;
    step_native_bank_created(bank_control);
    step_native_levels_state(0,0,3);atomic_store(&clock_tick,90);step_native_playback_context(things,0,1);
    atomic_store(&clock_tick,105);step_native_playback_context(things,0,1);
    assert(atomic_load(&playback_mask)==(1u<<6)); /* source, not pitch key */
    phase="Muted recorded events flash grey after leaving Mute mode";
    play_instrument[0x78]=1;step_native_native_mute(0);step_native_playback_context(things,0,1);
    assert(atomic_load(&playback_mask)==(1u<<6));
    atomic_store(&clock_tick,90);step_native_playback_context(things,0,1);
    atomic_store(&clock_tick,105);step_native_playback_context(things,0,1);assert(atomic_load(&playback_muted_mask)==(1u<<6));assert(step_native_light(6,&light)&&(uint32_t)light==PAD_MUTED);
    play_instrument[0x78]=0;
    phase="Native Mute shows audible source pulses, muted source stays red";
    step_native_native_mute(1);
    atomic_store(&clock_tick,90);step_native_playback_context(things,0,1);
    atomic_store(&clock_tick,105);step_native_playback_context(things,0,1);
    assert(atomic_load(&playback_mask)==(1u<<6));assert(step_native_light(6,&light)&&(uint32_t)light==PAD_FREE);
    play_instrument[0x78]=1;step_native_playback_context(things,0,1);
    assert(atomic_load(&playback_muted_mask)==(1u<<6));assert(step_native_light(6,&light)&&(uint32_t)light==PAD_DANGER);
    play_instrument[0x78]=0;step_native_native_mute(0);
    step_native_native_mute(1);step_native_playback_context(things,0,0);assert(!atomic_load(&playback_mask));step_native_native_mute(0);
    phase="Seq Mute source pulses; ordinary Seq remains dark";
    step_native_levels_state(1,1,3);step_native_native_mute(1);
    atomic_store(&clock_tick,90);step_native_playback_context(things,0,2);
    atomic_store(&clock_tick,105);step_native_playback_context(things,0,2);
    assert(atomic_load(&playback_mask)==(1u<<6)); /* sound bank, not Tune key */
    play_instrument[0x78]=1;step_native_playback_context(things,0,2);assert(atomic_load(&playback_muted_mask)==(1u<<6));assert(step_native_light(6,&light)&&(uint32_t)light==PAD_MUTED);
    play_instrument[0x78]=0;step_native_native_mute(0);step_native_playback_context(things,0,2);assert(!atomic_load(&playback_mask));
    /* Exit while transport is still playing; clear the old note pulse
     * even when no new event has occurred at the transition tick. */
    step_native_levels_state(1,1,3);atomic_store(&clock_tick,90);step_native_playback_context(things,0,1);
    atomic_store(&clock_tick,105);step_native_playback_context(things,0,1);assert(atomic_load(&playback_mask)==(1u<<5));
    step_native_levels_state(0,1,(unsigned)-1);atomic_store(&clock_tick,106);step_native_playback_context(things,0,1);
    assert(!atomic_load(&native_levels_enabled)&&!atomic_load(&playback_mask)&&step_native_light(5,&light)&&(uint32_t)light==0);
    atomic_store(&clock_tick,201);step_native_playback_context(things,0,1);assert(atomic_load(&playback_mask)==(1u<<6));
    step_native_levels_state(1,8,3);atomic_store(&clock_tick,250);step_native_playback_context(things,0,1);
    atomic_store(&clock_tick,301);step_native_playback_context(things,0,1);assert(atomic_load(&playback_mask)==(1u<<3));
    step_native_levels_state(0,8,3);atomic_store(&clock_tick,302);step_native_playback_context(things,0,1);
    assert(!atomic_load(&playback_mask)&&step_native_light(3,&light)&&(uint32_t)light==0);
    atomic_store(&clock_tick,90);step_native_playback_context(things,0,1);
    atomic_store(&clock_tick,105);step_native_playback_context(things,0,1);assert(atomic_load(&playback_mask)==(1u<<6));
    step_native_playback_context(things,0,0);assert(!atomic_load(&playback_mask)); /* hidden Sample */
    step_native_playback_context(things,0,1);atomic_store(&clock_tick,301);step_native_playback_context(things,0,1);
    assert(atomic_load(&playback_mask)==(1u<<6)); /* Slice on same source */
    FIELD(bank_list,0x158,int)=1;step_native_playback_context(things,0,1);
    atomic_store(&clock_tick,302);step_native_playback_context(things,0,1);assert(!atomic_load(&playback_mask));
    FIELD(bank_list,0x158,int)=0;
    for(int m=0;m<10;++m){
        if(play_tree->parent)FN(0x157df10,void(*)(void*))(play_tree->parent);
        tree_init(play_tree);
        struct step_event candidate;level_candidate(&sounding,&candidate,m,5,m==1?-3:m==8?1:0);candidate.time=0;
        FN(0x1574460,void(*)(void*,const void*))(play_tree,&candidate);
        struct step_event muted=candidate;muted.type|=0x200;muted.time=10;
        FN(0x1574460,void(*)(void*,const void*))(play_tree,&muted);
        step_native_levels_state(1,(unsigned)m,3);
        /* Reset on Stop, then cross end->start on the next playback loop. */
        atomic_store(&clock_running,0);step_native_playback_context(things,0,1);
        atomic_store(&clock_running,1);atomic_store(&clock_tick,7670);step_native_playback_context(things,0,1);
        atomic_store(&clock_tick,1);step_native_playback_context(things,0,1);
        int expected=level_nearest(&candidate,m,m==1?-3:m==8?1:0);
        assert(atomic_load(&playback_mask)==(1u<<expected));
        memset(playback_until,0,sizeof(playback_until));atomic_store(&playback_mask,0);
        atomic_store(&clock_tick,11);step_native_playback_context(things,0,1);assert(!atomic_load(&playback_mask));
    }
    /* Deterministic Sample focus and muted-event UI snapshot, no audio mutation. */
    void *saved_selected=FIELD(things,0x60,void*);FIELD(things,0x60,void*)=play_instrument;
    unsigned saved_pad_index=FIELD(things,0x5f8,unsigned);FIELD(things,0x5f8,unsigned)=6;
    memset(playback_until,0,sizeof(playback_until));memset(playback_muted_until,0,sizeof(playback_muted_until));
    play_instrument[0x78]=0;assert(music_set_pad_colour(6,13));step_native_observe_rgb(6,0);
    sample_palette_publish(things,program,0,1,0,0);assert((uint32_t)atomic_load(&sample_palette[6])==PAD_SELECT);
    sample_palette_publish(things,program,0,1,0,650);assert(atomic_load(&sample_palette[6])==pad_light(music_identity_rgb(13),.30f));
    /* fix6: native selection orange (device log) is the cursor, not a press; a real press lights the focus colour phase at full. */
    step_native_observe_rgb(6,0xff7700);sample_palette_publish(things,program,0,1,0,650);assert(atomic_load(&sample_palette[6])==pad_light(music_identity_rgb(13),.30f));
    step_native_observe_rgb(6,0x00ffff);sample_palette_publish(things,program,0,1,0,650);assert(atomic_load(&sample_palette[6])==pad_light(music_identity_rgb(13),1.f));
    step_native_observe_rgb(6,0);
    play_instrument[0x78]=1;sample_palette_publish(things,program,0,1,0,650);assert(atomic_load(&sample_palette[6])==pad_light(PAD_MUTED,.32f));
    playback_until[6]=playback_muted_until[6]=700;sample_palette_publish(things,program,0,1,0,650);assert(atomic_load(&sample_palette[6])==pad_light(PAD_MUTED,.65f)&&play_instrument[0x78]==1);
    sample_palette_publish(things,program,0,1,0,750);assert(atomic_load(&sample_palette[6])==pad_light(PAD_MUTED,.32f));
    play_instrument[0x78]=0;sample_focus_program=NULL;memset(playback_until,0,sizeof(playback_until));
    sample_palette_publish(things,program,0,1,0,1234567);assert((uint32_t)atomic_load(&sample_palette[6])==PAD_SELECT);
    sample_palette_publish(things,program,0,1,0,1234967);assert((uint32_t)atomic_load(&sample_palette[6])==music_identity_rgb(13));
    FIELD(things,0x60,void*)=saved_selected;assert(music_set_pad_colour(6,0));
    FIELD(things,0x5f8,unsigned)=saved_pad_index;
    puts("PASS Sample white focus, Pink identity, native held/Loop activity, grey muted-event pulse and expiry; mute property unchanged");
    /* Returning from hidden Steps must leave ordinary Sample pulses enabled. */
    steps=0;
    step_native_bank_destroyed(bank_control);
    atomic_store(&clock_running,0);step_native_playback_context(things,0,1);root=saved_root;controller=saved_controller;
    instruments[6]=NULL;
    FIELD(things,0x3d0,void*)=NULL;FIELD(things,0x668,void*)=NULL;
    FN(0x157df10,void(*)(void*))(play_tree->parent);
    step_native_levels_state(0,0,3);step_native_tick();
    for(unsigned i=0;i<16;++i) assert(pad_step((unsigned)pad_step(i))==(int)i);
    assert(pad_step(12)==0 && pad_step(15)==3 && pad_step(8)==4 && pad_step(3)==15);
    phase="Shift encoder moves selected step group; shifted palette and whole-sound Unmute";
    sound=4;grid=2;steps=want_steps=1;follow=0;hold(31);step_native_tick();
    struct step_event moving=make_note(4,7440,.57f);moving.type|=0x200;insert_note(&moving);
    unsigned total_move_notes=(unsigned)tree()->size;step_native_requested(0x31,1);
    event_control(1);assert(cursor==0&&tree()->size==total_move_notes);
    moving.time=0;assert(choose_exact(&moving));assert(step_native_light(7,&light));
    assert(step_native_button_owns(BUTTON_MUTE));button(BUTTON_MUTE,1);button(BUTTON_MUTE,0);
    moving.type&=~0x200u;assert(choose_exact(&moving)&&!atomic_load(&mute_active));
    step_native_requested(0x31,0);
    phase="picker audition is staged, clears mute in audition copy only, release stops it";
    audition_starts=audition_stops=0;
    unsigned char auditioner[0x30]={0},sequencer_things[0x380]={0};
    FN(0x18b04f0,void(*)(void*,void*,void*,void*))(auditioner,NULL,sequencer_things,NULL);
    assert(FIELD(auditioner,0x18,void*)!=FIELD(control,0x248,void*));
    step_native_audition_created(auditioner);stub(0x18b0510,audition_on);stub(0x18b0c00,audition_off);
    __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
    struct step_event audition_source=moving;event_choice_set(&audition_source,0,3);audition_source.type|=0x200;
    detail_old=detail_edit=audition_source;menu=3;picker_mode=1;picker_base=-4;picker_slice_count=16;
    step_native_tick();unsigned before_audition_notes=(unsigned)tree()->size;
    step_native_pad(7,step_native_ticket());assert(audition_starts==1&&picker_audition_active);
    assert(event_muted(&detail_edit)&&event_choice_value(&detail_edit,0)==3&&tree()->size==before_audition_notes);
    assert(!(audition_event.type&0x300)&&audition_event.time==0);
    step_native_pad_release(7,step_native_ticket());assert(audition_stops==1&&!picker_audition_active);
    picker_mode=8;picker_base=1;step_native_pad(2,step_native_ticket());assert(audition_starts==2&&event_choice_value(&audition_event,15)==3);
    menu=0;step_native_tick();assert(audition_stops==2&&!picker_audition_active);
    phase="time signature conversion";
    FIELD(sig_node,0x10,int)=3;struct bbt p=position(2880);assert(p.bar==1 && p.beat==0 && p.tick==0);
    assert(position(3840).bar==1 && position(3840).beat==1); /* native 3/4 conversion retained */
    phase="other sounds overlay and encoder cursor visibility";
    sound=4;grid=2;steps=want_steps=1;menu=0;follow=0;cursor_visible=1;hold(3);step_native_tick();
    assert(!slots[3].count);struct step_event background_note=make_note(5,720,.4f);insert_note(&background_note);
    unsigned count_before_background=(unsigned)tree()->size;int old_velocity=velocity;
    step_native_requested(0x31,1);unsigned char others_control[16]={0};others_control[8]=0x12;others_control[9]=127;
    assert(step_native_control(others_control,step_native_ticket())&&show_other_sounds&&velocity==old_velocity);
    step_native_requested(0x31,0);step_native_tick();assert(other_sound_steps&(1u<<3));
    assert(cursor_visible);button(0x6f,1);button(0x6f,0);assert(!cursor_visible);
    assert(step_native_light(15,&light)&&(uint32_t)light==PAD_CONTEXT);
    uint64_t background_light=light;
    was_playing=1;play_tick=720;palette();assert(step_native_light(15,&light)&&(uint32_t)light==PAD_PLAYHEAD);was_playing=0;
    struct step_event own_background=make_note(4,720,.5f);own_background.type|=0x200;insert_note(&own_background);++count_before_background;
    step_native_tick();assert(step_native_light(15,&light)&&(uint32_t)light==PAD_MUTED);
    button(0x6f,1);button(0x6f,0);assert(cursor_visible);
    assert(step_native_light(15,&light)&&light!=background_light);
    assert(tree()->size==count_before_background&&velocity==old_velocity);
    show_other_sounds=0;
    phase="Lights palette in both Tune event picker entry paths";
    instruments[6]=instrument;FIELD(sample_refs,8,int)=1;
    FN(0xf535c0,void(*)(uint64_t*,const char*))((uint64_t*)(sample_data+0xa8),"Picker source");
    FIELD(instrument,0xb98,int8_t)=-2;FIELD(layer,0x60,int8_t)=3;
    music_reset();assert(music_set_sample("Picker source",MUSIC_ROOT,0));
    assert(music_set_sample("Picker source",MUSIC_TRANSPOSE,7));
    struct step_event coloured=make_note(6,5280,.6f);event_choice_set(&coloured,0,5);insert_note(&coloured);
    struct music_sample unrelated;music_get_sample("Other pad",&unrelated);unrelated.root=8;unrelated.mode=MUSIC_PIANO;
    music_publish(&unrelated,12);uint64_t published_before_picker=music_published();
    unsigned notes_before_palette=(unsigned)tree()->size;int commands_before_palette=change_calls;
    for(int from_steps=0;from_steps<=1;++from_steps){
        steps=want_steps=from_steps;menu=0;picker_begin(0,&coloured);
        assert(menu==3&&picker_mode==1&&!strcmp(sample_name,"Picker source")&&sample_tuning==1);
        picker_base=-3;picker_has_choice=0;
        for(int mode=MUSIC_MONO;mode<=MUSIC_DEGREES;++mode){
            assert(music_set_sample(sample_name,MUSIC_MODE,mode));
            for(int local=0;local<2;++local){
                music_set_key(2,1);assert(music_set_sample(sample_name,MUSIC_LOCAL_TONIC,local?7:-1));
                assert(music_set_sample(sample_name,MUSIC_LOCAL_SCALE,local?3:-1));
                palette();struct music_sample cfg;music_get_sample(sample_name,&cfg);
                uint64_t snapshot=music_light_snapshot(&cfg,1);
                for(unsigned pad=0;pad<16;++pad){
                    if(pad==8)continue; /* Original event marker has priority. */
                    int replace=0;uint64_t expected=music_pitch_light(snapshot,(int)pad-3,&replace);
                    assert((uint32_t)atomic_load(&lights[pad])==(replace?(uint32_t)expected:0xffffff));
                }
                assert((uint32_t)atomic_load(&lights[8])==PAD_ORIGINAL);
                picker_has_choice=1;level_candidate(&detail_old,&detail_edit,1,11,picker_base);palette_at(0);
                assert((uint32_t)atomic_load(&lights[11])==0xffffff);picker_has_choice=0;
                assert(music_published()==published_before_picker);
            }
        }
        picker_mode=0;palette();assert((uint32_t)atomic_load(&lights[15])==0xffffff);
        picker_mode=8;picker_base=1;picker_slice_count=16;assert(music_set_pad_colour(6,3));palette_at(0);assert((uint32_t)atomic_load(&lights[15])==music_identity_rgb(3));
        picker_has_choice=1;level_candidate(&detail_old,&detail_edit,8,2,picker_base);
        palette_at(0);assert((uint32_t)atomic_load(&lights[2])==PAD_SELECT);
        palette_at(400);assert((uint32_t)atomic_load(&lights[2])==music_identity_rgb(3));
        picker_has_choice=0;assert(music_set_pad_colour(6,0));
        menu=0;preview_gate_set(NULL,NULL);
    }
    assert(tree()->size==notes_before_palette&&change_calls==commands_before_palette&&choose_exact(&coloured));
    FIELD(instrument,0xb98,int8_t)=0;FIELD(layer,0x60,int8_t)=0;
    FN(0xf0a290,void(*)(void*))((void*)(sample_data+0xa8));FIELD(sample_refs,8,int)=0;instruments[6]=NULL;
    music_reset();
    puts("PASS Tune picker Lights: Events and Steps, five modes, project/local key, own source tuning, recorded transpose not doubled, old/choice markers, other Sample snapshot and events unchanged; non-Tune palettes unchanged");
    phase="separate SAMPLE RECORD toggle and exclusive WRITE";
    steps=want_steps=1;menu=0;sound=4;grid=2;follow=0;
    atomic_store(&write_armed,0);atomic_store(&toggle_armed,0);
    int empty_step=-1;
    for(int n=0;n<count();++n){struct slot at[16];assert(scan_page(n/16,sound,grid,at));if(!at[n%16].count){empty_step=n;break;}}
    assert(empty_step>=0);hold(empty_step);step_native_tick();
    unsigned toggle_led=99;assert(step_native_button_owns(BUTTON_SAMPLE_RECORD));
    assert(step_native_button_light(BUTTON_SAMPLE_RECORD,&toggle_led)&&!toggle_led);
    button(BUTTON_SAMPLE_RECORD,1);button(BUTTON_SAMPLE_RECORD,1);button(BUTTON_SAMPLE_RECORD,0);
    assert(atomic_load(&toggle_armed)&&!atomic_load(&write_armed));
    assert(step_native_button_light(BUTTON_SAMPLE_RECORD,&toggle_led)&&toggle_led==1);
    assert(step_native_record_light(&rec_led)&&!rec_led);
    before=tree()->size;s_pad(empty_step%16,step_native_ticket());assert(tree()->size==before+1&&cursor==empty_step);
    s_pad(empty_step%16,step_native_ticket());assert(tree()->size==before&&cursor==empty_step);
    struct step_event foreign=make_note(5,(int64_t)empty_step*grid_ticks[grid],.6f);insert_note(&foreign);
    s_pad(empty_step%16,step_native_ticket());assert(tree()->size==before+2);
    s_pad(empty_step%16,step_native_ticket());assert(tree()->size==before+1&&choose_exact(&foreign));
    struct step_event chord_a=make_note(4,(int64_t)empty_step*grid_ticks[grid],.5f),chord_b=chord_a;
    chord_b.time++;chord_b.type|=0x200;chord_b.data[0x23]++;
    insert_note(&chord_a);insert_note(&chord_b);int group_before=group_delete_calls;
    s_pad(empty_step%16,step_native_ticket());assert(tree()->size==before+1&&group_delete_calls==group_before+1&&choose_exact(&foreign));
    button(BUTTON_RECORD,1);button(BUTTON_RECORD,0);
    assert(atomic_load(&write_armed)&&!atomic_load(&toggle_armed));
    assert(step_native_record_light(&rec_led)&&rec_led==1);
    assert(step_native_button_light(BUTTON_SAMPLE_RECORD,&toggle_led)&&!toggle_led);
    s_pad(empty_step%16,step_native_ticket());s_pad(empty_step%16,step_native_ticket());
    assert(tree()->size==before+2); /* WRITE remains add/select, never toggle. */
    button(BUTTON_SAMPLE_RECORD,1);button(BUTTON_SAMPLE_RECORD,0);
    assert(atomic_load(&toggle_armed)&&!atomic_load(&write_armed));
    button(BUTTON_SAMPLE_RECORD,1);button(BUTTON_SAMPLE_RECORD,0);
    assert(!atomic_load(&toggle_armed)&&!atomic_load(&write_armed));
    s_pad(empty_step%16,step_native_ticket());assert(tree()->size==before+2);
    menu=1;step_native_tick();button(BUTTON_SAMPLE_RECORD,1);button(BUTTON_SAMPLE_RECORD,0);
    assert(!atomic_load(&toggle_armed)&&menu==1);menu=0;step_native_tick();
    step_native_requested(0x31,1);assert(!step_native_button_owns(BUTTON_SAMPLE_RECORD));step_native_requested(0x31,0);
    button(BUTTON_SAMPLE_RECORD,1);button(BUTTON_SAMPLE_RECORD,0);assert(atomic_load(&toggle_armed));
    leave();step_native_tick();assert(!atomic_load(&toggle_armed)&&!atomic_load(&write_armed));
    assert(!step_native_button_owns(BUTTON_SAMPLE_RECORD)&&!step_native_button_light(BUTTON_SAMPLE_RECORD,&toggle_led));
    puts("PASS SAMPLE RECORD toggles a step on/off, removes the whole selected-sound chord including muted notes, preserves other sounds and cursor; WRITE remains add/select, buttons/LEDs mutually exclusive, duplicate down/menu/Shift/exit handled");
    phase="project and queue invalidation";
    step_native_project_changed();step_native_tick();assert(!clip_valid && !step_native_custom());
    step_native_queue_fault();step_native_tick();assert(!step_native_custom());
    assert(!last_control_block);
    extern void check_throw(void*);
    assert(!step_try_call(check_throw,NULL));
    FN(0x157df10,void(*)(void*))(tree()->parent);
    puts("PASS actual ARM events, move/wrap, unmocked transport state/position, shared bank setter, nonidentity pad/MIDI sample name, RGB and pad tickets; Shift+K3 other-sound overlay, unchanged velocity/events, own muted-note and playhead precedence, encoder click toggles only selected-pad cursor; live Tune/Chop exit clears note pulses and restores source-pad playback without Stop");
    return 0;
}
