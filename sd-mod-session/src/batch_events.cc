/* Pinned 1.3.0.12 contracts:
 * Project sequences:2909be0, array1250/count125c (also stock1963410).
 * Sequence list for track:2957ae0(sequence,track,0), as stock19634ac.
 * Bulk native Undo command:19501b0..1950210 ->19d79d0.
 * Pad->MIDI is Program12a8, not CopyPad's unrelated18b8 key properties.
 */
#include "batch_events.h"
#include <vector>
#include <set>
#include <memory>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <cmath>
#define F(p,o,t) (*(t*)((char*)(p)+(o)))
namespace {
static uintptr_t base;
#define FN(o,t) ((t)(base+(o)))
struct Event {int64_t time;uint32_t type,reserved;unsigned char data[40];};
static_assert(sizeof(Event)==56,"Native SequenceEvent");
struct Less {bool operator()(const Event&a,const Event&b)const {
 return FN(0x19d4d70,int(*)(const Event*,const Event*))(&a,&b)&255;
}};
using Events=std::multiset<Event,Less>;
/* Native changed-events argument is a map, though always empty here. Its
 * empty header has the same ABI as the two native multiset headers. */
static_assert(sizeof(Events)==48,"Native event multiset ABI");
static void drop(void*p){if(p)((void(*)(void*))F(p,0,uintptr_t*)[1])(p);}
struct Commands {
 std::vector<void*> commands;
 unsigned notes=0,done=0;
 ~Commands(){for(void*p:commands)drop(p);}
 int undo(){int failed=0;while(done){void*p=commands[--done];if(((int(*)(void*))F(p,0,uintptr_t*)[9])(p))failed=1;}return failed;}
 int perform(){
  if(done)return 1;
  try{for(void*p:commands){if(((int(*)(void*))F(p,0,uintptr_t*)[8])(p)){undo();return 1;}++done;}}
  catch(...){undo();return 1;}
  return 0;
 }
};
static void *make_command(void*list,const Events&added){
 Events removed,changed;uint64_t title=0,merge=0;
 void*p=FN(0xb51418,void*(*)(size_t))(0x100);
 try{
  FN(0xf535c0,void(*)(uint64_t*,const char*))(&title,"Copy Pad Events");
  FN(0xf535c0,void(*)(uint64_t*,const char*))(&merge,"");
  /* Float1 bits, unchanged event type, native normalization, and no merge
   * token, exactly the bulk factory's add-only branch. No factory dispatch:
   * the outer Batch owns perform/undo and the sole history entry. */
  FN(0x19d79d0,void(*)(void*,void*,const void*,const void*,const void*,
                       uint32_t,int,int,const void*,const void*,int,uintptr_t))
   (p,list,&removed,&added,&changed,0x3f800000u,0,1,&title,&merge,0,0);
 }catch(...){FN(0xb51408,void(*)(void*,size_t))(p,0x100);if(title)FN(0xf0a290,void(*)(void*))(&title);if(merge)FN(0xf0a290,void(*)(void*))(&merge);throw;}
 FN(0xf0a290,void(*)(void*))(&title);FN(0xf0a290,void(*)(void*))(&merge);return p;
}
}
extern "C" void *batch_events_prepare(uintptr_t b,void*project,void*things,void*program,
 const unsigned*src,const unsigned*dst,unsigned count){
 base=b;
 try{
  if(!project||!things||!program||!src||!dst||!count||count>16)return nullptr;
  if(F(project,0,uintptr_t)!=base+0x6ecc648)return nullptr;
  void*track=F(things,0x368,void*);if(!track)return nullptr;
  int sequences=F(project,0x125c,int);
  if(sequences<0||sequences>1024||(sequences&&!F(project,0x1250,void*)))return nullptr;
  int target[128];for(int&i:target)i=-1;
  for(unsigned i=0;i<count;++i){
   if(src[i]>=128||dst[i]>=128||src[i]==dst[i])return nullptr;
   int from=F(program,0x12a8+src[i]*4,int),to=F(program,0x12a8+dst[i]*4,int);
   if(from<0||from>=128||to<0||to>=128||from==to||target[from]>=0)return nullptr;
   target[from]=to;
  }
  std::unique_ptr<Commands> out(new Commands);
  std::set<void*> seen;
  /* Budget bounds the whole operation before any project mutation. */
  unsigned budget=262144;
  for(int s=0;s<sequences;++s){
   void*seq=FN(0x2909be0,void*(*)(void*,int))(project,s);
   if(!seq)continue;
   if(F(seq,0,uintptr_t)!=base+0x6ecdff0)return nullptr;
   void*list=FN(0x2957ae0,void*(*)(void*,void*,int))(seq,track,0);
   if(!list||!seen.insert(list).second)continue;
   Events added;
   void*end=(char*)list+0x68,*node=F(list,0x78,void*);
   while(node!=end){
    if(!node||!budget--)return nullptr;
    Event e;std::memcpy(&e,(char*)node+0x20,sizeof(e));
    float velocity;std::memcpy(&velocity,e.data,sizeof(velocity));
    unsigned note=e.data[0x20];
    if((e.type&255)==3&&note<128&&target[note]>=0){
     if(!std::isfinite(velocity)||velocity<0.f||velocity>1.01f)return nullptr;
     if(out->notes+added.size()>=32768)return nullptr;
     e.data[0x20]=(unsigned char)target[note];added.insert(e);
    }
    node=FN(0xb51738,void*(*)(void*))(node);
   }
   if(!added.empty()){
    /* Reserve first so a vector allocation cannot strand a native command. */
    out->commands.reserve(out->commands.size()+1);
    out->commands.push_back(make_command(list,added));out->notes+=(unsigned)added.size();
    std::fprintf(stderr,"BATCH EVENTS_SNAPSHOT sequence=%d notes=%zu\n",s,added.size());
   }
  }
  return out.release();
 }catch(...){return nullptr;}
}
extern "C" int batch_events_perform(void*p){return p?static_cast<Commands*>(p)->perform():1;}
extern "C" int batch_events_undo(void*p){return p?static_cast<Commands*>(p)->undo():1;}
extern "C" void batch_events_free(void*p){delete static_cast<Commands*>(p);}
extern "C" unsigned batch_events_count(void*p){return p?static_cast<Commands*>(p)->notes:0;}
