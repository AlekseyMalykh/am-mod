/* Song UI against the pinned ELF: real SongScreen vtable install and real
 * editor-context constructor; the stock Song step builders, list row and
 * graphics are recorded/applied by fixtures. No GUI, no device. */
#define base elf_image
#define main unused_fx_main
#define load load_elf
#include "check_fx_native.c"
#undef main
#undef load
#undef base
#undef FN
#undef FIELD
#include "src/song_ui.c"
#include <assert.h>

static void stub(uintptr_t offset,void *fn)
{
    void *page=(void*)((elf_image+offset)&~(uintptr_t)4095);
    assert(!mprotect(page,4096,PROT_READ|PROT_WRITE));
    uint32_t code[]={0x58000050,0xd61f0200};memcpy((void*)(elf_image+offset),code,8);memcpy((void*)(elf_image+offset+8),&fn,8);
    __builtin___clear_cache((char*)(elf_image+offset),(char*)(elf_image+offset+16));
    assert(!mprotect(page,4096,PROT_READ|PROT_EXEC));
}

/* ---- fixture model ---- */
static int steps[64][2],step_count,row;
static unsigned char screen_obj[0x1000],controller_obj[0x400],sequencer_obj[0x1400],song_obj[0x100];
static unsigned char sequences[16][0x210];
static void *songs[1];
static int ops,last_op,last_step,last_seq,last_rep;
static void sync_song(void)
{ FIELD(song_obj,0x30,char*)=(char*)steps; FIELD(song_obj,0x38,char*)=(char*)steps+8*step_count; }
static void check_ctx(void **ctx,void *song){ assert(*ctx==sequencer_obj&&song==song_obj); }
static void op_repeats(void **ctx,void *song,long step,int rep)
{ check_ctx(ctx,song);assert(step>=0&&step<step_count);steps[step][1]=rep;++ops;last_op='R';last_step=(int)step;last_rep=rep; }
static void op_sequence(void **ctx,void *song,long step,int seq)
{ check_ctx(ctx,song);assert(step>=0&&step<step_count);steps[step][0]=seq;++ops;last_op='S';last_step=(int)step;last_seq=seq; }
static void op_insert(void **ctx,void *song,long step,int seq,int rep)
{
    check_ctx(ctx,song);assert(step>=0&&step<=step_count&&step_count<64);
    /* The stock follow handshake is armed during the insert. */
    assert(FIELD(controller_obj,0x292,unsigned char)==1&&FIELD(controller_obj,0x294,int)==(int)step);
    memmove(steps[step+1],steps[step],sizeof(steps[0])*(size_t)(step_count-step));
    steps[step][0]=seq;steps[step][1]=rep;++step_count;sync_song();
    ++ops;last_op='I';last_step=(int)step;last_seq=seq;last_rep=rep;
}
static void op_delete(void **ctx,void *song,long step)
{
    check_ctx(ctx,song);assert(step>=0&&step<step_count);
    memmove(steps[step],steps[step+1],sizeof(steps[0])*(size_t)(step_count-step-1));
    --step_count;sync_song();++ops;last_op='D';last_step=(int)step;
}
static int list_row(void *list){ assert(list==screen_obj+0x798); return row; }
static void *sequence_get(void *sequencer,int seq){ assert(sequencer==sequencer_obj&&seq>=0&&seq<16); return sequences[seq]; }
static int song_bars(void *song,void *sequencer)
{ assert(song==song_obj&&sequencer==sequencer_obj); /* lab: 0x29ba240 walks steps via sequencer */int n=0;for(int i=0;i<step_count;++i)n+=FIELD(sequences[steps[i][0]],0x200,int)*steps[i][1];return n; }
static int selected=-1;
static void select_row(void *property,int value){ assert(property==controller_obj+0x168); selected=value; row=value; }
int music_pad_lighting_mode(void){ return 1; }
uint32_t music_user_rgb(unsigned raw){ return 0x101010u*raw; }
int music_sequence_colour(unsigned sequence){ return (int)(sequence%14); }
static int yes(void *p){ (void)p; return 1; }
static int no(void *p){ (void)p; return 0; }
static void noop(void){}
static char drawn[64];
static void string_ctor(uint64_t *s,const char *text){ *s=0; snprintf(drawn,sizeof(drawn),"%s",text); }

