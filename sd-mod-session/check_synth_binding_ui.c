#define MPC_PAD_MIDI_RECORDING_TRIAL 1
#include "src/synth_archive.h"
#include "src/batch_copy.h"
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
#include "src/synth_route.h"
#endif
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#define FIELD(p,o,T) (*(T*)((char*)(p)+(o)))
static uintptr_t base=1;
static _Atomic uintptr_t copy_context;
static _Alignas(8) unsigned char context[0x100],program[0x16a8],instrument[16];
static void*selected=program;static int occupied;
static void*current_program(void){return selected;}
static void*get_instrument(void*p,int pad){assert(p==program&&pad==15);return instrument;}
static int has_sample(void*p){assert(p==instrument);return occupied;}
static void*native_api(unsigned offset){if(offset==0x286bbe0)return get_instrument;if(offset==0x2804760)return has_sample;assert(!"unexpected native call");return NULL;}
#define FN(o,T) ((T)native_api(o))
int batch_piano_input_stop(uintptr_t b,void*c){assert(b==base&&c==context);return 1;}
unsigned batch_piano_synth_held(void){return 0;}
void workflow_synth_record_stop(void);
#include "src/synth_pad_record.inc"
extern void test_synth_load_binding(const struct am_synth_binding*);
extern void test_synth_load_opaque(void);
void am_synth_ui_binding_probe(void){
 atomic_store(&copy_context,(uintptr_t)context);FIELD(program,0xdc8,const char*)="Drum binding";
 am_synth_archive_reset();unsigned cutoff=am_synth_param_get(1);
 workflow_synth_record_tick();assert(am_synth_param_get(1)==cutoff&&!synth_record_enabled);
 occupied=1;assert(workflow_synth_record_toggle()==-1);occupied=0;
 assert(workflow_synth_record_toggle()==1);workflow_synth_record_tick();assert(synth_record_enabled);
 struct am_synth_binding b;uint64_t revision;unsigned mode;
 assert(am_synth_archive_snapshot(&b,&revision,&mode)&&mode==AM_SYNTH_ARCHIVE_KNOWN);
 assert(b.carrier==15&&!strcmp(b.program_key,"Drum binding"));
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
 struct am_synth_route_snapshot route;assert(am_synth_route_read(am_synth_archive_generation(),&route));
 assert(route.carrier==15&&!strcmp(route.program_key,b.program_key));
#endif
 am_synth_param_set(1,17);workflow_synth_record_tick();assert(am_synth_archive_snapshot(&b,&revision,&mode)&&b.parameters[1]==17);
 assert(workflow_synth_record_toggle()==0);assert(am_synth_archive_snapshot(&b,&revision,&mode)&&mode==AM_SYNTH_ARCHIVE_KNOWN);
 assert(workflow_synth_record_toggle()==1);workflow_synth_record_tick();
 b.parameters[1]=88;test_synth_load_binding(&b);workflow_synth_record_tick();
 assert(am_synth_param_get(1)==88&&!synth_record_enabled); /* same pointer, different loaded project */
 assert(workflow_synth_record_toggle()==1);workflow_synth_record_tick();selected=NULL;workflow_synth_record_tick();assert(!synth_record_enabled);selected=program;
 am_synth_archive_reset();workflow_synth_record_tick();assert(am_synth_param_get(1)==88); /* legacy load does not change global preset */
#ifdef MPC_SYNTH_NATIVE_PLAYBACK_TRIAL
 assert(!am_synth_route_read(am_synth_archive_generation(),&route));
#endif
 test_synth_load_opaque();workflow_synth_record_tick();assert(workflow_synth_record_toggle()==-2);
 assert(am_synth_archive_snapshot(&b,&revision,&mode)&&mode==AM_SYNTH_ARCHIVE_OPAQUE);
 puts("PASS Synth UI binding: empty carrier, key capture, preset edits/load, same-pointer project change, opaque read-only, legacy preset isolation");
}
