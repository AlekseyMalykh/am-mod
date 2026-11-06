/* Execute pinned native control dispatch, with model/queue service fixtures.
 * No UI, MIDI driver or device is initialized; ELF patches are private. */
#define main unused_input_fx_main
#include "check_fx_native.c"
#undef main
#include "src/mixer_input.h"
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
static unsigned modes,mode,posts;
static void jump(unsigned offset,void*target)
{
    const uint32_t code[]={0x58000050,0xd61f0200};
    memcpy((void*)(base+offset),code,8);memcpy((void*)(base+offset+8),&target,8);
    __builtin___clear_cache((char*)(base+offset),(char*)(base+offset+16));
}
static void open_mode(void*router,unsigned id){assert(router);++modes;mode=id;}
static void*thread_queue(unsigned key){(void)key;return NULL;}
static int post(void*queue,void*action,uintptr_t reference)
{
    assert(queue&&reference);++posts;
    ((void(*)(void*))F(action,0,uintptr_t))((char*)action+16);return 1;
}
static void pad_shortcut(void)
{
    _Alignas(16) unsigned char mapper[0x660]={0},owner[0x400]={0},queue[0x240]={0};
    _Alignas(8) unsigned char shift[0x58]={0},other[0x58]={0},router[0xe0]={0};
    float current[128]={0},raw[128]={0};uintptr_t vtable[3]={0,0,(uintptr_t)post};
    F(mapper,0x1c8,void*)=current;F(mapper,0x1e0,void*)=raw;
    F(mapper,0x1a8,void*)=shift;F(mapper,0x1b0,void*)=other;
    F(owner,0x18,void*)=queue;F(owner,0x30,void*)=router;
    F(queue,0x30,void*)=vtable;
    FN(0x236f0d0,void(*)(void*))(mapper+0xf0);
    FN(0x236f0d0,void(*)(void*))(mapper+0x140);
    uintptr_t callback[4]={(uintptr_t)owner,base+0x14ad170,0,0};
    FN(0x14b6160,void*(*)(void*,void*,void*))(mapper+0xf0,callback,NULL);
    jump(0x1c0e5c0,open_mode);jump(0xb53508,thread_queue);
    typedef void(*PadEdge)(void*,unsigned,unsigned,float);
    PadEdge edge=FN(0x1a3a6d0,PadEdge);
    shift[0x50]=1;
    typedef void(*PhysicalEdge)(void*,unsigned,float);
    PhysicalEdge physical=FN(0x1a3aba0,PhysicalEdge);
    physical(mapper,8,1);assert(modes==1&&mode==0x30ef&&posts==1);
    physical(mapper,8,1);assert(modes==1);
    physical(mapper,8,0);assert(modes==1&&current[8]==0);
    modes=posts=0;
    /* Native physical pad9 is zero-based8. No timer or release is invoked. */
    edge(mapper,8,8,1);assert(modes==1&&mode==0x30ef&&posts==1);
    assert(current[8]==1&&raw[8]==1&&F(owner,0x20,unsigned)==0);
    edge(mapper,8,8,1);assert(modes==1); /* held/repeated down */
    edge(mapper,8,8,0);assert(modes==1&&current[8]==0);
    edge(mapper,8,8,1);assert(modes==2&&posts==2);
    shift[0x50]=0;edge(mapper,8,8,0);assert(modes==2);
    edge(mapper,8,8,1);assert(modes==2); /* unshifted path */
    edge(mapper,8,8,0);
    /* New single gesture decision + actual native Fader dispatch. The future
     * FIFO caller supplies captured raw edges and restores native Shift. */
    struct mixer_entry entry;mixer_entry_init(&entry,0);
    unsigned before=modes;
    assert(mixer_entry_pad(&entry,8,1,1000,1,1,1,0)==MIXER_ENTRY_CONSUME);
    assert(mixer_entry_pad(&entry,8,0,1100,1,0,1,0)==MIXER_ENTRY_OPEN);
    assert(modes==before); /* short never sends a native down */
    assert(mixer_entry_pad(&entry,8,1,2000,1,1,1,0)==MIXER_ENTRY_CONSUME);
    assert(mixer_entry_tick(&entry,2500,1,1,0)==MIXER_ENTRY_LONG_DOWN);
    shift[0x50]=1;edge(mapper,8,8,1);shift[0x50]=0;
    assert(modes==before+1&&mode==0x30ef);
    assert(mixer_entry_tick(&entry,2600,1,1,0)==MIXER_ENTRY_PASS);
    assert(mixer_entry_pad(&entry,8,0,2700,1,0,1,0)==MIXER_ENTRY_NATIVE_UP);
    edge(mapper,8,8,0);assert(modes==before+1&&current[8]==0);
    FN(0x236f110,void(*)(void*))(mapper+0xf0);
    FN(0x236f110,void(*)(void*))(mapper+0x140);
    puts("PASS native Shift+Pad9 dispatch opens Fader Settings 0x30ef on first down; repeated down/release do not reopen; queue/model services are fixtures");
}
static unsigned captured_code,captured_down,captures;
static unsigned char*controls;
static void button(void*p,unsigned down)
{
    uintptr_t offset=(uintptr_t)p-(uintptr_t)controls;
    captured_code=(unsigned)offset;captured_down=down;++captures;
}
static void button_map(void)
{
    _Alignas(8) unsigned char block[0x3400]={0},driver[0x18]={0},event[16]={0};controls=block;
    F(driver,0x10,void*)=block;F(event,0,void*)=driver;jump(0x13904f0,button);
    const unsigned codes[]={0x00,0x01,0x06,0x07,0x0c,0x0d,0x0e,0x31,0x6f};
    const unsigned offsets[]={0x21a0,0x20f0,0x1f90,0x2040,0x1bc8,0x1c20,0x1c78,0x2250,0x2d50};
    for(unsigned i=0;i<sizeof(codes)/sizeof(*codes);++i)for(unsigned down=0;down<2;++down){
        unsigned before=captures;event[8]=(unsigned char)codes[i];event[9]=(unsigned char)down;
        FN(0x159f2f0,void(*)(void*))(event);
        assert(captures==before+1&&captured_code==offsets[i]&&captured_down==down);
    }
    _Alignas(8) unsigned char owner[0x400]={0},router[0xe0]={0},properties[0xa00]={0};
    F(owner,0x68,void*)=block;F(owner,0x30,void*)=router;F(owner,0x148,void*)=properties;
    FN(0x14ab500,void(*)(void*,unsigned))(owner,1);assert(mode==0x20a7);
    FN(0x14ab600,void(*)(void*,unsigned))(owner,1);assert(mode==0x20a5);
    FN(0x14ab680,void(*)(void*,unsigned))(owner,1);assert(mode==0x20ab);
    puts("PASS native button dispatcher maps both physical edges to audited GlobalControls fields; default Sample/PadFX/Seq actions retain native mode IDs");
}
int main(int argc,char**argv)
{
    assert(argc==2);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);
    setbuf(stdout,NULL);load(argv[1]);phase="native pad gesture dispatch";pad_shortcut();
    phase="native physical button mapping";button_map();return 0;
}
