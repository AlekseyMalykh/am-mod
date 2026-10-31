/* One native Undo command containing native CopyPadCommands. Only the
 * source getter at19875ac is redirected, and only during our own perform.
 * Snapshot every source first, so overlapping maps cannot copy an earlier
 * result. PCM is shared exactly as with native CopyPadCommand. */
#include "batch_copy.h"
#include "batch_events.h"
#ifdef MPC_PAD_EQ
extern "C" {
#include "music_settings.h"
}
#endif
#include <memory>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <cstdio>
#include <atomic>
#include <mutex>
#include <cmath>
#define F(p,o,t) (*(t*)((char*)(p)+(o)))
static uintptr_t base;
extern "C" void batch_copy_init(uintptr_t b){base=b;}
#define FN(o,t) ((t)(base+(o)))
static thread_local void *snapshot,*snapshot_program;
static thread_local int snapshot_source=-1;
extern "C" void *batch_copy_source(void*p,int source){if(snapshot&&p==snapshot_program&&source==snapshot_source)return snapshot;return FN(0x286bbe0,void*(*)(void*,int))(p,source);}
struct Pair{uintptr_t a,b;};
struct Row{void*command=nullptr;void*source=nullptr;void*extract=nullptr;void*slice=nullptr;unsigned from=0,to=0;int map=0;bool extraction_started=false;
 int mono=-1;void*functions=nullptr,*project=nullptr,*service=nullptr,*pool=nullptr,*assignment=nullptr;
 uintptr_t converted[2]{};uint64_t mono_name=0;bool assignment_started=false,copy_started=false;
#ifdef MPC_PAD_EQ
 uint64_t source_eq=0,target_eq=0;bool eq_written=false;
#endif
};
static void progress(unsigned phase,unsigned done,unsigned total);
extern "C" void step_layer_sample(void*,int,uintptr_t*,uintptr_t);
extern "C" void batch_name_result(void*,const uint64_t*,const uintptr_t*,uint64_t*,uintptr_t);
static void native_delete(void*p){if(p)((void(*)(void*))F(p,0,uintptr_t*)[1])(p);}
/* Native outer Undo returns before pool cleanup if extraction failed before
 * constructing AssignSampleCommand. Roll back that partial inner result too. */
