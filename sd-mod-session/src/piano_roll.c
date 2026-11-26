#include "piano_roll.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int fail(struct piano_roll*p,const char*s){pr_cancel(p);snprintf(p->error,sizeof(p->error),"%s",s);return 0;}
static int bounded(int v,int lo,int hi){return v<lo?lo:v>hi?hi:v;}
static int64_t bounded64(int64_t v,int64_t lo,int64_t hi){return v<lo?lo:v>hi?hi:v;}
static int64_t duration(const struct step_event*e){int64_t v;memcpy(&v,e->data+24,8);return v;}
static void set_duration(struct step_event*e,int64_t v){memcpy(e->data+24,&v,8);}
static int equal(const struct step_event*a,const struct step_event*b){
 return a->time==b->time&&((a->type^b->type)&~0x100u)==0&&!memcmp(a->data,b->data,37);
}
static uint64_t hash(const struct step_event*e){
 uint64_t h=UINT64_C(1469598103934665603);const unsigned char*t=(const void*)&e->time;
 for(unsigned i=0;i<8;++i)h=(h^t[i])*UINT64_C(1099511628211);
 h=(h^(e->type&~0x100u))*UINT64_C(1099511628211);
 for(unsigned i=0;i<37;++i)h=(h^e->data[i])*UINT64_C(1099511628211);
 return h;
}
int pr_init(struct piano_roll*p,struct pr_ops ops){
 if(!p)return 0;
 memset(p,0,sizeof(*p));p->ops=ops;p->capacity=PR_MAX_EVENTS;
 p->notes=calloc(PR_MAX_EVENTS,sizeof(*p->notes));p->clipboard=calloc(PR_MAX_EVENTS,sizeof(*p->clipboard));
 p->plan.removed=calloc(PR_MAX_EVENTS,sizeof(struct step_event));p->plan.added=calloc(PR_MAX_EVENTS,sizeof(struct step_event));
 if(!p->notes||!p->clipboard||!p->plan.removed||!p->plan.added){pr_destroy(p);return 0;}
 p->plan.capacity=PR_MAX_EVENTS;pr_reset(p);return 1;
}
void pr_destroy(struct piano_roll*p){if(p){free(p->notes);free(p->clipboard);free(p->plan.removed);free(p->plan.added);memset(p,0,sizeof(*p));}}
void pr_cancel(struct piano_roll*p){p->preview_valid=0;memset(p->preview_rows,0,sizeof(p->preview_rows));p->plan.removed_count=p->plan.added_count=0;p->plan.title=NULL;p->error[0]=0;}
void pr_reset(struct piano_roll*p){
 p->count=p->clip_count=p->pending_commit=p->selection_after_count=0;p->revision=0;p->length=1;p->cursor_tick=p->left=0;p->span=3840;
 p->cursor_row=p->top=p->layer=p->sound=p->range=0;p->rows_visible=6;p->row_height=19;p->zoom=2;p->grid=2;p->chop_count=128;
 p->layout_low=120;p->layout_high=135;p->follow=p->note_focus=0;p->focused=-1;
 memset(p->loaded,0,sizeof(p->loaded));memset(p->pinned,0,sizeof(p->pinned));pr_cancel(p);pr_rows(p);
}
int pr_note_row(const struct piano_roll*p,const struct pr_note*n){
 if(p->layer==PR_PADS)return n->sound;
 if(n->sound!=p->sound)return -1;
 if(p->layer==PR_CHOP)return n->slice>0&&n->slice<=p->chop_count?n->slice-1:-1;
 return n->tune+120;
}
void pr_reset_view(struct piano_roll*p){
 unsigned count=p->clip_count;int64_t tick=p->clip_tick;int row=p->clip_row,layer=p->clip_layer;
 pr_reset(p);p->clip_count=count;p->clip_tick=tick;p->clip_row=row;p->clip_layer=layer;
}
int pr_row_index(const struct piano_roll*p,int value){for(int i=0;i<p->row_count;++i)if(p->rows[i]==value)return i;return -1;}
void pr_rows(struct piano_roll*p){
 p->row_count=0;
 if(p->layer==PR_PADS){
  unsigned char used[128];memcpy(used,p->loaded,128);for(unsigned i=0;i<128;++i)used[i]|=p->pinned[i];
  for(unsigned i=0;i<p->count;++i)used[p->notes[i].sound]=1;
  for(unsigned i=0;i<128;++i)used[i]|=p->preview_rows[i];
  for(int i=0;i<128;++i)if(used[i])p->rows[p->row_count++]=i;
 }else{
  int max=p->layer==PR_TUNE?240:bounded(p->chop_count,1,128)-1;
  int lo=bounded(p->layout_low,0,max),hi=bounded(p->layout_high,lo,max);
  for(unsigned i=0;i<p->count;++i){int row=pr_note_row(p,p->notes+i);if(row<0||row>max)continue;if(row<lo)lo=row;if(row>hi)hi=row;}
  for(int row=0;row<=max;++row)if(p->preview_rows[row]){if(row<lo)lo=row;if(row>hi)hi=row;}
  lo=bounded(lo-3,0,max);hi=bounded(hi+3,lo,max);
  for(int i=lo;i<=hi;++i)p->rows[p->row_count++]=i;
 }
 if(!p->row_count)p->rows[p->row_count++]=bounded(p->sound,0,127);
 p->cursor_row=bounded(p->cursor_row,0,p->row_count-1);p->top=bounded(p->top,0,p->row_count-1);
}
int pr_load(struct piano_roll*p,const struct pr_note*notes,unsigned count,int64_t length,uint64_t revision,const unsigned char loaded[128]){
 if(!p||count>p->capacity||length<=0||length>INT64_C(960)*128*64)return p?fail(p,"Sequence exceeds editor capacity"):0;
 for(unsigned i=0;i<count;++i){
  const struct pr_note*n=notes+i;
  float velocity;memcpy(&velocity,n->event.data,4);
  if(n->sound<0||n->sound>127||n->tune< -120||n->tune>120||n->slice<0||n->slice>128||
     !isfinite(velocity)||velocity<0.f||velocity>1.01f||n->event.data[32]>127||
     (n->event.type&255)!=3||n->event.time<0||n->event.time>=length||duration(&n->event)<0||duration(&n->event)>INT64_C(960)*128*64)
   return fail(p,"Unsupported event: sequence unchanged");
 }
 /* Preserve selections as a multiset, including exact duplicate events. */
 int committed=p->pending_commit&&revision!=p->revision;
 unsigned previous=committed?p->selection_after_count:p->count;
 int *heads=malloc(16384*sizeof(int)),*next=malloc((previous?previous:1)*sizeof(int));
 if(!heads||!next){free(heads);free(next);return fail(p,"Selection allocation failed");}
 for(unsigned i=0;i<16384;++i)heads[i]=-1;
 for(unsigned i=0;i<previous;++i){const struct step_event*e=committed?p->plan.added+i:&p->notes[i].event;unsigned h=(unsigned)hash(e)&16383;next[i]=heads[h];heads[h]=(int)i;}
 unsigned char selected[PR_MAX_EVENTS];
 for(unsigned i=0;i<count;++i){unsigned h=(unsigned)hash(&notes[i].event)&16383;int *at=heads+h;selected[i]=0;
  while(*at>=0){int index=*at;const struct step_event*e=committed?p->plan.added+index:&p->notes[index].event;if(equal(e,&notes[i].event)){selected[i]=committed?1:(unsigned char)p->notes[index].selected;*at=next[index];break;}at=next+index;}
 }
 free(heads);free(next);
 int changed=p->revision!=revision||p->length!=length;
 int cursor_value=p->rows[p->cursor_row];
 memcpy(p->notes,notes,count*sizeof(*notes));for(unsigned i=0;i<count;++i)p->notes[i].selected=selected[i]&&pr_note_row(p,p->notes+i)>=0;
 p->count=count;p->length=length;p->revision=revision;if(loaded)memcpy(p->loaded,loaded,128);
 if(p->span>length)p->span=length;
 if(changed){pr_cancel(p);p->pending_commit=p->selection_after_count=0;}
 pr_rows(p);int row=pr_row_index(p,cursor_value);
 if(changed)pr_cursor(p,p->cursor_tick,row>=0?row:p->cursor_row);
 else { /* Polling the native list must not undo manual viewport scrolling. */
  p->cursor_tick=bounded64(p->cursor_tick,0,p->length-1);
  p->cursor_row=bounded(row>=0?row:p->cursor_row,0,p->row_count-1);
  p->left=bounded64(p->left,0,p->length>p->span?p->length-p->span:0);
  p->top=bounded(p->top,0,p->row_count>p->rows_visible?p->row_count-p->rows_visible:0);
 }return 1;
}
void pr_view_layer(struct piano_roll*p,int layer,int sound){
 if(layer<0||layer>PR_CHOP||sound<0||sound>127)return;
 p->layer=layer;p->sound=sound;p->range=0;pr_cancel(p);
 p->layout_low=layer==PR_TUNE?120:0;p->layout_high=p->layout_low+15;pr_rows(p);
 for(unsigned i=0;i<p->count;++i)if(pr_note_row(p,p->notes+i)<0)p->notes[i].selected=0;
 int r=pr_row_index(p,layer==PR_PADS?sound:layer==PR_TUNE?120:0);
 p->top=bounded(r-p->rows_visible/2,0,p->row_count-1);pr_cursor(p,p->cursor_tick,r);
}
void pr_cursor(struct piano_roll*p,int64_t tick,int row){
 p->focused=-1;
 p->cursor_tick=bounded64(tick,0,p->length-1);p->cursor_row=bounded(row,0,p->row_count-1);
 if(p->cursor_tick<p->left)p->left=p->cursor_tick;
 if(p->cursor_tick>=p->left+p->span)p->left=p->cursor_tick-p->span+1;
 if(p->cursor_row<p->top)p->top=p->cursor_row;
 if(p->cursor_row>=p->top+p->rows_visible)p->top=p->cursor_row-p->rows_visible+1;
 p->left=bounded64(p->left,0,p->length>p->span?p->length-p->span:0);
 p->top=bounded(p->top,0,p->row_count>p->rows_visible?p->row_count-p->rows_visible:0);
}
unsigned pr_selected(const struct piano_roll*p){unsigned n=0;for(unsigned i=0;i<p->count;++i)n+=!!p->notes[i].selected;return n;}
void pr_select_all(struct piano_roll*p){for(unsigned i=0;i<p->count;++i)p->notes[i].selected=pr_note_row(p,p->notes+i)>=0;p->range=0;pr_cancel(p);}
void pr_select(struct piano_roll*p,int additive){
 int hit=-1;
 for(unsigned i=0;i<p->count;++i){struct pr_note*n=p->notes+i;
  if(pr_note_row(p,n)==p->rows[p->cursor_row]&&p->cursor_tick>=n->event.time&&p->cursor_tick<n->event.time+(duration(&n->event)>0?duration(&n->event):1)){
   if(hit<0)hit=(int)i;
   if(n->selected&&!additive){for(unsigned j=i+1;j<p->count;++j)if(pr_note_row(p,p->notes+j)==pr_note_row(p,n)&&
    p->cursor_tick>=p->notes[j].event.time&&p->cursor_tick<p->notes[j].event.time+(duration(&p->notes[j].event)>0?duration(&p->notes[j].event):1)){hit=(int)j;break;}break;}
  }
 }
 if(p->focused>=0&&(unsigned)p->focused<p->count)hit=p->focused;
 if(!additive)for(unsigned i=0;i<p->count;++i)p->notes[i].selected=0;
 if(hit>=0)p->notes[hit].selected=additive?!p->notes[hit].selected:1;
 p->range=0;pr_cancel(p);
}
/* Live area selection (hold DATA + encoder / fader): select every event inside anchor..cursor, keep the rectangle open. */
void pr_range_apply(struct piano_roll*p){
 int64_t a=p->anchor_tick,b=p->cursor_tick;if(a>b){int64_t t=a;a=b;b=t;}
 int lo=p->anchor_row,hi=p->cursor_row;if(lo>hi){int t=lo;lo=hi;hi=t;}
 for(unsigned i=0;i<p->count;++i){int row=pr_row_index(p,pr_note_row(p,p->notes+i));
  p->notes[i].selected=row>=lo&&row<=hi&&p->notes[i].event.time<=b&&p->notes[i].event.time+(duration(&p->notes[i].event)>0?duration(&p->notes[i].event):1)>a;
 }pr_cancel(p);
}
void pr_range(struct piano_roll*p){
 if(!p->range){p->anchor_tick=p->cursor_tick;p->anchor_row=p->cursor_row;p->range=1;return;}
 int64_t a=p->anchor_tick,b=p->cursor_tick;if(a>b){int64_t t=a;a=b;b=t;}
 int lo=p->anchor_row,hi=p->cursor_row;if(lo>hi){int t=lo;lo=hi;hi=t;}
 for(unsigned i=0;i<p->count;++i){int row=pr_row_index(p,pr_note_row(p,p->notes+i));
  p->notes[i].selected=row>=lo&&row<=hi&&p->notes[i].event.time<=b&&p->notes[i].event.time+(duration(&p->notes[i].event)>0?duration(&p->notes[i].event):1)>a;
 }p->range=0;pr_cancel(p);
}
int64_t pr_grid_ticks(int grid){static const int values[10]={960,480,240,120,60,640,320,160,80,40};return grid==10?1:values[bounded(grid,0,9)];}
static int plan_begin(struct piano_roll*p,const char*title){if(p->pending_commit)return fail(p,"Waiting for native history");pr_cancel(p);if(!pr_selected(p))return fail(p,"Select events first");p->plan.title=title;p->plan.revision=p->revision;return 1;}
int pr_copy(struct piano_roll*p){
 if(!pr_selected(p))return fail(p,"Select events first");
 unsigned count=0;int64_t start=INT64_MAX;int row=INT_MAX;
 for(unsigned i=0;i<p->count;++i)if(p->notes[i].selected){struct pr_note*n=p->notes+i;
  p->clipboard[count++]=*n;if(n->event.time<start)start=n->event.time;int r=pr_note_row(p,n);if(r<row)row=r;
 }p->clip_count=count;p->clip_tick=start;p->clip_row=row;p->clip_layer=p->layer;p->error[0]=0;return 1;
}
int pr_delete(struct piano_roll*p,int cut){
 if(!plan_begin(p,cut?"Piano Roll Cut":"Piano Roll Delete"))return 0;
 for(unsigned i=0;i<p->count;++i)if(p->notes[i].selected)p->plan.removed[p->plan.removed_count++]=p->notes[i].event;
 if(!pr_apply(p))return 0;
 return cut?pr_copy(p):1;
}
int pr_apply(struct piano_roll*p){
 if(p->pending_commit)return fail(p,"Waiting for native history");
 if(!p->plan.title||(!p->plan.removed_count&&!p->plan.added_count))return fail(p,"No event changes");
 if(p->plan.revision!=p->revision)return fail(p,"Events changed: retry selection");
 p->error[0]=0;
 if(!p->ops.commit||!p->ops.commit(p->ops.user,&p->plan)){char m[96];snprintf(m,sizeof m,"%s",p->error[0]?p->error:"Native edit refused; refresh and retry");return fail(p,m);}
 /* Native snapshot will be read back before another operation. */
 p->pending_commit=1;p->selection_after_count=p->plan.added_count;pr_cancel(p);return 1;
}
int pr_paste(struct piano_roll*p){
 if(p->pending_commit)return fail(p,"Waiting for native history");
 if(!p->clip_count)return fail(p,"Clipboard empty");
 if(p->clip_layer!=p->layer)return fail(p,"Paste in the copied layer");
 if(p->count+p->clip_count>p->capacity)return fail(p,"Too many events for editor");
 pr_cancel(p);p->plan.title="Piano Roll Paste";p->plan.revision=p->revision;
 for(unsigned i=0;i<p->clip_count;++i){struct pr_note n=p->clipboard[i];
  int64_t t=p->cursor_tick+(n.event.time-p->clip_tick);
  if(t<0||t>=p->length)return fail(p,"Paste exceeds sequence length");
  int row=p->rows[p->cursor_row]+pr_note_row(p,&n)-p->clip_row;
  if(p->layer==PR_PADS)n.sound=row;else {n.sound=p->sound;if(p->layer==PR_TUNE)n.tune=row-120;else n.slice=row+1;}
  if(n.sound<0||n.sound>127||n.tune< -120||n.tune>120||n.slice<0||n.slice>128||(p->layer==PR_CHOP&&n.slice>p->chop_count))return fail(p,"Paste exceeds row range");
  n.event.time=t;
  if(!p->ops.encode||!p->ops.encode(p->ops.user,&n,p->plan.added+i))return fail(p,"Destination incompatible with Chop");
  ++p->plan.added_count;
 }return pr_apply(p);
}
void pr_reveal_edit(struct piano_roll*p){
 if(!p->preview_valid)return;
 int64_t start=p->preview_start,end=p->preview_end;
 if(end-start>p->span){start=p->preview_anchor_tick;end=start+1;}
 if(start<p->left)p->left=start;
 if(end>p->left+p->span)p->left=end-p->span;
 p->left=bounded64(p->left,0,p->length>p->span?p->length-p->span:0);
 int lo=pr_row_index(p,p->preview_low),hi=pr_row_index(p,p->preview_high);
 if(lo<0||hi<0)return;
 if(hi-lo>=p->rows_visible)lo=hi=pr_row_index(p,p->preview_anchor_row);
 if(lo<p->top)p->top=lo;
 if(hi>=p->top+p->rows_visible)p->top=hi-p->rows_visible+1;
 p->top=bounded(p->top,0,p->row_count>p->rows_visible?p->row_count-p->rows_visible:0);
}
int pr_edit(struct piano_roll*p,int64_t time_delta,int row_delta,int64_t length_delta,int velocity_delta){
 if(!plan_begin(p,"Piano Roll Edit"))return 0;
 int64_t lo=INT64_MIN,hi=INT64_MAX,lenlo=INT64_MIN,lenhi=INT64_MAX;int rowlo=-1000,rowhi=1000,vlo=-127,vhi=127;
 for(unsigned i=0;i<p->count;++i)if(p->notes[i].selected){struct pr_note*n=p->notes+i;int64_t d=duration(&n->event);
  if(-n->event.time>lo)lo=-n->event.time;
  if(p->length-1-n->event.time<hi)hi=p->length-1-n->event.time;
  if(1-d>lenlo)lenlo=1-d;
  if(INT64_C(960)*128*64-d<lenhi)lenhi=INT64_C(960)*128*64-d;
  int row=pr_note_row(p,n),max=p->layer==PR_TUNE?240:p->layer==PR_CHOP?p->chop_count-1:127;
  if(row<0)return fail(p,"Selection includes a different layer");
  if(-row>rowlo)rowlo=-row;
  if(max-row<rowhi)rowhi=max-row;
  float v;memcpy(&v,n->event.data,4);int vel=(int)lroundf(v*127.f);
  if(1-vel>vlo)vlo=1-vel;
  if(127-vel<vhi)vhi=127-vel;
 }
 time_delta=bounded64(time_delta,lo,hi);if(length_delta)length_delta=bounded64(length_delta,lenlo,lenhi);
 row_delta=bounded(row_delta,rowlo,rowhi);if(velocity_delta)velocity_delta=bounded(velocity_delta,vlo,vhi);
 for(unsigned i=0;i<p->count;++i)if(p->notes[i].selected){struct pr_note n=p->notes[i];struct step_event result;
  n.event.time+=time_delta;set_duration(&n.event,duration(&n.event)+length_delta);
  if(p->layer==PR_PADS)n.sound+=row_delta;else if(p->layer==PR_TUNE)n.tune+=row_delta;else n.slice+=row_delta;
  if(velocity_delta){float v;memcpy(&v,n.event.data,4);v=bounded((int)lroundf(v*127.f)+velocity_delta,1,127)/127.f;memcpy(n.event.data,&v,4);}
  if(!p->ops.encode||!p->ops.encode(p->ops.user,&n,&result))return fail(p,"Destination incompatible with Chop");
  int value=pr_note_row(p,&n);int64_t event_length=duration(&n.event);int64_t end=bounded64(n.event.time+(event_length>0?event_length:1),0,p->length);
  if(!p->preview_valid){p->preview_low=p->preview_high=p->preview_anchor_row=value;p->preview_start=p->preview_anchor_tick=n.event.time;p->preview_end=end;p->preview_valid=1;}
  if(value<p->preview_low)p->preview_low=value;
  if(value>p->preview_high)p->preview_high=value;
  if(n.event.time<p->preview_start)p->preview_start=n.event.time;
  if(end>p->preview_end)p->preview_end=end;
  if(value>=0&&value<241)p->preview_rows[value]=1;
  if(!equal(&result,&p->notes[i].event)){
   p->plan.removed[p->plan.removed_count++]=p->notes[i].event;p->plan.added[p->plan.added_count++]=result;
  }
 }
 int cursor_value=p->rows[p->cursor_row],top_value=p->rows[p->top];
 pr_rows(p);int cursor=pr_row_index(p,cursor_value),top=pr_row_index(p,top_value);
 if(cursor>=0)p->cursor_row=cursor;
 if(top>=0)p->top=top;
 p->follow=0;pr_reveal_edit(p);return 1;
}
static int64_t snap(int64_t v,int64_t grid){return ((v+grid/2)/grid)*grid;}
int pr_quantize(struct piano_roll*p,enum pr_snap mode){
 if(mode<PR_SNAP_START||mode>PR_SNAP_BOTH)return fail(p,"Unknown snap mode");
 if(!plan_begin(p,"Piano Roll Quantize"))return 0;
 int64_t grid=pr_grid_ticks(p->grid);
 for(unsigned i=0;i<p->count;++i)if(p->notes[i].selected){struct step_event e=p->notes[i].event;int64_t oldend=e.time+duration(&e);
  if(mode!=PR_SNAP_END)e.time=bounded64(snap(e.time,grid),0,((p->length-1)/grid)*grid);
  if(mode!=PR_SNAP_START){int64_t end=snap(oldend,grid);if(end<=e.time)end=e.time+grid;end=bounded64(end,e.time+1,e.time+INT64_C(960)*128*64);set_duration(&e,end-e.time);}
  if(!equal(&e,&p->notes[i].event)){p->plan.removed[p->plan.removed_count++]=p->notes[i].event;p->plan.added[p->plan.added_count++]=e;}
 }if(!p->plan.removed_count){pr_cancel(p);return 1;}return pr_apply(p);
}
void pr_zoom(struct piano_roll*p,int horizontal,int value){
 if(horizontal){static const int64_t spans[]={960,1920,3840,7680,15360,30720,61440,122880,245760,491520};
  int max_zoom=0;while(max_zoom<9&&spans[max_zoom]<p->length)++max_zoom;
  int64_t anchor=p->cursor_tick-p->left,old=p->span;p->zoom=bounded(value,0,max_zoom);p->span=p->zoom==9?p->length:spans[p->zoom];
  if(p->span>p->length)p->span=p->length;
  p->left=bounded64(p->cursor_tick-anchor*p->span/old,0,p->length-p->span);
 }else{p->row_height=bounded(value,12,38);p->rows_visible=120/p->row_height;if(p->rows_visible<1)p->rows_visible=1;}
 pr_cursor(p,p->cursor_tick,p->cursor_row);
 pr_reveal_edit(p);
}
void pr_layout_range(struct piano_roll*p,int low,int high){
 int cursor=p->rows[p->cursor_row],anchor=p->rows[bounded(p->anchor_row,0,p->row_count-1)],top=p->rows[p->top];
 p->layout_low=low;p->layout_high=high;pr_rows(p);
 int r=pr_row_index(p,cursor);if(r>=0)p->cursor_row=r;
 r=pr_row_index(p,anchor);if(r>=0)p->anchor_row=r;
 r=pr_row_index(p,top);if(r>=0)p->top=r;
 p->top=bounded(p->top,0,p->row_count>p->rows_visible?p->row_count-p->rows_visible:0);
}
void pr_follow(struct piano_roll*p,int64_t tick,int playing){
 if(!p->follow||!playing||tick<0||tick>=p->length)return;
 if(tick<p->left||tick>=p->left+p->span)p->left=bounded64((tick/p->span)*p->span,0,p->length-p->span);
}
static int note_order(const struct piano_roll*p,int a,int b){
 if(p->notes[a].event.time!=p->notes[b].event.time)return p->notes[a].event.time<p->notes[b].event.time?-1:1;
 int ar=pr_note_row(p,p->notes+a),br=pr_note_row(p,p->notes+b);
 return ar!=br?(ar<br?-1:1):a==b?0:a<b?-1:1;
}
int pr_browse(struct piano_roll*p,int delta){
 if(!delta)return 0;
 delta=bounded(delta,-127,127);int direction=delta>0?1:-1,current=p->focused;
 for(int step=0;step<(delta>0?delta:-delta);++step){int best=-1,wrap=-1;
  for(unsigned i=0;i<p->count;++i){int row=pr_note_row(p,p->notes+i);if(row<0)continue;
   if(wrap<0||note_order(p,(int)i,wrap)*direction<0)wrap=(int)i;
   int after=current>=0&&current<(int)p->count?note_order(p,(int)i,current)*direction>0:
    p->notes[i].event.time==p->cursor_tick?(row-p->rows[p->cursor_row])*direction>=0:
    direction>0?p->notes[i].event.time>p->cursor_tick:p->notes[i].event.time<p->cursor_tick;
   if(after&&(best<0||note_order(p,(int)i,best)*direction<0))best=(int)i;
  }
  current=best>=0?best:wrap;if(current<0)return 0;
 }
 pr_cursor(p,p->notes[current].event.time,pr_row_index(p,pr_note_row(p,p->notes+current)));p->focused=current;return 1;
}
