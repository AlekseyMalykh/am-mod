/* Native factory, native pending recorder and real Synth DSP. No installed
 * application, device, project save or UI hook is involved. Input/commit are
 * explicit fixtures; this is NOT a full native Engine recording proof. */
#define MPC_POLY_TEST_MAIN legacy_poly_fixture_main
#include "check_poly_record.c"
#include "src/pad_midi_note.h"
#include "src/synth.h"

static struct step_event take[16];static unsigned take_count;
static void commit_take(void *r,void *p,int destination,void *event,unsigned flag,
                        uintptr_t a,uintptr_t b,uintptr_t c,unsigned d,int flags)
{
    (void)r;(void)p;(void)a;(void)b;(void)c;(void)d;(void)flags;
    assert(destination==7&&flag==1&&take_count<16);
    take[take_count++]=*(struct step_event*)event;
}
static float render_energy(void)
{
    float l[256]={0},r[256]={0},energy=0;
    am_synth_render_add(l,r,256);
    for(unsigned i=0;i<256;++i){assert(isfinite(l[i])&&isfinite(r[i]));energy+=l[i]*l[i]+r[i]*r[i];}
    return energy;
}
static void render_release(void)
{
    for(unsigned i=0;i<40;++i)render_energy();
    assert(!am_synth_active());
}
int main(int argc,char **argv)
{
    assert(argc==2);load(argv[1]);
    struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);sigaction(SIGILL,&sa,NULL);
    phase="pad MIDI native value adapter";
    am_pad_note_factory make=FN(0x27e54d0,am_pad_note_factory);
    am_pad_note_choice_set set=FN(0x27e4400,am_pad_note_choice_set);
    am_midi_choice_get get=FN(0x27e4230,am_midi_choice_get);
    struct step_event e;
    for(unsigned pitch=0;pitch<128;++pitch)for(unsigned velocity=1;velocity<128;++velocity){
        assert(am_pad_midi_note_make(38,pitch,velocity,9,make,set,get,&e));
        struct am_midi_event out;unsigned note;
        assert(am_pad_midi_note_read(&e,get,&out,&note));
        assert(note==pitch&&out.source_note==38&&out.velocity==velocity);
    }
    memset(&e,0xa5,sizeof e);struct step_event unchanged=e;
    assert(!am_pad_midi_note_make(128,60,100,0,make,set,get,&e));
    assert(!am_pad_midi_note_make(38,128,100,0,make,set,get,&e));
    assert(!am_pad_midi_note_make(38,60,0,0,make,set,get,&e));
    assert(!am_pad_midi_note_make(38,60,100,16,make,set,get,&e));
    assert(!am_pad_midi_note_make(38,60,100,0,NULL,set,get,&e));
    assert(!memcmp(&e,&unchanged,sizeof e));

    struct step_event low,high;
    assert(am_pad_midi_note_make(36,48,31,2,make,set,get,&low));
    assert(am_pad_midi_note_make(36,67,119,2,make,set,get,&high));
    struct am_midi_event out={0},saved=out;unsigned pitch=255;
    struct step_event chop=low;set(chop.data+4,15,0,1);
    assert(!am_pad_midi_note_read(&chop,get,&out,&pitch)&&pitch==255&&!memcmp(&out,&saved,sizeof out));
    chop=low;chop.type|=0x200;assert(!am_pad_midi_note_read(&chop,get,&out,&pitch));

    phase="native recorder independent gate finalization";
    assert(poly_record_install(base));redirect(0x213a940,commit_take);
    unsigned char recorder[0x1c0]={0};void *header=recorder+0x1a0,*program=(void*)0x4560;
    F(header,0,void*)=F(header,8,void*)=header;
    /* These native pending nodes stand in for input accepted by the recorder.
     * No Sample object or sample audio voice is allocated in this fixture. */
    pending(recorder,program,&low);pending(recorder,program,&high);
    finish_note(recorder,program,7,MODIFIER_MARK|0x100000u|((unsigned)high.data[6]<<8)|36,0,500,0,0,0);
    assert(take_count==1&&F(&take[0],0x28,int64_t)==400);
    assert(am_pad_midi_note_read(take,get,&out,&pitch)&&pitch==67&&out.velocity==119);
    finish_note(recorder,program,7,MODIFIER_MARK|0x100000u|((unsigned)high.data[6]<<8)|36,0,600,0,0,0);
    assert(take_count==1); /* A stale release cannot finish the other pitch. */
    finish_note(recorder,program,7,MODIFIER_MARK|0x100000u|((unsigned)low.data[6]<<8)|36,0,800,0,0,0);
    assert(take_count==2&&F(&take[1],0x28,int64_t)==700&&!F(recorder,0x1b0,uint64_t));
    assert(am_pad_midi_note_read(take+1,get,&out,&pitch)&&pitch==48&&out.velocity==31);

    phase="native note values through real Synth DSP";
    am_synth_param_set(2,0);am_synth_param_set(5,0);
    assert(am_pad_midi_note_read(&low,get,&out,&pitch));am_synth_note(pitch,out.velocity,1);
    assert(render_energy()>0&&am_synth_active());am_synth_note(pitch,0,0);render_release();
    for(unsigned i=0;i<take_count;++i){
        assert(am_pad_midi_note_read(take+i,get,&out,&pitch));
        am_synth_note(pitch,out.velocity,1);assert(render_energy()>0);
        am_synth_note(pitch,0,0);render_release();
    }
    struct step_event edited=take[0];set(edited.data+4,0,0,9);
    assert(am_pad_midi_note_read(&edited,get,&out,&pitch)&&pitch==69);
    assert(am_pad_midi_note_read(take,get,&out,&pitch)&&pitch==67);
    puts("PASS pad MIDI prototype: all 128 pitches x 127 velocities through native factory/choices, channel, transactional rejection, actual native recorder finalization with no sample voice, reverse/stale releases, retained velocity/duration, real Synth PCM and editable Tune; Engine input and final project commit are fixtures");
    return 0;
}
