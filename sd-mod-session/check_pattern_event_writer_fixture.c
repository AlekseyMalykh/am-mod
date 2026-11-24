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
static _Alignas(8) unsigned char command_manager[0x200],command_environment[0x100],command_transaction[0x88];
static uintptr_t command_environment_vtable[8];static unsigned manager_dispatches;
extern void pattern_history_retain(void*,const void*,int);
static int history_environment_valid(void*p,void*flags,int mode){assert(p==command_environment&&flags&&mode==3);return 1;}
static void history_monitor(void*p,void*manager,int enabled){assert(p==command_environment+8&&manager==command_manager&&enabled==1);}
static void history_enter(void*p){assert(p==command_manager&&F(p,0,uintptr_t)==base+0x6d97818);++manager_dispatches;}
static void history_trim(void*p,uint64_t limit){assert(p==command_manager);(void)limit;}
void pattern_history_setup(void){
    /* Same native dispatch boundary as check_batch_dispatch.c; model monitor,
     * history maintenance and storage are adapters, not command execution. */
    redirect(0x19d12e0,history_monitor);redirect(0x19cd820,history_enter);
    redirect(0x19cd250,history_trim);redirect(0x19cbc30,pattern_history_retain);
    command_environment_vtable[5]=(uintptr_t)history_environment_valid;F(command_environment,0,void*)=command_environment_vtable;
    FN(0x19cf2b0,void(*)(void*,void*,void*,uint64_t))(command_manager,command_environment,(void*)1,UINT64_MAX);
    F(command_manager,0x1b0,void*)=command_transaction;
}
void*pattern_history_create(const struct step_event*event){
    void*command=FN(0xb51418,void*(*)(size_t))(0x98);
    /* Mode 2 suppresses the optional nested edit; the real inline DeleteEvent
     * constructor initializes native strings, command state and event copy. */
    FN(0x19d8ba0,void(*)(void*,void*,const void*,unsigned))(command,pattern_fixture_list(0),event,2);
    assert(F(command,0,uintptr_t)==base+0x6e44288&&!F(command,0x90,void*));return command;
}
int pattern_history_dispatch(const void*shared){return FN(0x19d0a00,int(*)(void*,const void*))(command_manager,shared);}
void*pattern_history_transaction(void){return command_transaction;}
unsigned pattern_history_dispatches(void){return manager_dispatches;}
int pattern_history_replay(void*command,int undo){
    return undo?FN(0x19c9c40,int(*)(void*))(command):FN(0x19c9a50,int(*)(void*,void*))(command,command_environment);
}
