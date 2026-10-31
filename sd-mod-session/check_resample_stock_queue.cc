/* Transport boundary only; the production queue is verified separately. */
#include "src/resample_queue.h"
#include <cassert>
struct resample_queue {int(*tick)(void*);void*user;bool pending=false,stopped=false;};
static resample_queue*current;
extern "C" resample_queue*resample_queue_create(uintptr_t,void*,int(*tick)(void*),void*user){current=new resample_queue{};current->tick=tick;current->user=user;return current;}
extern "C" int resample_queue_wake(resample_queue*q){if(q->stopped)return 0;q->pending=true;return 1;}
extern "C" void resample_queue_stop(resample_queue*q){q->stopped=true;q->pending=false;}
extern "C" int resample_queue_status(resample_queue*q){return q->pending;}
extern "C" int resample_queue_destroy(resample_queue*){return 0;}
extern "C" void stock_queue_pump(){assert(current->pending&&!current->stopped);current->pending=false;assert(!current->tick(current->user));}
