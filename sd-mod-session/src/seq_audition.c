#define _POSIX_C_SOURCE 200809L
#include "seq_audition.h"
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "music_settings.h"
#define FIELD(p,o,t) (*(t*)((char*)(p)+(o)))
#define FN(o,t) ((t)(base+(o)))
#define LAYER 0xb30
#define L_SAMPLE 0x4a8
#define L_RANGE 0x740
#define R_FIRST 0x78
#define R_LAST 0xd0
#define L_LOOP (0x740+0x188+0x50)
#define GRAVE_MS 30000u
/* The newest retired candidates are never timed out: the stock Sample window
 * may still hold the raw pointer of the last sample that was swapped in (a
 * crash in the waveform painter after leaving the browser, r30b/dev7). They
 * are released only when newer ones push them out. */
#define GRAVE_KEEP 4u
struct retired_sample {uintptr_t ctrl;uint64_t due;struct retired_sample*next;};
__attribute__((weak)) void step_layer_sample(void*,int,uintptr_t*,uintptr_t);
static uintptr_t base;
static struct seq_audition_ops O;
struct smp{uintptr_t ptr;int64_t first,last,frames;};
static struct{
 _Atomic int active,hold,swapped;
 void*program;unsigned pad;void*inst;
 struct smp orig,cand;uintptr_t cand_ctrl;
 struct retired_sample*grave,*reserve;
 int preview_paused,waiting_data;
 _Atomic unsigned preview_resume;
 unsigned swaps;
 int pad_root,override_set,override_root,tune_written;
 int8_t orig_tune,written_tune;
}A;
static atomic_flag lock_flag=ATOMIC_FLAG_INIT;
static unsigned save_depth;
static void lock(void){while(atomic_flag_test_and_set_explicit(&lock_flag,memory_order_acquire))sched_yield();}
static void unlock(void){atomic_flag_clear_explicit(&lock_flag,memory_order_release);}
static void say(const char*what,const char*extra){fprintf(stderr,"SEQ_AUDITION %s %s pad=%u swaps=%u\n",what,extra?extra:"",A.pad,A.swaps);}
static void*d_instrument(void*p,unsigned pad){return FN(0x286bbe0,void*(*)(void*,int))(p,(int)pad);}
static int d_get(void*inst,uintptr_t out[2]){
 out[0]=out[1]=0;if(!inst||!FIELD(inst,0x22e8,void*)||!step_layer_sample)return 0;
 step_layer_sample(inst,0,out,base+0x2803b90);return out[0]&&out[1];
}
static void d_release(uintptr_t ctrl){if(ctrl)FN(0x123bca0,void(*)(void*))((void*)ctrl);}
static int64_t d_frames(uintptr_t s){return FN(0x2941f80,int(*)(void*))((void*)s);}
static int d_chopped(uintptr_t s){
 void*d=FN(0x2941ff0,void*(*)(void*))((void*)s);if(!d)return 1;
 /* Slice list = 8-byte pointers. Opening a pad in Chop leaves one slice
  * that is the whole sample; that is not a chopped pad (audition works).
  * Anything malformed is refused. */
 uintptr_t b=FIELD(d,0x60,uintptr_t),e=FIELD(d,0x68,uintptr_t);
 if(e<b||(e-b)%8||e-b>128*8)return 1;
 return (e-b)/8>1;
}
static int d_looping(void*layer){return FIELD(layer,L_LOOP,unsigned)!=0;}
__attribute__((weak)) int music_get_sample(const char*,struct music_sample*);
static int d_root_of(uintptr_t s){
 if(!music_get_sample||!s)return -1;
 const char*name=FIELD((void*)s,0xa8,const char*);
 size_t n=name?strnlen(name,MUSIC_NAME_BYTES):0;
 struct music_sample m;
 if(!n||n>=MUSIC_NAME_BYTES)return -1;
 if(!music_get_sample(name,&m))return -1;
 return m.root>=0&&m.root<12?m.root:-1;
}
static int d_ready(uintptr_t s){return s&&FIELD((void*)s,0x130,void*)!=NULL;}
static uint64_t d_now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return(uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
void seq_audition_init(uintptr_t b){
 base=b;O.instrument=d_instrument;O.get_sample=d_get;O.release=d_release;O.frames=d_frames;O.chopped=d_chopped;O.looping=d_looping;O.now_ms=d_now;O.root_of=d_root_of;O.ready=d_ready;
}
void seq_audition_set_ops(const struct seq_audition_ops*ops){if(ops)O=*ops;}
int seq_audition_plan(int64_t af,int64_t al,int64_t ax,int64_t bf,int64_t bl,int64_t bx,int64_t*mf,int64_t*ml){
 if(af<0||al<=af||al>ax||bf<0||bl<=bf||bl>bx)return 0;
 if(mf)*mf=af<bf?af:bf;
 if(ml)*ml=al<bl?al:bl;
 return 1;
}
static void wr(char*layer,int off,int64_t v){__atomic_store_n((int64_t*)(layer+L_RANGE+off),v,__ATOMIC_RELEASE);}
/* Every intermediate state is a valid (sample,range) pair for the audio
 * thread: shrink to the overlap of both ranges, swap the pointer, then grow. */
static int move(char*layer,const struct smp*a,const struct smp*b){
 int64_t mf,ml;if(!seq_audition_plan(a->first,a->last,a->frames,b->first,b->last,b->frames,&mf,&ml))return 0;
 wr(layer,R_FIRST,mf);wr(layer,R_LAST,ml);
 __atomic_store_n((uintptr_t*)(layer+L_SAMPLE),b->ptr,__ATOMIC_RELEASE);
 wr(layer,R_LAST,b->last);wr(layer,R_FIRST,b->first);return 1;
}
static char*layer_of(void*program,unsigned pad,void**inst_out){
 void*inst=program&&O.instrument?O.instrument(program,pad):NULL;
 if(!inst||FIELD(inst,0x2310,void*)!=program)return NULL;
 uintptr_t b=FIELD(inst,0x22e8,uintptr_t),e=FIELD(inst,0x22f0,uintptr_t);
 if(!b||e<b||e-b!=8*LAYER)return NULL;
 if(inst_out)*inst_out=inst;
 return(char*)b;
}
static int pick_root(void){
 if(A.override_set)return A.override_root;
 return O.root_of&&A.cand.ptr?O.root_of(A.cand.ptr):-1;
}
static int shift_value(void){
 int p=A.pad_root,c=pick_root();
 if(p<0||p>11||c<0||c>11)return 0;
 int d=((p-c)%12+12)%12;
 return d>5?d-12:d;
}
static void apply_tune(char*l){
 int t=A.orig_tune+shift_value();
 if(t<-128)t=-128;
 if(t>127)t=127;
 FIELD(l,0x60,int8_t)=(int8_t)t;A.written_tune=(int8_t)t;A.tune_written=1;
}
static void restore_tune(char*l){
 if(A.tune_written&&FIELD(l,0x60,int8_t)==A.written_tune)FIELD(l,0x60,int8_t)=A.orig_tune;
 A.tune_written=0;
}
static int grave_reserve(void){
 if(!A.reserve)A.reserve=calloc(1,sizeof(*A.reserve));
 return A.reserve!=NULL;
}
static void grave_add(uintptr_t ctrl){
 if(!ctrl)return;
 /* A reserve is acquired before a candidate is installed. No early eviction
  * of a previous sample when the user scrolls faster than eight files. */
 struct retired_sample*n=A.reserve;A.reserve=NULL;
 if(!n){say("RETAIN_INVARIANT","reference kept alive");return;}
 n->ctrl=ctrl;n->due=O.now_ms()+GRAVE_MS;n->next=A.grave;A.grave=n;
}
static void grave_drain(void){
 uint64_t t=O.now_ms();unsigned rank=0;struct retired_sample**link=&A.grave; /* newest first */
 while(*link){struct retired_sample*n=*link;if(rank>=GRAVE_KEEP&&(int64_t)(t-n->due)>=0){*link=n->next;O.release(n->ctrl);free(n);}else{link=&n->next;++rank;}}
}
static void drop_candidate(void){grave_add(A.cand_ctrl);A.cand_ctrl=0;memset(&A.cand,0,sizeof(A.cand));}
static void drop_all(void){A.tune_written=0;drop_candidate();free(A.reserve);A.reserve=NULL;A.preview_paused=0;A.preview_resume=0;A.active=A.swapped=0;A.program=NULL;A.inst=NULL;}
/* Put the original sample back. Returns 0 when the layer was changed by
 * someone else: then nothing is written and the audition ends. */
static int restore_locked(const char*why){
 if(!A.swapped)return 1;
 void*inst=NULL;char*l=layer_of(A.program,A.pad,&inst);
 A.swapped=0;
 if(!l||inst!=A.inst){say("STALE_NO_WRITE",why);drop_all();return 0;}
 if(FIELD(l,L_SAMPLE,uintptr_t)!=A.cand.ptr){say("CONFLICT_NO_WRITE",why);drop_all();return 0;}
 if(!move(l,&A.cand,&A.orig)){say("RESTORE_RANGE_FAILED",why);drop_all();return 0;}
 restore_tune(l);
 say("RESTORED",why);return 1;
}
static int acquire(void*audition_program,struct smp*out,uintptr_t*ctrl){
 void*inst=audition_program?O.instrument(audition_program,0x14):NULL;
 if(!inst||FIELD(inst,0x2310,void*)!=audition_program)return 0;
 uintptr_t s[2]={0,0};
 if(!O.get_sample(inst,s))return 0;
 /* Do not swap in a file whose audio data is still loading: the pad would
  * play silence until Preview was toggled. Keep polling instead. */
 if(O.ready&&!O.ready(s[0])){O.release(s[1]);if(!A.waiting_data){A.waiting_data=1;say("WAIT_DATA","file still loading");}return 0;}
 A.waiting_data=0;
 int64_t frames=O.frames(s[0]);
 if(frames<=0){O.release(s[1]);return 0;}
 char*l=NULL;{uintptr_t b=FIELD(inst,0x22e8,uintptr_t),e=FIELD(inst,0x22f0,uintptr_t);if(b&&e>=b&&e-b==8*LAYER)l=(char*)b;}
 int64_t first=0,last=frames;
 if(l){int64_t f=FIELD(l,L_RANGE+R_FIRST,int64_t),e=FIELD(l,L_RANGE+R_LAST,int64_t);if(f>=0&&e>f&&e<=frames){first=f;last=e;}}
 out->ptr=s[0];out->first=first;out->last=last;out->frames=frames;*ctrl=s[1];return 1;
}
static void resume_candidate(void){
  if(!A.cand.ptr||A.swapped)return;
  /* The private preview instrument is emptied when preview ends. The
   * audition still owns its captured sample and must resume after a save. */
  void*i2=NULL;char*l2=layer_of(A.program,A.pad,&i2);
  if(!l2||i2!=A.inst){say("STALE_NO_WRITE","resume");drop_all();return;}
  if(FIELD(l2,L_SAMPLE,uintptr_t)!=A.orig.ptr){say("CONFLICT_NO_WRITE","resume");drop_all();return;}
  if(!move(l2,&A.orig,&A.cand)){say("RESUME_RANGE_REFUSED","");return;}
  A.swapped=1;++A.swaps;apply_tune(l2);say("SWAPPED","resume");
}
static void take_candidate(void*audition_program){
 struct smp c;uintptr_t ctrl=0;
 if(!acquire(audition_program,&c,&ctrl)){resume_candidate();return;}
 if(A.cand.ptr==c.ptr&&A.cand.first==c.first&&A.cand.last==c.last){
  O.release(ctrl);resume_candidate();return;
 }
 struct retired_sample*next_reserve=NULL;
 if(!grave_reserve()){O.release(ctrl);say("RETAIN_REFUSED","memory");return;}
 if(A.cand_ctrl){next_reserve=calloc(1,sizeof(*next_reserve));if(!next_reserve){O.release(ctrl);say("RETAIN_REFUSED","memory");return;}}
 void*inst=NULL;char*l=layer_of(A.program,A.pad,&inst);
 if(!l||inst!=A.inst){free(next_reserve);O.release(ctrl);say("STALE_NO_WRITE","candidate");drop_all();return;}
 if(A.swapped){
  if(FIELD(l,L_SAMPLE,uintptr_t)!=A.cand.ptr){free(next_reserve);O.release(ctrl);say("CONFLICT_NO_WRITE","candidate");A.swapped=0;drop_all();return;}
  if(!move(l,&A.cand,&c)){free(next_reserve);O.release(ctrl);say("CANDIDATE_RANGE_REFUSED","");return;}
  grave_add(A.cand_ctrl);if(next_reserve)A.reserve=next_reserve;A.cand=c;A.cand_ctrl=ctrl;++A.swaps;A.override_set=0;apply_tune(l);say("CANDIDATE_CHANGED","");return;
 }
 if(FIELD(l,L_SAMPLE,uintptr_t)!=A.orig.ptr){free(next_reserve);O.release(ctrl);say("CONFLICT_NO_WRITE","pad sample changed");drop_all();return;}
 if(!move(l,&A.orig,&c)){free(next_reserve);O.release(ctrl);say("CANDIDATE_RANGE_REFUSED","");return;}
 grave_add(A.cand_ctrl);if(next_reserve)A.reserve=next_reserve;A.cand=c;A.cand_ctrl=ctrl;A.swapped=1;++A.swaps;apply_tune(l);say("SWAPPED","");
}
int seq_audition_toggle(void*program,unsigned pad,void*audition_program,char*msg,size_t size){
 int r;lock();
 if(A.active){restore_locked("toggle off");drop_all();say("OFF","toggle");r=0;goto out;}
 void*inst=NULL;char*l=pad<128?layer_of(program,pad,&inst):NULL;
 const char*why=NULL;
 if(!l)why="Pad not ready";
 else if(!FIELD(l,L_SAMPLE,uintptr_t))why="Pad has no sample";
 else if(O.chopped(FIELD(l,L_SAMPLE,uintptr_t)))why="Chopped pad: not supported";
 else if(O.looping(l))why="Looped pad: not supported";
 if(!why){
  A.orig.ptr=FIELD(l,L_SAMPLE,uintptr_t);A.orig.frames=O.frames(A.orig.ptr);
  A.orig.first=FIELD(l,L_RANGE+R_FIRST,int64_t);A.orig.last=FIELD(l,L_RANGE+R_LAST,int64_t);
  if(A.orig.first<0||A.orig.last<=A.orig.first||A.orig.last>A.orig.frames)why="Pad range invalid";
 }
 if(why){if(msg&&size)snprintf(msg,size,"%s",why);say("REFUSED",why);r=-1;goto out;}
 A.program=program;A.pad=pad;A.inst=inst;A.active=1;A.hold=(int)save_depth;A.swapped=0;A.swaps=0;
 A.pad_root=O.root_of?O.root_of(A.orig.ptr):-1;A.orig_tune=FIELD(l,0x60,int8_t);A.override_set=0;A.tune_written=0;A.preview_paused=0;A.preview_resume=0;
 say("ON","");if(!A.hold)take_candidate(audition_program);r=1;
out:unlock();return r;
}
void seq_audition_tick(void*current_program,void*audition_program,int browser_visible){
 lock();grave_drain();if(atomic_exchange(&A.preview_resume,0)&&A.active)A.preview_paused=0;
 if(A.active&&current_program!=A.program){A.swapped=0;say("STALE_NO_WRITE","project/program changed");drop_all();}
 else if(A.active&&!A.hold){
  if(!browser_visible){restore_locked("browser exit");drop_all();say("OFF","browser exit");}
  else if(!A.preview_paused)take_candidate(audition_program);
 }
 unlock();
}
void seq_audition_off(const char*why){
 lock();if(A.active||A.swapped){restore_locked(why?why:"off");drop_all();say("OFF",why?why:"");}unlock();
}
void seq_audition_save_begin(void){
 lock();++save_depth;if(A.active){A.hold=(int)save_depth;restore_locked("save");}unlock();
}
void seq_audition_save_end(void){
 lock();if(save_depth)--save_depth;A.hold=(int)save_depth;if(A.active&&!save_depth)say("SAVE_DONE","resume");unlock();
}
int seq_audition_knob(unsigned v){
 int r=0;
 lock();
 if(A.active){
  int idx=(int)(v>127?127:v)*13/128;
  A.override_set=1;A.override_root=idx-1;r=1;
  if(A.swapped&&!A.hold){
   void*inst=NULL;char*l=layer_of(A.program,A.pad,&inst);
   if(l&&inst==A.inst&&FIELD(l,L_SAMPLE,uintptr_t)==A.cand.ptr){apply_tune(l);}
  }
  say("ROOT","");
 }
 unlock();
 return r;
}
void seq_audition_info(char*out,size_t n){
 static const char*N[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
 if(!out||!n)return;
 lock();
 if(!A.active){out[0]=0;}
 else if(A.preview_paused){snprintf(out,n,"AUDITION PAUSED: Preview Off");}
 else if(A.hold){snprintf(out,n,"SEQ AUDITION: saving");}
 else if(!A.swapped){snprintf(out,n,"SEQ AUDITION: pick a file   K2: file note");}
 else{
  int c=pick_root(),p=A.pad_root;
  snprintf(out,n,"SEQ AUDITION  file %s  pad %s  %+d st  (K2)",c<0?"--":N[c],p<0?"--":N[p],shift_value());
 }
 unlock();
}
int seq_audition_active(void){return A.active;}
int seq_audition_state(void){return !A.active?0:A.hold?3:A.preview_paused?4:A.swapped?2:1;}
unsigned seq_audition_pad(void){return A.pad;}

void seq_audition_preview(int enabled){
 if(enabled){atomic_store_explicit(&A.preview_resume,1,memory_order_release);return;}
 lock();A.preview_resume=0;if(A.active){restore_locked("Preview Off");drop_candidate();A.preview_paused=1;}unlock();
}
int seq_audition_view(struct seq_audition_view*out){
 if(!out)return 0;
 lock();out->state=seq_audition_state();out->pad=A.pad;out->file_root=pick_root();out->pad_root=A.pad_root;out->transpose=shift_value();int on=A.active;unlock();return on;
}
