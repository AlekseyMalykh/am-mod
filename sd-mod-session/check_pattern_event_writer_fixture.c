#define pattern_native_proof pattern_event_writer_proof
#include "check_pattern_native_fixture.c"
#undef pattern_native_proof

static unsigned notifications;
static uint64_t notified_size;
static void event_notification(void*p,int kind){
    assert(p==clips[0]+0x688&&kind==0);
    ++notifications;notified_size=F(p,0x88,uint64_t);
}
void pattern_event_writer_setup(void){redirect(0x27ee100,event_notification);notifications=0;}
unsigned pattern_event_notifications(void){return notifications;}
uint64_t pattern_event_notified_size(void){return notified_size;}
void*pattern_event_erase(unsigned ordinal){
    void*list=pattern_fixture_list(0),*node=pattern_fixture_node(0,ordinal);
    void*next=FN(0xb51738,void*(*)(void*))(node);
    /* Iterator is passed by address, NOT as the node pointer. Native function
     * updates change trees, erases/frees the main node and returns successor. */
    void*result=FN(0x27eef10,void*(*)(void*,void**))(list,&node);
    assert(result==next);return result;
}
void*pattern_event_erase_key(const struct step_event*key){
    return FN(0x27ef360,void*(*)(void*,const void*))(pattern_fixture_list(0),key);
}
void pattern_event_insert(const struct step_event*event){
    FN(0x27ee5d0,void*(*)(void*,const void*))(pattern_fixture_list(0),event);
}
static _Alignas(8) unsigned char history_command[0xa0],delta_sets[4][0x30];
void pattern_event_command_setup(const struct step_event*event){
    /* Inline-only command fixture, not a factory/history-manager test.
     * The real command virtual methods execute against the real EventList. */
    memset(history_command,0,sizeof history_command);F(history_command,0,uintptr_t)=base+0x6e44288;
    F(history_command,0x50,void*)=pattern_fixture_list(0);memcpy(history_command+0x58,event,sizeof *event);
}
void pattern_event_command(int undo){
    assert(undo==0||undo==1);uintptr_t*vtable=F(history_command,0,uintptr_t*);
    assert(vtable[8]==base+0x19d8f70&&vtable[9]==base+0x19d52c0);
    assert(((int(*)(void*))vtable[undo?9:8])(history_command)==0);
}
void pattern_event_delta_setup(const struct step_event*first,const struct step_event*duplicate){
    for(unsigned i=0;i<4;++i){
        memset(delta_sets[i],0,sizeof delta_sets[i]);
        F(delta_sets[i],0x18,void*)=F(delta_sets[i],0x20,void*)=delta_sets[i]+8;
    }
    FN(0x1574460,void*(*)(void*,const void*))(delta_sets[0],first);
    for(unsigned i=0;i<2;++i)FN(0x1574460,void*(*)(void*,const void*))(delta_sets[0],duplicate);
    FN(0x1574460,void*(*)(void*,const void*))(delta_sets[2],duplicate);
    struct step_event changed=*duplicate;changed.time=4800;changed.data[32]=52;
    FN(0x1574460,void*(*)(void*,const void*))(delta_sets[3],&changed);
}
void pattern_event_delta(unsigned operation){
    assert(operation<4);unsigned removed=operation,added=operation<2?1-operation:5-operation;
    FN(0x27f0ee0,void(*)(void*,const void*,const void*))(pattern_fixture_list(0),delta_sets[removed],delta_sets[added]);
    assert(F(delta_sets[0],0x28,uint64_t)==3&&!F(delta_sets[1],0x28,uint64_t));
    assert(F(delta_sets[2],0x28,uint64_t)==1&&F(delta_sets[3],0x28,uint64_t)==1);
}
