#include "src/synth_accept_native.h"
#include <cassert>
#include <stdexcept>
extern "C" void check_synth_mark_partial_drain(void*);
extern "C" void check_synth_throw_drain(void*engine,uint64_t,unsigned){
 check_synth_mark_partial_drain(engine);
 throw std::runtime_error("fixture: native drain interrupted before removing entry");
}
extern "C" void check_synth_catch_drain(am_synth_drain_fn wrapper,void*engine){
 bool caught=false;
 try{wrapper(engine,96,0);}catch(const std::runtime_error&){caught=true;}
 assert(caught);
}
extern "C" void check_synth_remove_then_fail(void*,void*);
extern "C" void check_synth_throw_track_cancel(void*engine,void*track){
 check_synth_remove_then_fail(engine,track);
 throw std::runtime_error("fixture: Track cancellation failed after deleting entries");
}
extern "C" void check_synth_catch_track_cancel(am_synth_track_cancel_fn wrapper,void*engine,void*track){
 bool caught=false;try{wrapper(engine,track);}catch(const std::runtime_error&){caught=true;}assert(caught);
}
extern "C" void check_synth_accept_unqueued(void*,uint64_t,void*,const step_event*,unsigned,unsigned);
extern "C" void check_synth_throw_engine_event(void*engine,uint64_t offset,void*track,const step_event*event,unsigned kind,unsigned tag){
 check_synth_accept_unqueued(engine,offset,track,event,kind,tag);
 throw std::runtime_error("fixture: accepted On without a due entry");
}
extern "C" void check_synth_catch_engine_event(am_synth_engine_event_fn wrapper,void*engine,void*track,const step_event*event){
 bool caught=false;try{wrapper(engine,96,track,event,3,0);}catch(const std::runtime_error&){caught=true;}assert(caught);
}