static void button(unsigned code,int down)
{ unsigned char e[16]={0};e[8]=(unsigned char)code;e[9]=(unsigned char)down;assert(song_ui_button(e)); }
static void knob(int value)
{ unsigned char e[16]={0};e[8]=K1;e[9]=(unsigned char)value;assert(song_ui_control(e)); }

int main(int argc,char **argv)
{
    assert(argc==2);setbuf(stdout,NULL);
    struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);sigaction(SIGILL,&sa,NULL);
    load_elf(argv[1]);song_ui_init(elf_image);

    phase="SongScreen vtable install";
    uintptr_t *vt=(uintptr_t*)(elf_image+SONG_VPTR);
    assert(vt[SLOT_PAINT/8]==elf_image+NATIVE_NOOP&&vt[0]==elf_image+NATIVE_DTOR&&vt[1]==elf_image+NATIVE_DELETE);
    assert(song_ui_install());
    assert(vt[SLOT_PAINT/8]==(uintptr_t)paint&&vt[0]==(uintptr_t)destroy&&vt[1]==(uintptr_t)destroy_delete);
    uintptr_t *ex=(uintptr_t*)(elf_image+EXPORT_VPTR);
    assert(ex[SLOT_PAINT/8]==(uintptr_t)export_paint&&ex[0]==(uintptr_t)export_destroy&&ex[1]==(uintptr_t)export_delete);
    assert(!song_ui_install()); /* second install refuses: slots no longer stock */

    phase="fixtures";
    stub(0x112fd50,(void*)yes);stub(0x112f6b0,(void*)no);stub(0x1149880,(void*)noop);
    stub(0x102d5e0,(void*)noop);stub(0x102e000,(void*)noop);stub(0xf535c0,(void*)string_ctor);
    stub(0x1082810,(void*)noop);stub(0x102dca0,(void*)noop);stub(0x10817b0,(void*)noop);
    stub(0x1070d60,(void*)noop);stub(0xf0a290,(void*)noop);
    stub(0x3b71e80,(void*)list_row);stub(0x2909be0,(void*)sequence_get);stub(0x29ba240,(void*)song_bars);
    stub(0x19a20d0,(void*)op_repeats);stub(0x19a29e0,(void*)op_sequence);
    stub(0x19a2d20,(void*)op_insert);stub(0x19a1880,(void*)op_delete);
    stub(0x14e19c0,(void*)select_row);
    for(int i=0;i<16;++i)FIELD(sequences[i],0x200,int)=2;
    FIELD(screen_obj,0,uintptr_t)=elf_image+SONG_VPTR;FIELD(screen_obj,0x110,void*)=controller_obj;
    FIELD(controller_obj,0x260,void*)=sequencer_obj;FIELD(controller_obj,0x1dc,int)=5;
    songs[0]=song_obj;FIELD(sequencer_obj,0xb38,int)=0;
    FIELD(sequencer_obj,0x1260,void**)=songs;FIELD(sequencer_obj,0x126c,unsigned)=1;
    steps[0][0]=0;steps[0][1]=1;steps[1][0]=1;steps[1][1]=1;steps[2][0]=2;steps[2][1]=1;step_count=3;sync_song();row=1;

    phase="ownership before the screen is seen";
    assert(!song_ui_owns(PHONE_CONTROLLER,K1,0,1)&&!song_ui_owns(PHONE_BUTTON,B3,1,1));

    phase="paint binds the screen; tick publishes visibility";
    ((void(*)(void*,void*))vt[SLOT_PAINT/8])(screen_obj,(void*)0x10);
    assert(screen==screen_obj&&!drawn[0]); /* nothing drawn until an edit */
    song_ui_tick();assert(song_ui_active()&&song_ui_pads_active()&&song_ui_selected_sequence()==5);

    phase="ownership pairing";
    assert(song_ui_owns(PHONE_CONTROLLER,K1,0,1)&&!song_ui_owns(PHONE_CONTROLLER,0x11,0,1));
    assert(!song_ui_owns(PHONE_BUTTON,B3,0,1));          /* plain B3 stays stock Insert */
    assert(!song_ui_owns(PHONE_BUTTON,0x0c,1,1));        /* B1 Export untouched */
    assert(song_ui_owns(PHONE_BUTTON,B3,1,1));
    assert(song_ui_owns(PHONE_BUTTON,B3,0,0));           /* Shift released first: up still ours */
    assert(!song_ui_owns(PHONE_BUTTON,B2,0,0));          /* unclaimed up goes native */

    phase="K1 repeats";
    knob(64);assert(!ops);                                 /* baseline only */
    knob(72);assert(ops==1&&last_op=='R'&&last_step==1&&last_rep==3&&steps[1][1]==3);
    knob(70);assert(ops==1);                               /* below one notch */
    knob(10);assert(last_rep==1&&steps[1][1]==1);          /* clamped at x1 */
    int before=ops;knob(6);assert(ops==before);            /* already x1: no command */

    phase="Shift+B3 replace";
    button(B3,1);assert(last_op=='S'&&last_step==1&&last_seq==5&&steps[1][0]==5);button(B3,0);
    ((void(*)(void*,void*))vt[SLOT_PAINT/8])(screen_obj,(void*)0x10);
    assert(!strcmp(drawn,"Step replaced   Step 2/3 x1"));   /* transient strip after the edit */
    toast_until=1;((void(*)(void*,void*))vt[SLOT_PAINT/8])(screen_obj,(void*)0x10);song_ui_tick();assert(!toast_until);
    assert(!song_ui_owns(PHONE_BUTTON,B3,0,0));
    before=ops;song_ui_owns(PHONE_BUTTON,B3,1,1);button(B3,1);button(B3,0);assert(ops==before); /* same sequence */

    phase="Shift+B2 duplicate";
    steps[1][1]=4;
    song_ui_owns(PHONE_BUTTON,B2,1,1);button(B2,1);button(B2,0);
    assert(last_op=='I'&&last_step==2&&last_seq==5&&last_rep==4&&step_count==4);
    assert(FIELD(controller_obj,0x292,unsigned char)==0);  /* follow flag restored */
    song_ui_tick();assert(selected==2&&row==2);             /* new step selected after the command */

    phase="Shift+-/+ move";
    row=1;song_ui_owns(PHONE_BUTTON,MINUS,1,1);button(MINUS,1);button(MINUS,0);
    assert(last_op=='I'&&last_step==0&&steps[0][0]==5&&steps[0][1]==4&&steps[1][0]==0&&step_count==4);
    song_ui_tick();assert(selected==0);
    row=3;before=ops;song_ui_owns(PHONE_BUTTON,PLUS,1,1);button(PLUS,1);button(PLUS,0);assert(ops==before); /* last row */
    row=2;song_ui_owns(PHONE_BUTTON,PLUS,1,1);button(PLUS,1);button(PLUS,0);
    assert(steps[3][0]==5&&steps[3][1]==4&&steps[2][0]==2);

    phase="song length limit";
    FIELD(sequences[5],0x200,int)=SONG_BAR_LIMIT;row=3;before=ops;
    song_ui_owns(PHONE_BUTTON,B2,1,1);button(B2,1);button(B2,0);assert(ops==before&&!strcmp(toast,"Song would be too long"));
    knob(64);knob(80);assert(ops==before);

    phase="end-of-song row and empty selection are ignored";
    row=step_count;before=ops;knob(64);knob(90);song_ui_owns(PHONE_BUTTON,B3,1,1);button(B3,1);button(B3,0);assert(ops==before);

    phase="screen destruction";
    forget(screen_obj);assert(!song_ui_active()&&!screen);
    assert(!song_ui_owns(PHONE_CONTROLLER,K1,0,1)&&!song_ui_owns(PHONE_BUTTON,B3,1,1));

    puts("PASS Song UI: real SongScreen vtable install/refusal and editor context; K1 repeats with clamp, Shift+B3 replace, Shift+B2 duplicate, Shift+-/+ move through stock Song step builders with selection follow, length limit, end row, paired button ownership and screen lifetime; fixture model/graphics, no hardware");
    return 0;
}