static int undo_extract(void*p){
 if(!p)return 0;
 if(F(p,0xa0,void*))return FN(0x143ee90,int(*)(void*))(p);
 void*inner=F(p,0x98,void*);
 if(!inner||!F(inner,0xb8,void*))return 0;
 return ((int(*)(void*))F(inner,0,uintptr_t*)[9])(inner);
}
static int undo_row(Row&r)noexcept{
 int result=0;
 try{if(r.assignment_started){if(((int(*)(void*))F(r.assignment,0,uintptr_t*)[9])(r.assignment))result=1;else r.assignment_started=false;}}catch(...){result=1;}
 try{if(r.converted[0]&&!r.assignment_started)FN(0x2930010,void(*)(void*,const uintptr_t*))(r.pool,r.converted);}catch(...){result=1;}
 try{if(r.extraction_started){result=undo_extract(r.extract);r.extraction_started=false;}}catch(...){result=1;}
 try{if(r.copy_started){if(FN(0x1987890,int(*)(void*))(r.command))result=1;else r.copy_started=false;}}catch(...){result=1;}
#ifdef MPC_PAD_EQ
 if(!result&&r.eq_written){music_set_pad_eq(r.to,r.target_eq);r.eq_written=false;}
#endif
 return result;
}
static void *lookup(void*services,uintptr_t type){
 void*lock=(char*)services+0x38;if(FN(0xb51828,int(*)(void*))(lock))throw std::runtime_error("Service lock failed");
 try{void*entry=FN(0x143ad50,void*(*)(void*,const uintptr_t*))(services,&type);void*p=entry?F(entry,0,void*):nullptr;FN(0xb51808,void(*)(void*))(lock);return p;}catch(...){FN(0xb51808,void(*)(void*))(lock);throw;}
}
static uintptr_t vtable[11];
struct NativeSamplePair {uintptr_t words[2]{};~NativeSamplePair(){}};
static void mono_perform(Row&r,void*program){
 if(!r.converted[0]){
  uintptr_t source[2]{};step_layer_sample(r.source,0,source,base+0x2803b90);
  struct Release{uintptr_t*p;~Release(){if(p[1])FN(0x123bca0,void(*)(void*))((void*)p[1]);}}release{source};
  if(!source[0]||FN(0x2941f70,int(*)(void*))((void*)source[0])!=2)throw std::runtime_error("Source is not stereo");
  auto out=FN(0x209e510,NativeSamplePair(*)(void*,const uintptr_t*,unsigned,unsigned,const uint64_t*))
      (r.functions,source,(unsigned)r.mono,24,&r.mono_name);
  r.converted[0]=out.words[0];r.converted[1]=out.words[1];
  if(!r.converted[0]||!r.converted[1]||FN(0x2941f70,int(*)(void*))((void*)r.converted[0])!=1||
     FN(0x2941f80,int(*)(void*))((void*)r.converted[0])!=FN(0x2941f80,int(*)(void*))((void*)source[0]))
      throw std::runtime_error("Mono conversion did not produce a complete sample");
 }
 if(!((FN(0x292eb00,uint64_t(*)(void*,const uintptr_t*))(r.pool,r.converted)>>32)&255)){
  auto added=FN(0x2931440,NativeSamplePair(*)(void*,const uintptr_t*))(r.pool,r.converted);
  if(added.words[1])FN(0x123bca0,void(*)(void*))((void*)added.words[1]);
 }
 if(!r.assignment){
  void*memory=FN(0xb51418,void*(*)(size_t))(0x108);
  try{FN(0x1995f90,void(*)(void*,void*,void*,const uintptr_t*,int,int,void*))
      (memory,r.project,program,r.converted,(int)r.to,0,r.service);}catch(...){FN(0xb51408,void(*)(void*,size_t))(memory,0x108);throw;}
  r.assignment=memory;
 }
 r.assignment_started=true;
 if(((int(*)(void*))F(r.assignment,0,uintptr_t*)[8])(r.assignment))throw std::runtime_error("Mono assignment failed");
 void*target=FN(0x286bbe0,void*(*)(void*,int))(program,(int)r.to);
 void*layer=target?F(target,0x22e8,void*):nullptr,*original=F(r.source,0x22e8,void*);
 if(!layer||!original)throw std::runtime_error("Mono layer missing");
 FN(0x294bca0,void*(*)(void*,void*))((char*)layer+0x740,(char*)original+0x740);
 uintptr_t assigned[2]{};step_layer_sample(target,0,assigned,base+0x2803b90);
 bool ok=assigned[0]==r.converted[0];if(assigned[1])FN(0x123bca0,void(*)(void*))((void*)assigned[1]);
 if(!ok)throw std::runtime_error("Mono sample was not assigned");
 std::fprintf(stderr,"MONO APPLIED source=%u target=%u channel=%d frames=%d\n",r.from,r.to,r.mono,FN(0x2941f80,int(*)(void*))((void*)r.converted[0]));
}
struct Batch{
 alignas(8) unsigned char header[0x50];
 std::vector<Row> rows;
 void *event_commands=nullptr,*project=nullptr,*things=nullptr;
 bool copy_events=false;
 bool ready=false;
 explicit Batch(bool chops,bool mono=false):header{} {
  uint64_t title=0;FN(0xf535c0,void(*)(uint64_t*,const char*))(&title,mono?"Stereo to Mono":chops?"Export Chops":"Copy Pads Batch");
  FN(0x19c9850,void(*)(void*,const uint64_t*,int,int))(this,&title,1,0);
  FN(0xf0a290,void(*)(uint64_t*))(&title);F(this,0,uintptr_t*)=vtable;
 }
 ~Batch(){
  batch_events_free(event_commands);
  for(auto&r:rows){native_delete(r.assignment);if(r.converted[1])FN(0x123bca0,void(*)(void*))((void*)r.converted[1]);if(r.mono_name)FN(0xf0a290,void(*)(uint64_t*))(&r.mono_name);native_delete(r.extract);if(r.slice){FN(0x294a7f0,void(*)(void*))(r.slice);FN(0xb51408,void(*)(void*,size_t))(r.slice,0x368);}if(r.source){auto v=F(r.source,0,uintptr_t*);((void(*)(void*))v[1])(r.source);}if(r.command)FN(0x1986890,void(*)(void*))(r.command);}
  FN(0xf0a290,void(*)(void*))((char*)this+0x38);FN(0xf0a290,void(*)(void*))((char*)this+0x30);FN(0x236f110,void(*)(void*))((char*)this+8);
 }
 void*program()const{return rows.empty()?nullptr:FN(0x19870e0,void*(*)(void*))(rows[0].command);}
 int perform(){
  void*p=program();if(!p)return 1;
  if(!ready){
   /* All potentially allocating source snapshots precede the first write. */
   if(copy_events){
    /* Row is not a packed array: pass owned source/target arrays explicitly. */
    unsigned from[BATCH_MAX_ITEMS],to[BATCH_MAX_ITEMS];
    for(unsigned i=0;i<rows.size();++i){from[i]=rows[i].from;to[i]=rows[i].to;}
    event_commands=batch_events_prepare(base,project,things,p,from,to,(unsigned)rows.size());
    if(!event_commands)return 1;
   }
   unsigned n=0;for(auto&r:rows){
    progress(BATCH_JOB_PREPARE,n,(unsigned)rows.size());
    void*s=FN(0x286bbe0,void*(*)(void*,int))(p,r.from);if(!s)return 1;
    auto v=F(s,0,uintptr_t*);Pair info=((Pair(*)(void*,int))v[7])(s,0);
    r.source=((void*(*)(void*,int,const Pair*))v[14])(s,0,&info);if(!r.source)return 1;
    r.map=F(p,0x16a8+(r.from+0x84)*4,int);
#ifdef MPC_PAD_EQ
    r.source_eq=music_get_pad_eq(r.from);r.target_eq=music_get_pad_eq(r.to);
#endif
    ++n;
   }
   ready=true;
  }
  size_t done=0;
  try{
   for(auto&r:rows){
    progress(BATCH_JOB_COPY,(unsigned)done,(unsigned)rows.size());
    snapshot=r.source;snapshot_program=p;snapshot_source=(int)r.from;
    /* Native source key is read from its original pad. Temporarily supply the
     * captured key under the command executor lock, then restore that pad's
     * current key. This also preserves earlier overlapping destinations. */
    int previous=F(p,0x16a8+(r.from+0x84)*4,int);F(p,0x16a8+(r.from+0x84)*4,int)=r.map;
    int result=0;
    try{if(r.mono<0||r.from!=r.to){result=FN(0x1987530,int(*)(void*))(r.command);r.copy_started=result==0;}}catch(...){F(p,0x16a8+(r.from+0x84)*4,int)=previous;throw;}
    F(p,0x16a8+(r.from+0x84)*4,int)=previous;snapshot=nullptr;snapshot_program=nullptr;snapshot_source=-1;
    if(result)throw std::runtime_error("Native copy failed");
#ifdef MPC_PAD_EQ
    music_set_pad_eq(r.to,r.source_eq);r.eq_written=true;
#endif
    ++done;
    if(r.mono>=0)mono_perform(r,p);
    if(r.extract){
     r.extraction_started=true;
     std::fprintf(stderr,"CHOP EXTRACT_BEGIN row=%zu source=%u target=%u start=%lld end=%lld\n",done,r.from,r.to,
       (long long)F(r.slice,0x78,int64_t),(long long)F(r.slice,0xd0,int64_t));
     int extracted=FN(0x143f0b0,int(*)(void*))(r.extract);
     if(extracted){std::fprintf(stderr,"CHOP EXTRACT_FAILED row=%zu status=%d\n",done,extracted);throw std::runtime_error("Native extract failed");}
     void*inner=F(r.extract,0x98,void*),*created=inner?F(inner,0xb8,void*):nullptr;
     void*target=FN(0x286bbe0,void*(*)(void*,int))(p,(int)r.to);uintptr_t assigned[2]={0};
     if(target&&F(target,0x22e8,void*))step_layer_sample(target,0,assigned,base+0x2803b90);
     int64_t expected=F(r.slice,0xd0,int64_t)-F(r.slice,0x78,int64_t);
     int64_t actual=created&&F(created,0x130,void*)?FN(0x2941f80,int(*)(void*))(created):-1;
     bool valid=!extracted&&created&&assigned[0]==(uintptr_t)created&&actual==expected;
     if(assigned[1])FN(0x123bca0,void(*)(void*))((void*)assigned[1]);
     std::fprintf(stderr,"CHOP EXTRACT_END row=%zu status=%d frames=%lld expected=%lld assigned=%u\n",done,extracted,(long long)actual,(long long)expected,valid?1u:0u);
     if(!valid)throw std::runtime_error("Extract output or assignment failed");
    }
    progress(BATCH_JOB_COPY,(unsigned)done,(unsigned)rows.size());
   }
   if(copy_events&&batch_events_perform(event_commands))throw std::runtime_error("Native event copy failed");
  }catch(...){snapshot=nullptr;snapshot_program=nullptr;snapshot_source=-1;while(done){progress(BATCH_JOB_ROLLBACK,(unsigned)done,(unsigned)rows.size());undo_row(rows[--done]);}return 1;}
  return 0;
 }
 int undo(){if(!program())return 1;int result=copy_events?batch_events_undo(event_commands):0;for(auto i=rows.rbegin();i!=rows.rend();++i)if(undo_row(*i))result=1;return result;}
};
static void destroy(Batch*p){p->~Batch();}
static void delete_batch(Batch*p){delete p;}
static int perform_batch(Batch*p)noexcept{try{return p->perform();}catch(...){snapshot=nullptr;snapshot_program=nullptr;snapshot_source=-1;return 1;}}
static int undo_batch(Batch*p)noexcept{try{return p->undo();}catch(...){return 1;}}
static std::shared_ptr<Batch> prepare(void*context,const unsigned*src,const unsigned*dst,unsigned count,bool chops=false,bool events=false,bool mono=false){
 if(!context||!count||count>BATCH_MAX_ITEMS)return {};
 for(unsigned i=0;i<count;++i){if(src[i]>=128||dst[i]>=128||(!mono&&src[i]==dst[i]))return {};for(unsigned j=0;j<i;++j)if(dst[i]==dst[j])return {};}
 void*things=F(context,0x78,void*);void*program=things?F(things,0xb8,void*):nullptr;void*properties=F(context,0xc8,void*);
 if(!program||!properties)return {};
 void*registry=F(properties,0x1288,void*);void*manager=F(context,0x68,void*);if(!registry||!manager||F(manager,0,uintptr_t)!=base+0x6d97818)return {};
 if(!vtable[2]){std::memcpy(vtable,(void*)(base+0x6e439a0),sizeof(vtable));vtable[0]=(uintptr_t)destroy;vtable[1]=(uintptr_t)delete_batch;vtable[8]=(uintptr_t)perform_batch;vtable[9]=(uintptr_t)undo_batch;}
 auto batch=std::make_shared<Batch>(chops,mono);batch->rows.reserve(count);
 batch->copy_events=events;batch->project=properties;batch->things=things;
 for(unsigned i=0;i<count;++i){
  if(!FN(0x286bbe0,void*(*)(void*,int))(program,src[i]))return {};
  Row r;r.from=src[i];r.to=dst[i];r.command=FN(0xb51418,void*(*)(size_t))(0x4b8);
  uint64_t low=dst[i]<64?UINT64_C(1)<<dst[i]:0,high=dst[i]>=64?UINT64_C(1)<<(dst[i]-64):0;
  try{FN(0x1986f90,void(*)(void*,void*,void*,void*,int,uint64_t,uint64_t))(r.command,properties,registry,program,(int)src[i],low,high);}catch(...){FN(0xb51408,void(*)(void*,size_t))(r.command,0x4b8);throw;}
  batch->rows.push_back(r);
 }
 return batch;
}
static int dispatch(void*context,std::shared_ptr<Batch>&batch){
 if(!batch)return 0;
 /* Context68 already owns CommandManager. Its +60 is a signal field,
  * not another dispatcher pointer. Stock1518378 passes the manager itself. */
 void*manager=F(context,0x68,void*);
 return FN(0x19d0a00,int(*)(void*,const void*))(manager,&batch)==0;
}
extern "C" int batch_copy_submit(uintptr_t b,void*context,const unsigned*src,const unsigned*dst,unsigned count){
 return batch_copy_submit_events(b,context,src,dst,count,0);
}
extern "C" int batch_copy_submit_events(uintptr_t b,void*context,const unsigned*src,const unsigned*dst,unsigned count,int events){
 base=b;if(!src||!dst||!count||count>BATCH_MAX_ITEMS)return 0;try{for(unsigned i=0;i<count;++i)for(unsigned j=0;j<i;++j)if(src[i]==src[j])return 0;auto batch=prepare(context,src,dst,count,false,events!=0);return dispatch(context,batch);}catch(...){return 0;}
}
struct Operation{
 std::shared_ptr<Batch> command;
 void*context;
 std::atomic<unsigned> phase{BATCH_JOB_PREPARE},done{0};
 unsigned total;
 Operation(std::shared_ptr<Batch> c,void*p,unsigned n):command(c),context(p),total(n){}
};
static std::mutex operation_mutex;
static std::shared_ptr<Operation> operation;
static thread_local Operation*running_operation;
static void progress(unsigned phase,unsigned done,unsigned total){
 if(running_operation){running_operation->done.store(done,std::memory_order_relaxed);running_operation->phase.store(phase,std::memory_order_release);}
 std::fprintf(stderr,"BATCH stage=%u done=%u total=%u\n",phase,done,total);
}
struct Action{uintptr_t invoke,manage;void*reference,*job;uintptr_t reserved;};
static_assert(sizeof(Action)==40,"Native Copy task payload");
static void run_operation(void*data)noexcept{
 void*reference=F(data,0,void*);void*identity=F(data,8,void*);
 std::shared_ptr<Operation> active;
 {std::lock_guard<std::mutex> lock(operation_mutex);if(operation.get()==identity)active=operation;}
 if(active){
  running_operation=active.get();bool ok=false;
  try{std::fprintf(stderr,"BATCH DISPATCH_BEGIN count=%u\n",active->total);ok=dispatch(active->context,active->command);}catch(...){ok=false;}
  active->done.store(ok?active->total:0,std::memory_order_relaxed);
  active->phase.store(ok?BATCH_JOB_DONE:BATCH_JOB_FAILED,std::memory_order_release);
  running_operation=nullptr;std::fprintf(stderr,"BATCH DISPATCH_END success=%u\n",ok?1:0);
 }
 __atomic_fetch_sub((unsigned*)((char*)reference+8),1,__ATOMIC_RELEASE);
}
static int enqueue_operation(void*context,std::shared_ptr<Batch> command,unsigned count){
 if(!command)return 0;
 try{
  {std::lock_guard<std::mutex> lock(operation_mutex);if(operation)return 0;}
  auto active=std::make_shared<Operation>(command,context,count);
  void*owner=F(context,0x48,void*);if(!owner)return 0;
  {std::lock_guard<std::mutex> lock(operation_mutex);if(operation)return 0;operation=active;}
  /* Same context lifetime counter, queue, mutex and trivial closure manager
   * as native Copy1517e34..1517e98. No new worker or observer thread. */
  void*reference=(char*)context+0x48;
  Action action={(uintptr_t)run_operation,base+0x1517b30,reference,active.get(),0};
  __atomic_fetch_add((unsigned*)((char*)context+0x50),1,__ATOMIC_ACQUIRE);
  void*mutex=(char*)owner+0x220;
  FN(0xf40e50,void(*)(void*))(mutex);
  int accepted=FN(0x2b59300,int(*)(void*,void*,uintptr_t))((char*)owner+0x1d8,&action,(uintptr_t)reference&~(uintptr_t)3)&255;
  FN(0xf40e90,void(*)(void*))(mutex);
  ((int(*)(int,void*,void*))action.manage)(1,&action.reference,nullptr);
  if(!accepted){__atomic_fetch_sub((unsigned*)((char*)context+0x50),1,__ATOMIC_RELEASE);std::lock_guard<std::mutex> lock(operation_mutex);operation.reset();return 0;}
  std::fprintf(stderr,"BATCH QUEUED count=%u\n",count);return 1;
 }catch(...){return 0;}
}
extern "C" int batch_copy_begin(uintptr_t b,void*context,const unsigned*src,const unsigned*dst,unsigned count){
 return batch_copy_begin_events(b,context,src,dst,count,0);
}
extern "C" int batch_copy_begin_events(uintptr_t b,void*context,const unsigned*src,const unsigned*dst,unsigned count,int events){
 base=b;if(!context||!src||!dst||!count||count>BATCH_MAX_ITEMS)return 0;
 try{for(unsigned i=0;i<count;++i)for(unsigned j=0;j<i;++j)if(src[i]==src[j])return 0;
  return enqueue_operation(context,prepare(context,src,dst,count,false,events!=0),count);
 }catch(...){return 0;}
}
extern "C" void batch_copy_poll(struct batch_copy_progress*out){
 if(!out)return;
 std::lock_guard<std::mutex> lock(operation_mutex);
 if(!operation){*out={BATCH_JOB_IDLE,0,0};return;}
 out->phase=operation->phase.load(std::memory_order_acquire);out->done=operation->done.load(std::memory_order_relaxed);out->total=operation->total;
}
extern "C" int batch_copy_release(void){
 std::shared_ptr<Operation> previous;
 {std::lock_guard<std::mutex> lock(operation_mutex);if(!operation)return 1;
  if(operation->phase.load(std::memory_order_acquire)<BATCH_JOB_DONE)return 0;
  previous.swap(operation);}
 return 1;
}
static std::atomic<unsigned> exit_state{BATCH_EXIT_IDLE};
static std::atomic<int> hold_native_release_exit{0};
extern "C" void batch_copy_hold_native_exit(int hold){hold_native_release_exit.store(hold,std::memory_order_release);}
extern "C" int batch_copy_native_release_exit(void*mode,int reason,int id){
 if(id==0x30ed&&hold_native_release_exit.load(std::memory_order_acquire)){std::fprintf(stderr,"BATCH NATIVE_GESTURE_EXIT_DEFERRED\n");return id;}
 return FN(0x1c0d990,int(*)(void*,int,int))(mode,reason,id);
}
static void run_exit(void*data)noexcept{
 void*reference=F(data,0,void*);void*mode=F(data,8,void*);
 unsigned result=BATCH_EXIT_FAILED;
 try{
  int before=F(mode,0xd8,int);
  int removed=FN(0x1c0d990,int(*)(void*,int,int))(mode,0,0x30ed);
  int after=F(mode,0xd8,int);
  /* EngineMode3080 means its modal stack is empty; base mode80 still plays.
   * A different modal screen is not removed as a fallback. */
  result=after!=0x30ed?BATCH_EXIT_DONE:BATCH_EXIT_FAILED;
  std::fprintf(stderr,"BATCH EXIT before=%x removed=%x after=%x success=%u\n",before,removed,after,result==BATCH_EXIT_DONE);
 }catch(...){std::fprintf(stderr,"BATCH EXIT exception\n");}
 __atomic_fetch_sub((unsigned*)((char*)reference+8),1,__ATOMIC_RELEASE);
 exit_state.store(result,std::memory_order_release);
}
extern "C" unsigned batch_copy_exit_poll(void){return exit_state.load(std::memory_order_acquire);}
extern "C" void batch_copy_exit_reset(void){unsigned n=exit_state.load(std::memory_order_acquire);if(n!=BATCH_EXIT_PENDING)exit_state.store(BATCH_EXIT_IDLE,std::memory_order_release);}
extern "C" int batch_copy_exit_begin(uintptr_t b,void*context){
 base=b;if(!context)return 0;
 {std::lock_guard<std::mutex> lock(operation_mutex);if(operation)return 0;}
 void*owner=F(context,0x48,void*),*mode=F(context,0x98,void*);
 if(!owner||!mode)return 0;
 unsigned expected=BATCH_EXIT_IDLE;
 if(!exit_state.compare_exchange_strong(expected,BATCH_EXIT_PENDING,std::memory_order_acq_rel))return 0;
 void*reference=(char*)context+0x48;
 Action action={(uintptr_t)run_exit,base+0x1517b30,reference,mode,0};
 __atomic_fetch_add((unsigned*)((char*)context+0x50),1,__ATOMIC_ACQUIRE);
 void*mutex=(char*)owner+0x220;
 FN(0xf40e50,void(*)(void*))(mutex);
 int accepted=FN(0x2b59300,int(*)(void*,void*,uintptr_t))((char*)owner+0x1d8,&action,(uintptr_t)reference&~(uintptr_t)3)&255;
 FN(0xf40e90,void(*)(void*))(mutex);
 ((int(*)(int,void*,void*))action.manage)(1,&action.reference,nullptr);
 if(!accepted){__atomic_fetch_sub((unsigned*)((char*)context+0x50),1,__ATOMIC_RELEASE);exit_state.store(BATCH_EXIT_FAILED,std::memory_order_release);return 0;}
 std::fprintf(stderr,"BATCH EXIT_QUEUED\n");return 1;
}
static std::atomic<unsigned> preview_state{BATCH_PREVIEW_IDLE};
static void*active_preview_input;static unsigned active_preview_pad=128;
static void*preview_input(void*context){
 void*mode=F(context,0x98,void*);if(!mode)return nullptr;
 void*node=F(mode,0x168,void*);void*found=nullptr;
 for(unsigned n=0;node&&n<128;++n){int key=F(node,0x20,int);if(key==0x20a7){found=F(node,0x28,void*);break;}node=F(node,key>0x20a7?0x10:0x18,void*);}
 if(!found||F(found,0,uintptr_t)!=base+0x6e10488||F(found,0x48,uintptr_t)!=base+0x6e104e0)return nullptr;
 void*interaction=F(found,0x48+0xb0,void*);
 if(!interaction||F(interaction,0,uintptr_t)!=base+0x6d8d850)return nullptr;
 return F(interaction,0x60,void*);
}
static bool preview_recording(void*input){
 void*engine=F(input,0xd8,void*),*transport=engine?F(engine,0x768,void*):nullptr;
 return !transport||!F(transport,0x188,void*)||!FN(0x2133870,int(*)(void*))(transport);
}
static void run_preview(void*data)noexcept{
 void*reference=F(data,0,void*),*input=F(data,8,void*);uintptr_t value=F(data,16,uintptr_t);unsigned pad=value&127;int down=(value>>8)&1;
 try{
  if(down&&preview_recording(input))preview_state.store(BATCH_PREVIEW_RECORDING,std::memory_order_release);
  else if(down){FN(0x1e27360,void(*)(void*,int,int,void*,float))(input,0,(int)pad,nullptr,1.f);active_preview_input=input;active_preview_pad=pad;preview_state.store(BATCH_PREVIEW_PLAYING,std::memory_order_release);}
  else{if(active_preview_input==input&&active_preview_pad==pad){FN(0x1e270d0,void(*)(void*,int,int))(input,0,(int)pad);active_preview_input=nullptr;active_preview_pad=128;}preview_state.store(BATCH_PREVIEW_IDLE,std::memory_order_release);}
 }catch(...){preview_state.store(BATCH_PREVIEW_FAILED,std::memory_order_release);}
 __atomic_fetch_sub((unsigned*)((char*)reference+8),1,__ATOMIC_RELEASE);
}
extern "C" unsigned batch_copy_preview_status(void){return preview_state.load(std::memory_order_acquire);}
extern "C" int batch_copy_preview(uintptr_t b,void*context,unsigned source,int down){
 base=b;if(!context||source>=128)return 0;
 void*owner=F(context,0x48,void*),*input=preview_input(context);if(!owner||!input)return 0;
 /* Use the ordinary source-pad audio input, not the physical Copy pad route.
  * Record mode must be None; the same check repeats on the worker. Never
  * change transport, record arming, current pad, event list or bank. */
 if(down&&preview_recording(input)){preview_state.store(BATCH_PREVIEW_RECORDING,std::memory_order_release);return 2;}
 void*reference=(char*)context+0x48;
 Action action={(uintptr_t)run_preview,base+0x1517b30,reference,input,(uintptr_t)source|((uintptr_t)(down!=0)<<8)};
 __atomic_fetch_add((unsigned*)((char*)context+0x50),1,__ATOMIC_ACQUIRE);
 if(down)preview_state.store(BATCH_PREVIEW_QUEUED,std::memory_order_release);
 void*mutex=(char*)owner+0x220;FN(0xf40e50,void(*)(void*))(mutex);
 int accepted=FN(0x2b59300,int(*)(void*,void*,uintptr_t))((char*)owner+0x1d8,&action,(uintptr_t)reference&~(uintptr_t)3)&255;
 FN(0xf40e90,void(*)(void*))(mutex);((int(*)(int,void*,void*))action.manage)(1,&action.reference,nullptr);
 if(!accepted){__atomic_fetch_sub((unsigned*)((char*)context+0x50),1,__ATOMIC_RELEASE);preview_state.store(BATCH_PREVIEW_FAILED,std::memory_order_release);return 0;}
 return 1;
}
struct EventPreviewNote {int64_t time;uint32_t type,padding;unsigned char data[40];};
static_assert(sizeof(EventPreviewNote)==56,"Event ABI");
struct EventPreviewSlot {
 std::atomic<unsigned> used{0};void*context;void*input;void*program;
 EventPreviewNote note;unsigned key,mode,down,source;uint64_t epoch;
};
static EventPreviewSlot event_slots[64];
static std::atomic<uint64_t> event_epoch{0},event_stop_requested{0};
static std::atomic<unsigned> event_status{BATCH_PREVIEW_IDLE};
static std::atomic<unsigned> event_pending{0};
static void*event_active_input;static void*event_active_context;
static unsigned event_active_note;static uint64_t event_active_epoch;
static thread_local const EventPreviewNote*event_override_note;
static thread_local void*event_override_properties;
static thread_local bool event_override_used;
extern "C" unsigned batch_event_preview_status(void){return event_status.load(std::memory_order_acquire);}
extern "C" unsigned batch_event_preview_pending(void){return event_pending.load(std::memory_order_acquire);}
extern "C" int batch_event_preview_override(void*properties,void*generated){
 if(!event_override_note||properties!=event_override_properties)return 0;
 unsigned char source=F(generated,0x30,unsigned char);
 std::memcpy(generated,event_override_note,56);
 /* The ordinary Sample input has not mapped pad index -> MIDI yet. */
 F(generated,0x30,unsigned char)=source;
 F(generated,0,int64_t)=0;F(generated,8,uint32_t)&=~0x300u;
 event_override_used=true;return 1;
}
static void stop_event_voice(){
 if(event_active_input){
  /* Native SixteenLevelsPadMode15450a4: zero-velocity release uses source1
   * and properties+158 (the original source pad), not the physical key. */
  FN(0x1e270d0,void(*)(void*,int,int))(event_active_input,1,(int)event_active_note);
  event_active_input=event_active_context=nullptr;event_active_epoch=0;
 }
}
static void*event_context_program(void*c){void*t=F(c,0x78,void*);return t?F(t,0xb8,void*):nullptr;}
static void run_event_preview(void*data)noexcept{
 void*reference=F(data,0,void*);auto*s=static_cast<EventPreviewSlot*>(F(data,8,void*));
 try{
  if(!s->down){
   if(event_active_context==s->context&&event_active_epoch<=s->epoch)stop_event_voice();
   event_status.store(BATCH_PREVIEW_IDLE,std::memory_order_release);
  }else if(s->epoch!=event_epoch.load(std::memory_order_acquire)){
   /* A newer choice or Cancel superseded this queued press. */
  }else if(event_context_program(s->context)!=s->program||s->source>=128||F(s->program,0x14a8+s->note.data[0x20]*4,int)!=(int)s->source||F(s->program,0x12a8+s->source*4,int)!=s->note.data[0x20]){event_status.store(BATCH_PREVIEW_FAILED,std::memory_order_release);
  }else if(preview_recording(s->input)){event_status.store(BATCH_PREVIEW_RECORDING,std::memory_order_release);
  }else{
   stop_event_voice();
   if(s->mode==8){
    /* Native Sample1e27360 sends the MIDI service's result to211b090.
     * A staged Chop already owns its complete Slice15 value: do not ask
     * the live MIDI/Chop mapper to reconstruct that result a second time.
     * Use the identical ordinary-pad receiver, source0 and mask, on the
     * same worker. It expects a raw pad index;2115070 normalizes it later.
     * Keep the verified Levels path below unchanged. */
    void*engine=F(s->input,0xd8,void*),*things=engine?F(engine,0x3c0,void*):nullptr;
    if(!things||F(things,0xb8,void*)!=s->program){event_status.store(BATCH_PREVIEW_FAILED,std::memory_order_release);}
    else{
     EventPreviewNote raw=s->note;raw.time=0;raw.type&=~0x300u;raw.data[0x20]=(unsigned char)s->source;
     FN(0x1e270d0,void(*)(void*,int,int))(s->input,0,(int)s->source);
     FN(0x211b090,void(*)(void*,int64_t,const void*,unsigned))(engine,0,&raw,0x7ffffffa);
     event_active_input=s->input;event_active_context=s->context;event_active_note=s->source;event_active_epoch=s->epoch;
     event_status.store(BATCH_PREVIEW_PLAYING,std::memory_order_release);
    }
   }else{
   alignas(8) unsigned char properties[0x230]={};
   F(properties,0x88,int)=(int)s->mode;F(properties,0x158,int)=(int)s->source;F(properties,0x228,int)=3;
   struct OverrideScope {
    const EventPreviewNote*old_note;void*old_properties;bool old_used;
    OverrideScope(const EventPreviewNote*n,void*p):old_note(event_override_note),old_properties(event_override_properties),old_used(event_override_used){event_override_note=n;event_override_properties=p;event_override_used=false;}
    ~OverrideScope(){event_override_note=old_note;event_override_properties=old_properties;event_override_used=old_used;}
   } scope(&s->note,properties);
   float velocity=F(&s->note,0x10,float);
   FN(0x1e27360,void(*)(void*,int,int,void*,float))(s->input,0,(int)s->key,properties,velocity);
   if(event_override_used){
    event_active_input=s->input;event_active_context=s->context;event_active_note=s->source;event_active_epoch=s->epoch;
    event_status.store(BATCH_PREVIEW_PLAYING,std::memory_order_release);
   }else{event_status.store(BATCH_PREVIEW_FAILED,std::memory_order_release);}
   }
  }
  if(event_active_input&&event_active_epoch<=event_stop_requested.load(std::memory_order_acquire))stop_event_voice();
 }catch(...){event_override_note=nullptr;event_override_properties=nullptr;event_status.store(BATCH_PREVIEW_FAILED,std::memory_order_release);}
 s->used.store(0,std::memory_order_release);
 event_pending.fetch_sub(1,std::memory_order_release);
 __atomic_fetch_sub((unsigned*)((char*)reference+8),1,__ATOMIC_RELEASE);
}
static int queue_event_preview(void*context,void*input,void*program,const EventPreviewNote*note,unsigned key,unsigned mode,uint64_t epoch){
 EventPreviewSlot*s=nullptr;
 for(auto&slot:event_slots){unsigned expected=0;if(slot.used.compare_exchange_strong(expected,1,std::memory_order_acq_rel)){s=&slot;break;}}
 if(!s)return 0;
 s->context=context;s->input=input;s->program=program;s->key=key;s->mode=mode;s->epoch=epoch;s->down=note!=nullptr;if(note)s->note=*note;
 if(note)s->source=(unsigned)F(program,0x14a8+note->data[0x20]*4,int);
 void*owner=F(context,0x48,void*),*reference=(char*)context+0x48;
 Action action={(uintptr_t)run_event_preview,base+0x1517b30,reference,s,0};
 __atomic_fetch_add((unsigned*)((char*)reference+8),1,__ATOMIC_ACQUIRE);
 event_pending.fetch_add(1,std::memory_order_acq_rel);
 void*mutex=(char*)owner+0x220;FN(0xf40e50,void(*)(void*))(mutex);
 int accepted=FN(0x2b59300,int(*)(void*,void*,uintptr_t))((char*)owner+0x1d8,&action,(uintptr_t)reference&~(uintptr_t)3)&255;
 FN(0xf40e90,void(*)(void*))(mutex);((int(*)(int,void*,void*))action.manage)(1,&action.reference,nullptr);
 if(!accepted){__atomic_fetch_sub((unsigned*)((char*)reference+8),1,__ATOMIC_RELEASE);s->used.store(0,std::memory_order_release);event_pending.fetch_sub(1,std::memory_order_release);return 0;}
 return 1;
}
extern "C" int batch_event_preview(uintptr_t b,void*c,void*program,const void*event,unsigned key,unsigned mode){
 base=b;if(!c||!program||!event||key>=16||mode>=10||!F(c,0x48,void*)||event_context_program(c)!=program)return 0;
 auto*n=static_cast<const EventPreviewNote*>(event);float velocity=F(n,0x10,float);
 if((n->type&255)!=3||n->data[0x20]>=128||!std::isfinite(velocity)||velocity<0||velocity>1.01f)return 0;
 int source=F(program,0x14a8+n->data[0x20]*4,int);if(source<0||source>=128||F(program,0x12a8+source*4,int)!=n->data[0x20])return 0;
 void*input=preview_input(c);if(!input)return 0;
 if(preview_recording(input)){event_status.store(BATCH_PREVIEW_RECORDING,std::memory_order_release);return 2;}
 uint64_t epoch=event_epoch.fetch_add(1,std::memory_order_acq_rel)+1;
 event_status.store(BATCH_PREVIEW_QUEUED,std::memory_order_release);
 if(!queue_event_preview(c,input,program,n,key,mode,epoch)){event_status.store(BATCH_PREVIEW_FAILED,std::memory_order_release);return 0;}
 return 1;
}
extern "C" int batch_event_preview_stop(uintptr_t b,void*c,int cancel){
 base=b;uint64_t epoch=cancel?event_epoch.fetch_add(1,std::memory_order_acq_rel):event_epoch.load(std::memory_order_acquire);
 event_stop_requested.store(epoch,std::memory_order_release);
 if(!c||!F(c,0x48,void*))return 0;
 return queue_event_preview(c,nullptr,nullptr,nullptr,0,0,epoch);
}
static std::shared_ptr<Batch> prepare_chops(void*context,void*services,unsigned source,const unsigned*slices,const unsigned*dst,unsigned count){
 std::fprintf(stderr,"CHOP PREPARE source=%u count=%u\n",source,count);
 if(!context||!services||!slices||!dst||source>=128||!count||count>BATCH_MAX_ITEMS)return {};
 uintptr_t sample[2]={0};
 try{
  void*things=F(context,0x78,void*),*program=things?F(things,0xb8,void*):nullptr;void*instrument=program?FN(0x286bbe0,void*(*)(void*,int))(program,source):nullptr;
  if(!instrument||!F(instrument,0x22e8,void*)||F(instrument,0x2310,void*)!=program||!F(context,0x48,void*)||!F(context,0x70,void*))return {};
  step_layer_sample(instrument,0,sample,base+0x2803b90);if(!sample[0]||!F((void*)sample[0],0x130,void*))throw std::runtime_error("Sample data missing");
  int64_t source_frames=FN(0x2941f80,int(*)(void*))((void*)sample[0]);
  void*data=FN(0x2941ff0,void*(*)(void*))((void*)sample[0]);uintptr_t first=F(data,0x60,uintptr_t),end=F(data,0x68,uintptr_t);
  if(!first||end<first||(end-first)%8||end-first>128*8)throw std::runtime_error("Invalid slices");
  unsigned total=(unsigned)((end-first)/8),src[BATCH_MAX_ITEMS];
  for(unsigned i=0;i<count;++i){src[i]=source;if(slices[i]>=total||dst[i]==source)throw std::runtime_error("Invalid slice target");for(unsigned j=0;j<i;++j)if(slices[i]==slices[j])throw std::runtime_error("Repeated slice");}
  auto batch=prepare(context,src,dst,count,true);if(!batch)throw std::runtime_error("Invalid mapping");
  void*functions=lookup(services,base+0x6e09000);if(!functions)throw std::runtime_error("SampleEditFunctions missing");
  void*project=F(context,0xc8,void*);void*pool=F(project,0x1278,void*);if(!pool)throw std::runtime_error("Sample pool missing");
  /* All metadata is copied into native property objects before any target
   * changes; no source marker or selected Chop index is modified. */
  uint64_t frames=0;
  for(unsigned i=0;i<count;++i){
   auto&r=batch->rows[i];void*original=F((void*)first,slices[i]*8,void*);if(!original)throw std::runtime_error("Empty slice");
   r.slice=FN(0xb51418,void*(*)(size_t))(0x368);
   try{FN(0x2949dd0,void(*)(void*,int64_t,int64_t,int64_t,int,int64_t))(r.slice,0,0,1,0,0);}catch(...){FN(0xb51408,void(*)(void*,size_t))(r.slice,0x368);r.slice=nullptr;throw;}
   FN(0x294bca0,void*(*)(void*,void*))(r.slice,original);
   if(F(r.slice,0x78,int64_t)<0||F(r.slice,0xd0,int64_t)<=F(r.slice,0x78,int64_t)||F(r.slice,0xd0,int64_t)>source_frames)throw std::runtime_error("Invalid slice range");
   frames+=(uint64_t)(F(r.slice,0xd0,int64_t)-F(r.slice,0x78,int64_t));if(frames>8388608)throw std::runtime_error("First trial render limit64MiB");
   char text[64];std::snprintf(text,sizeof(text),"Chop-%c%02u-%03u",'A'+source/16,source%16+1,slices[i]+1);
   uint64_t candidate=0,name=0;uintptr_t none[2]={0};FN(0xf535c0,void(*)(uint64_t*,const char*))(&candidate,text);
   batch_name_result(pool,&candidate,none,&name,base+0x292f0e0);FN(0xf0a290,void(*)(uint64_t*))(&candidate);
   void*memory=FN(0xb51418,void*(*)(size_t))(0xb0);
   try{FN(0x143ed40,void(*)(void*,const uint64_t*,int,void*,void*,void*,void*,void*,void*))(memory,&name,(int)dst[i],r.slice,instrument,project,functions,F(context,0x70,void*),F(context,0x48,void*));}catch(...){FN(0xb51408,void(*)(void*,size_t))(memory,0xb0);FN(0xf0a290,void(*)(uint64_t*))(&name);throw;}
   r.extract=memory;FN(0xf0a290,void(*)(uint64_t*))(&name);
  }
  FN(0x123bca0,void(*)(void*))((void*)sample[1]);sample[1]=0;
  return batch;
 }catch(const std::exception&e){std::fprintf(stderr,"CHOP PREPARE_FAILED reason=%s\n",e.what());if(sample[1])FN(0x123bca0,void(*)(void*))((void*)sample[1]);return {};}
 catch(...){std::fprintf(stderr,"CHOP PREPARE_FAILED unknown_exception\n");if(sample[1])FN(0x123bca0,void(*)(void*))((void*)sample[1]);return {};}
}
extern "C" int batch_chop_submit(uintptr_t b,void*context,void*services,unsigned source,const unsigned*slices,const unsigned*dst,unsigned count){
 base=b;auto command=prepare_chops(context,services,source,slices,dst,count);return dispatch(context,command);
}
extern "C" int batch_chop_begin(uintptr_t b,void*context,void*services,unsigned source,const unsigned*slices,const unsigned*dst,unsigned count){
 base=b;
 try{return enqueue_operation(context,prepare_chops(context,services,source,slices,dst,count),count);}catch(...){return 0;}
}
static std::shared_ptr<Batch> prepare_mono(void*context,void*services,unsigned source,unsigned target,unsigned channel){
 if(!context||!services||source>=128||target>=128||channel>1)return {};
 auto command=prepare(context,&source,&target,1,false,false,true);if(!command)return {};
 auto&r=command->rows[0];r.mono=(int)channel;r.functions=lookup(services,base+0x6e09000);
 r.project=F(context,0xc8,void*);r.pool=r.project?F(r.project,0x1278,void*):nullptr;r.service=F(context,0x48,void*);
 if(!r.functions||!r.pool||!r.service)return {};
 char title[48];std::snprintf(title,sizeof(title),"Mono-%c%02u-%c",'A'+source/16,source%16+1,channel?'R':'L');
 uint64_t candidate=0;uintptr_t none[2]{};FN(0xf535c0,void(*)(uint64_t*,const char*))(&candidate,title);
 batch_name_result(r.pool,&candidate,none,&r.mono_name,base+0x292f0e0);FN(0xf0a290,void(*)(uint64_t*))(&candidate);
 return command;
}
extern "C" int batch_mono_begin(uintptr_t b,void*c,void*s,unsigned source,unsigned target,unsigned channel){
 base=b;try{return enqueue_operation(c,prepare_mono(c,s,source,target,channel),1);}catch(...){return 0;}
}
extern "C" int batch_mono_submit(uintptr_t b,void*c,void*s,unsigned source,unsigned target,unsigned channel){
 base=b;try{auto command=prepare_mono(c,s,source,target,channel);return dispatch(c,command);}catch(...){return 0;}
}
