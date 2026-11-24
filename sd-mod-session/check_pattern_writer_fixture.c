#define pattern_native_proof pattern_writer_proof
#include "check_pattern_native_fixture.c"
#undef pattern_native_proof
static _Alignas(8) unsigned char alternate_program[0x2900],alternate_clip[0x900],map_rows[2][0xe0],alternate_map_node[0x40],other_track[0xc00];
static void*map_span[2];static uintptr_t program_vtable[41];
struct program_observation {unsigned kind;void*current;int number;};
static struct program_observation observations[16];static unsigned observation_count;
static void observe(unsigned kind){assert(observation_count<16);observations[observation_count++]=(struct program_observation){kind,F(track,0xbb8,void*),F(track,0xbc0,int)};}
static void property_observer(void*p,uint64_t slot,uint64_t count,void*a,void*b,void*c){
    (void)a;(void)b;(void)c;assert(p==track+0x208&&count==1&&(slot==0x30||slot==0x38));observe((unsigned)slot);
}
static void detach(void*p,void*subscriber){assert((p==program+0x20||p==alternate_program+0x20)&&subscriber==track+0x910);}
static void attach(void*p,void*subscriber,int enabled){assert((p==program+0x20||p==alternate_program+0x20)&&subscriber==track+0x910&&enabled==1);}
static void old_listener(void*p,void*callback){(void)callback;assert(p==program||p==alternate_program);observe(0x100);}
static void new_listener(void*p,void*callback){(void)callback;assert(p==program||p==alternate_program);observe(0x101);}
static void program_changed(void*p,void*old,void*next){assert(p==track&&(old==program||old==alternate_program)&&!next);observe(0x102);}
void*pattern_writer_track(void){return track;}
void*pattern_writer_program(unsigned which){assert(which<2);return which?alternate_program:program;}
void pattern_writer_clear_trace(void){observation_count=0;}
unsigned pattern_writer_trace_count(void){return observation_count;}
unsigned pattern_writer_trace_kind(unsigned i){assert(i<observation_count);return observations[i].kind;}
void*pattern_writer_trace_program(unsigned i){assert(i<observation_count);return observations[i].current;}
int pattern_writer_trace_number(unsigned i){assert(i<observation_count);return observations[i].number;}
void pattern_writer_setup(void){
    memcpy(alternate_program,program,sizeof alternate_program);
    /* Observer services are fixtures. Real 29c9490 code performs all tests,
     * pointer/number assignments and orders calls before/after those stores. */
    program_vtable[32]=base+0x18de400;program_vtable[39]=(uintptr_t)new_listener;program_vtable[40]=(uintptr_t)old_listener;
    F(program,0,void*)=program_vtable;F(alternate_program,0,void*)=program_vtable;
    F(track,0xbc0,int)=5;
    F(program,0x1234,int)=61;F(alternate_program,0x1234,int)=92;
    redirect(0x29c61d0,property_observer);redirect(0x153c8e0,detach);redirect(0x153c200,attach);redirect(0x29c9210,program_changed);
}
int pattern_writer_clip_selectors(void){
    FN(0x27eb120,void(*)(void*))(alternate_clip+0x688);
    struct step_event event=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))(52,222,360,100,0,.5f);
    FN(0x27ee5d0,void*(*)(void*,const void*))(alternate_clip+0x688,&event);
    map_span[0]=map_rows;map_span[1]=map_rows+2;F(sequences[0],0x620,void*)=map_span;
    F(map_rows[1],0xc0,void*)=alternate_map_node;F(alternate_map_node,0x20,void*)=track;F(alternate_map_node,0x28,void*)=alternate_clip;
    unsigned char before[0x28],row_before[0x30];memcpy(before,sequences[0]+0x628,sizeof before);memcpy(row_before,map_rows[1]+0xb0,sizeof row_before);
    void*(*lookup)(void*,void*,int)=FN(0x2957ae0,void*(*)(void*,void*,int));
    assert(lookup(sequences[0],track,0)==clips[0]+0x688);
    assert(lookup(sequences[0],track,1)==alternate_clip+0x688);
    assert(!lookup(sequences[0],track,2)&&!lookup(sequences[0],track,-1));
    assert(!lookup(sequences[0],other_track,0)&&!lookup(sequences[0],other_track,1));
    assert(F(clips[0]+0x688,0x88,uint64_t)==3&&F(alternate_clip+0x688,0x88,uint64_t)==1);
    assert(!memcmp(before,sequences[0]+0x628,sizeof before)&&!memcmp(row_before,map_rows[1]+0xb0,sizeof row_before));
    puts("PASS native Clip map selector: 0 selects default map, 1 selects another pre-existing map, absent/out-of-range return null; map headers and event counts unchanged. Third argument is not a create flag.");
    return 1;
}
