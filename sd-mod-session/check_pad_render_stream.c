#include "src/pad_render.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
struct source {unsigned reads,closed,max_read,fail,nan;uint64_t next;struct pad_render_job*cancel;};
static int read_source(void*context,uint64_t first,size_t count,float*out){
 struct source*s=context;assert(!s->closed&&first==s->next&&count>0&&count<=256);++s->reads;s->next+=count;
 if(count>s->max_read)s->max_read=(unsigned)count;
 for(size_t i=0;i<count;++i){out[i*2]=(float)(first+i)/10000;out[i*2+1]=-(float)(first+i)/10000;}
 if(s->nan)out[0]=NAN;
 if(s->cancel)pad_render_cancel(s->cancel);
 return !s->fail;
}
static void close_source(void*context){struct source*s=context;assert(!s->closed);++s->closed;}
static struct pad_render_job*create(struct source*s,struct pad_render_request*r){
 struct pad_render_stream stream={s,10000,48000,read_source,close_source};enum pad_render_status error;
 struct pad_render_job*j=pad_render_create_stream(&stream,r,NULL,&error);assert(j&&error==PAD_RENDER_WORKING&&!s->reads);return j;
}
int main(void){
 struct source s={.next=17};struct pad_render_request r={.first=17,.end=9017,.repeats=2,.tail_frames=257};
 struct pad_render_job*j=create(&s,&r);struct pad_render_progress p;
 assert(pad_render_step(j,SIZE_MAX)==PAD_RENDER_WORKING);pad_render_progress(j,&p);
 assert(p.source_done==4096&&p.source_total==9000&&!p.done&&!s.closed&&s.max_read==256);
 assert(pad_render_step(j,0)==PAD_RENDER_INVALID);
 uint64_t previous=p.source_done+p.done;
 do{
  assert(!pad_render_output(j,NULL,NULL));enum pad_render_status status=pad_render_step(j,333);pad_render_progress(j,&p);
  assert(p.source_done+p.done>=previous&&p.source_done+p.done-previous<=333);previous=p.source_done+p.done;
  if(status==PAD_RENDER_DONE)break;
  assert(status==PAD_RENDER_WORKING);
 }while(1);
 uint64_t frames;const float*out=pad_render_output(j,&frames,NULL);assert(out&&frames==18257&&s.closed==1);
 for(uint64_t i=0;i<frames;++i){float v=i<18000?(float)(17+i%9000)/10000:0;assert(out[i*2]==v&&out[i*2+1]==-v);}
 pad_render_destroy(j);assert(s.closed==1);
 puts("PASS streaming capture and render share4096-frame step budget,256-frame reads, distinct source/output progress, exact trim/repeats/tail, no incomplete output and stream release before render");
 for(unsigned failure=0;failure<6;++failure){
  s=(struct source){.next=17};j=create(&s,&r);
  if(failure==0)pad_render_cancel(j);
  if(failure==1){assert(pad_render_step(j,8)==PAD_RENDER_WORKING);pad_render_cancel(j);}
  if(failure==2)s.fail=1;
  if(failure==3)s.nan=1;
  if(failure==4)s.cancel=j;
  if(failure!=5){enum pad_render_status expected=(failure==2||failure==3)?PAD_RENDER_AUDIO_ERROR:PAD_RENDER_CANCELLED;
   assert(pad_render_step(j,4096)==expected&&!pad_render_output(j,NULL,NULL));}
  pad_render_destroy(j);assert(s.closed==1);
 }
 s=(struct source){.next=17};struct pad_render_stream stream={&s,10000,48000,read_source,close_source};enum pad_render_status error;
 r.pcm_budget=1;assert(!pad_render_create_stream(&stream,&r,NULL,&error)&&error==PAD_RENDER_LIMIT&&!s.closed&&!s.reads);
 r.pcm_budget=0;r.chain.count=1;r.chain.stage[0].kind=FX_STAGE_NATIVE;
 assert(!pad_render_create_stream(&stream,&r,NULL,&error)&&error==PAD_RENDER_FX_ERROR&&!s.closed&&!s.reads);
 stream.close(stream.user);assert(s.closed==1);
 puts("PASS cancellation before/during capture including inside reader, destroy mid-capture, short/error/NaN input release exactly once; failed allocation/backend preparation retains caller stream ownership");
}
