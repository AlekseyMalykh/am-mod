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
