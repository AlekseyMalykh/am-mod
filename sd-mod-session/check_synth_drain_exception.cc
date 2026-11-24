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
