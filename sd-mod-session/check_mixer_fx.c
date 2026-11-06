#define MIXER_SESSION_PROOF_MAIN unused_session_entry
#include "check_mixer_session_native.c"
#include "check_fx_metadata.h"
#include "src/fx_catalog.h"
extern void mixer_fx_fixture_begin(uintptr_t);
extern void mixer_fx_fixture_end(void);
extern void mixer_fx_fixture_position(uint64_t,double,unsigned,unsigned,int64_t,int,double);
extern void mixer_fx_fixture_counts(unsigned*,unsigned*,unsigned*);
extern void mixer_fx_fixture_factory_ready(int);
extern void mixer_fx_fixture_clock_ready(int);
int mixer_fx_test_audio(void){return (int)in_audio;}
static void*(*native_malloc)(size_t);
static void*(*native_calloc)(size_t,size_t);
static void*(*native_realloc)(void*,size_t);
static void(*native_free)(void*);
static void*(*native_new)(size_t);
static void*(*native_new_array)(size_t);
static void(*native_delete)(void*);
static void(*native_delete_array)(void*);
static int(*native_mutex)(pthread_mutex_t*);
static void*checked_malloc(size_t n){assert(!in_audio);return native_malloc(n);}
static void*checked_calloc(size_t a,size_t b){assert(!in_audio);return native_calloc(a,b);}
static void*checked_realloc(void*p,size_t n){assert(!in_audio);return native_realloc(p,n);}
static void checked_free(void*p){assert(!in_audio);native_free(p);}
static void*checked_new(size_t n){assert(!in_audio);return native_new(n);}
static void*checked_new_array(size_t n){assert(!in_audio);return native_new_array(n);}
static void checked_delete(void*p){assert(!in_audio);native_delete(p);}
static void checked_delete_array(void*p){assert(!in_audio);native_delete_array(p);}
static int checked_mutex(pthread_mutex_t*p){assert(!in_audio);return native_mutex(p);}
static void*(*native_nothrow)(size_t,const void*);
static void*checked_nothrow(size_t n,const void*p){assert(!in_audio);return native_nothrow(n,p);}
static void*(*native_aligned_new)(size_t,size_t);
static void*checked_aligned_new(size_t a,size_t b){assert(!in_audio);return native_aligned_new(a,b);}
static void*(*native_aligned_new_array)(size_t,size_t);
static void*checked_aligned_new_array(size_t a,size_t b){assert(!in_audio);return native_aligned_new_array(a,b);}
static void*(*native_memalign)(size_t,size_t);
static void*checked_memalign(size_t a,size_t b){assert(!in_audio);return native_memalign(a,b);}
static void*(*native_aligned_alloc)(size_t,size_t);
static void*checked_aligned_alloc(size_t a,size_t b){assert(!in_audio);return native_aligned_alloc(a,b);}
static void(*native_sized_delete)(void*,size_t);
static void checked_sized_delete(void*p,size_t n){assert(!in_audio);native_sized_delete(p,n);}
static void(*native_sized_delete_array)(void*,size_t);
static void checked_sized_delete_array(void*p,size_t n){assert(!in_audio);native_sized_delete_array(p,n);}
static void(*native_aligned_delete)(void*,size_t);
static void checked_aligned_delete(void*p,size_t n){assert(!in_audio);native_aligned_delete(p,n);}
static void(*native_aligned_delete_array)(void*,size_t);
static void checked_aligned_delete_array(void*p,size_t n){assert(!in_audio);native_aligned_delete_array(p,n);}
static void(*native_sized_aligned_delete)(void*,size_t,size_t);
static void checked_sized_aligned_delete(void*p,size_t n,size_t a){assert(!in_audio);native_sized_aligned_delete(p,n,a);}
static int(*native_mutex_try)(pthread_mutex_t*);
static int checked_mutex_try(pthread_mutex_t*p){assert(!in_audio);return native_mutex_try(p);}
static int(*native_mutex_unlock)(pthread_mutex_t*);
static int checked_mutex_unlock(pthread_mutex_t*p){assert(!in_audio);return native_mutex_unlock(p);}
static int(*native_mutex_destroy)(pthread_mutex_t*);
static int checked_mutex_destroy(pthread_mutex_t*p){assert(!in_audio);return native_mutex_destroy(p);}
static int(*native_mutex_init)(pthread_mutex_t*,const pthread_mutexattr_t*);
static int checked_mutex_init(pthread_mutex_t*p,const pthread_mutexattr_t*a){assert(!in_audio);return native_mutex_init(p,a);}
void*check_native_symbol(const char*name,void*resolved)
{
#define BIND(symbol,slot,fn) if(!strcmp(name,symbol)){slot=resolved;return fn;}
    BIND("malloc",native_malloc,checked_malloc)
    BIND("calloc",native_calloc,checked_calloc)
    BIND("realloc",native_realloc,checked_realloc)
    BIND("free",native_free,checked_free)
    BIND("_Znwm",native_new,checked_new)
    BIND("_Znam",native_new_array,checked_new_array)
    BIND("_ZdlPv",native_delete,checked_delete)
    BIND("_ZdaPv",native_delete_array,checked_delete_array)
    BIND("pthread_mutex_lock",native_mutex,checked_mutex)
    BIND("_ZnwmRKSt9nothrow_t",native_nothrow,checked_nothrow)
    BIND("_ZnwmSt11align_val_t",native_aligned_new,checked_aligned_new)
    BIND("_ZnamSt11align_val_t",native_aligned_new_array,checked_aligned_new_array)
    BIND("memalign",native_memalign,checked_memalign)
    BIND("aligned_alloc",native_aligned_alloc,checked_aligned_alloc)
    BIND("_ZdlPvm",native_sized_delete,checked_sized_delete)
    BIND("_ZdaPvm",native_sized_delete_array,checked_sized_delete_array)
    BIND("_ZdlPvSt11align_val_t",native_aligned_delete,checked_aligned_delete)
    BIND("_ZdaPvSt11align_val_t",native_aligned_delete_array,checked_aligned_delete_array)
    BIND("_ZdlPvmSt11align_val_t",native_sized_aligned_delete,checked_sized_aligned_delete)
    BIND("pthread_mutex_trylock",native_mutex_try,checked_mutex_try)
    BIND("pthread_mutex_unlock",native_mutex_unlock,checked_mutex_unlock)
    BIND("pthread_mutex_destroy",native_mutex_destroy,checked_mutex_destroy)
    BIND("pthread_mutex_init",native_mutex_init,checked_mutex_init)
#undef BIND
    return resolved;
}
static unsigned index_of_fx(uint32_t id)
{for(unsigned i=0;i<fx_catalog_count();++i)if(fx_catalog_at(i)->id==id)return i;assert(0);return 0;}
static void real_block(uint64_t epoch,double bpm,unsigned num,unsigned den,int64_t frame,int playing,double ppq)
{
    mixer_fx_fixture_position(epoch,bpm,num,den,frame,playing,ppq);live_fixture();run_discovery();
    for(unsigned i=0;i<N;++i)assert(isfinite(root_pcm[0][i])&&isfinite(root_pcm[1][i]));
}
static void key(struct mixer_control *control,enum mixer_action action,uint64_t ticket)
{assert(mixer_control_button(control,action,1,ticket)&&mixer_control_button(control,action,0,ticket));}
int main(int argc,char**argv)
{
    assert(argc==2);setbuf(stdout,NULL);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGILL,&sa,NULL);sigaction(SIGABRT,&sa,NULL);
    load(argv[1]);phase="Mixer real native FX";check_fx_metadata(base);mixer_fx_fixture_begin(base);
    assert(mixer_live_native_install(base));ops=mixer_live_native_ops()->route;collection_checks();
    service(0x220a4a0,reset_engine);service(0x2226d10,events);service(0x18c6990,render_list);service(0x221ee10,voice_chunk);
    service(0x2760ae0,native_source);assert(pad_eq_native_install(base));use_voice_pcm=1;live_caller_fixture();
    mixer_archive_init(base);struct mixer_project model,saved;uint64_t revision;unsigned mode;
    assert(mixer_archive_snapshot(&model,&revision,&mode));model.output[0]=1;
    model.track[0].fx.count=1;assert(fx_catalog_stage(index_of_fx(0x61465444),48000,model.track[0].fx.stage));
    model.master.fx.count=1;assert(fx_catalog_stage(index_of_fx(0x61464c6d),48000,model.master.fx.stage));
    assert(mixer_archive_replace(&model,revision)==MIXER_EDIT_OK);mixer_runtime_ui_tick();
    real_block(1,123,7,8,0,0,0);struct mixer_native_binding binding;
    assert(mixer_live_native_binding(&binding)&&binding.clock_valid);uint64_t epoch=binding.epoch;
    float native_dry[2][N];memcpy(native_dry,root_pcm,sizeof(native_dry));
    mixer_fx_fixture_position(epoch,123,7,8,0,0,0);mixer_runtime_ui_tick();assert(automatic);
    real_block(epoch,123,7,8,0,0,0);mixer_session_ui_tick(automatic);assert(mixer_session_status(automatic)==1);
    struct mixer_control*c=mixer_session_control(automatic);assert(mixer_control_open(c,1,0));uint64_t ticket=mixer_control_ticket(c);
    float dry[N];memcpy(dry,root_pcm[0],sizeof(dry));
    assert(mixer_control_button(c,MIXER_KNOB,1,ticket)&&mixer_control_button(c,MIXER_KNOB,0,ticket));
    assert(mixer_control_cc(c,0x10,127,ticket));mixer_session_ui_tick(automatic);mixer_runtime_ui_tick();
    assert(mixer_archive_snapshot(&saved,&revision,&mode)&&saved.track[0].fx.stage[0].parameter[0]==0);
    real_block(epoch,137,6,8,48000,1,2.0);mixer_runtime_ui_tick();mixer_session_ui_tick(automatic);
    assert(mixer_archive_snapshot(&saved,&revision,&mode)&&saved.track[0].fx.stage[0].parameter[0]==1);
    double difference=0;for(unsigned i=240;i<N;++i)difference+=fabs(root_pcm[0][i]-dry[i]);assert(difference>1e-4);
    /* Stop holds position, loops may repeat exactly the same native frame;
     * each real callback still starts at offset zero in every native FX. */
    for(unsigned i=0;i<4;++i)real_block(epoch,97,3,4,1234,1,7.25);
    for(unsigned i=0;i<3;++i)real_block(epoch,97,3,4,1234,0,7.25);
    /* Every supported native catalog entry runs through the same live graph,
     * Track and Master together. Factories/delete/imports are guarded on RT. */
    for(unsigned i=1;i<fx_catalog_count();++i){
      const struct fx_catalog_entry *entry=fx_catalog_at(i);
      for(unsigned variant=0;variant<3;++variant){
        mixer_control_button(c,MIXER_SHIFT,1,ticket);key(c,MIXER_SELECT,ticket);mixer_control_button(c,MIXER_SHIFT,0,ticket);
        assert(mixer_control_cc(c,0x10,(i*128+fx_catalog_count()-1)/fx_catalog_count(),ticket));key(c,MIXER_SELECT,ticket);
        /* Exercise both normalized boundaries through the actual paged UI,
         * then factory defaults. This includes first process after prepare. */
        if(variant<2){
            unsigned named=0;
            for(unsigned p=0;p<entry->count;++p)if(entry->parameter[p]&&*entry->parameter[p]){
                if(named&&named%3==0)assert(mixer_control_cc(c,0x64,1,ticket));
                assert(mixer_control_cc(c,0x10+named%3,variant?127:0,ticket));++named;
            }
        }
        mixer_session_ui_tick(automatic);mixer_runtime_ui_tick();
        for(unsigned j=0;j<4;++j)real_block(epoch,137,6,8,(int64_t)j*N,1,j*N/48000.*137/60.);
        mixer_runtime_ui_tick();mixer_session_ui_tick(automatic);
        assert(mixer_archive_snapshot(&saved,&revision,&mode)&&saved.track[0].fx.count==1&&saved.track[0].fx.stage[0].native_id==fx_catalog_at(i)->id);
        struct mixer_view_state state;mixer_control_view(c,0,&state);
        assert(mixer_session_status(automatic)==1&&mixer_control_active(c)&&!state.pending);
        for(unsigned p=0;p<entry->count;++p)if(entry->parameter[p]&&*entry->parameter[p])
            assert(saved.track[0].fx.stage[0].parameter[p]==(variant<2?(float)variant:entry->defaults[p]));
      }
        printf("PASS live Mixer native FX defaults/min/max %08x %s\n",entry->id,entry->name);
    }
    mixer_fx_fixture_factory_ready(0);
    assert(mixer_control_cc(c,0x10,127,ticket));mixer_session_ui_tick(automatic);mixer_runtime_ui_tick();mixer_session_ui_tick(automatic);
    struct mixer_view_state view;mixer_control_view(c,0,&view);
    assert(!view.pending&&mixer_control_active(c)&&!strcmp(mixer_control_message(c),"FX unavailable"));
    assert(mixer_archive_snapshot(&saved,&revision,&mode)&&saved.track[0].fx.stage[0].parameter[0]==0);
    unsigned before_create,before_delete,before_process;
    mixer_fx_fixture_counts(&before_create,&before_delete,&before_process);
    key(c,MIXER_B1,ticket);assert(mixer_control_cc(c,0x12,127,ticket));
    mixer_session_ui_tick(automatic);mixer_runtime_ui_tick();real_block(epoch,97,3,4,0,1,0);
    mixer_runtime_ui_tick();mixer_session_ui_tick(automatic);
    assert(mixer_archive_snapshot(&saved,&revision,&mode)&&saved.track[0].colour==mixer_colour_at(11));
    assert(mixer_control_cc(c,0x14,103,ticket)&&mixer_control_cc(c,0x14,107,ticket));
    mixer_session_ui_tick(automatic);mixer_runtime_ui_tick();real_block(epoch,97,3,4,0,1,0);
    mixer_runtime_ui_tick();mixer_session_ui_tick(automatic);
    assert(mixer_archive_snapshot(&saved,&revision,&mode)&&saved.track[0].mix.level_db==1);
    assert(mixer_control_cc(c,0x14,103,ticket));mixer_session_ui_tick(automatic);mixer_runtime_ui_tick();real_block(epoch,97,3,4,0,1,0);
    mixer_runtime_ui_tick();mixer_session_ui_tick(automatic);
    unsigned after_create,after_delete,after_process;mixer_fx_fixture_counts(&after_create,&after_delete,&after_process);
    assert(after_create==before_create&&after_process>before_process);
    key(c,MIXER_KNOB,ticket);
    mixer_fx_fixture_factory_ready(1);real_block(epoch,97,3,4,0,1,0);
    assert(mixer_control_cc(c,0x10,127,ticket));mixer_session_ui_tick(automatic);mixer_runtime_ui_tick();
    real_block(epoch,97,3,4,N,1,N/48000.*97/60.);mixer_runtime_ui_tick();mixer_session_ui_tick(automatic);
    assert(mixer_archive_snapshot(&saved,&revision,&mode)&&saved.track[0].fx.stage[0].parameter[0]==1);
    unsigned created,destroyed,processed,before;
    mixer_fx_fixture_counts(&created,&destroyed,&before);live_fixture();run_live();mixer_fx_fixture_counts(&created,&destroyed,&processed);assert(processed==before);
    /* A revoked clock must not invent120 BPM or commit an unprocessed edit.
     * After a fresh native observation the saved FX can be prepared again. */
    assert(mixer_control_cc(c,0x10,0,ticket));mixer_session_ui_tick(automatic);mixer_runtime_ui_tick();
    mixer_fx_fixture_clock_ready(0);real_block(epoch,97,3,4,0,1,0);
    mixer_session_owner_tick(automatic);assert(mixer_session_status(automatic)<0);
    assert(mixer_archive_snapshot(&saved,&revision,&mode)&&saved.track[0].fx.stage[0].parameter[0]==1);
    mixer_runtime_ui_tick();assert(!automatic);
    mixer_fx_fixture_clock_ready(1);real_block(epoch,97,3,4,0,1,0);mixer_runtime_ui_tick();assert(automatic);
    real_block(epoch,97,3,4,0,1,0);mixer_session_ui_tick(automatic);c=mixer_session_control(automatic);assert(mixer_control_open(c,1,0));ticket=mixer_control_ticket(c);
    mixer_control_button(c,MIXER_SHIFT,1,ticket);key(c,MIXER_B2,ticket);mixer_control_button(c,MIXER_SHIFT,0,ticket);
    assert(mixer_control_pad(c,15,1,ticket)&&mixer_control_pad(c,15,0,ticket));
    mixer_control_button(c,MIXER_SHIFT,1,ticket);key(c,MIXER_B2,ticket);mixer_control_button(c,MIXER_SHIFT,0,ticket);
    mixer_session_ui_tick(automatic);mixer_runtime_ui_tick();real_block(epoch,97,3,4,0,1,0);
    mixer_runtime_ui_tick();mixer_session_ui_tick(automatic);real_block(epoch,97,3,4,0,1,0);
    compare_pcm(root_pcm[0],native_dry[0],1);compare_pcm(root_pcm[1],native_dry[1],1);
    assert(!mixer_control_button(c,MIXER_NATIVE_MODE,1,ticket));real_block(epoch,97,3,4,0,1,0);
    mixer_runtime_ui_tick();assert(automatic&&mixer_session_status(automatic)==1);
    mixer_runtime_ui_stop();assert(!automatic);mixer_fx_fixture_end();release_collections();
    mixer_fx_fixture_counts(&created,&destroyed,&processed);
    assert(created&&created==destroyed&&processed>8);
    printf("PASS Mixer native FX: automatic saved Tube Drive, actual factories/DSP/clock, parameter audio->archive ack, live BPM/meter/Stop/loop offsets, closed UI audio, off-audio prepare/destruction; %u instances/%u process calls; native allocation and mutex imports guarded; fixture app services, no hardware\n",created,processed);
    return 0;
}
