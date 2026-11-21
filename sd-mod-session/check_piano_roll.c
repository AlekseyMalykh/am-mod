#include "src/piano_roll.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static struct pr_note sequence[64],saved[64],future[64];
static unsigned count,saved_count,future_count,commits;static int refused;
static uint64_t revision=1;
static int64_t length=3840;
static int encode(void*u,const struct pr_note*n,struct step_event*e){
 (void)u;if(n->sound==127&&n->slice)return 0;
 *e=n->event;e->data[32]=(unsigned char)n->sound;
 memcpy(e->data+4,&n->tune,4);memcpy(e->data+8,&n->slice,4);return 1;
}
static void decode(struct pr_note*n,const struct step_event*e){memset(n,0,sizeof(*n));n->event=*e;n->sound=e->data[32];memcpy(&n->tune,e->data+4,4);memcpy(&n->slice,e->data+8,4);}
static int commit(void*u,const struct pr_plan*p){
 (void)u;if(refused||revision!=p->revision)return 0;
 memcpy(saved,sequence,sizeof(saved));saved_count=count;future_count=0;
 for(unsigned i=0;i<p->removed_count;++i){unsigned j;for(j=0;j<count;++j)if(!memcmp(&sequence[j].event,p->removed+i,sizeof(struct step_event)))break;assert(j<count);memmove(sequence+j,sequence+j+1,(count-j-1)*sizeof(*sequence));--count;}
 for(unsigned i=0;i<p->added_count;++i){assert(count<64);decode(sequence+count++,p->added+i);}
 ++revision;++commits;return 1;
}
static void reload(struct piano_roll*p){unsigned char loaded[128]={0};loaded[0]=loaded[127]=1;assert(pr_load(p,sequence,count,length,revision,loaded));}
static void undo(struct piano_roll*p){memcpy(future,sequence,sizeof(future));future_count=count;memcpy(sequence,saved,sizeof(sequence));count=saved_count;++revision;reload(p);}
static void redo(struct piano_roll*p){memcpy(sequence,future,sizeof(sequence));count=future_count;++revision;reload(p);}
static struct pr_note note(int pad,int64_t tick,int64_t duration,int tune,int slice){
 struct pr_note n={0};n.sound=pad;n.tune=tune;n.slice=slice;n.event.type=3;n.event.time=tick;
 float velocity=.5f;memcpy(n.event.data,&velocity,4);memcpy(n.event.data+24,&duration,8);
 n.event.data[22]=0x5a;n.event.data[34]=37;n.event.data[36]=3;encode(NULL,&n,&n.event);return n;
}
static int64_t dur(const struct step_event*e){int64_t d;memcpy(&d,e->data+24,8);return d;}
int main(void){
 struct piano_roll p;assert(pr_init(&p,(struct pr_ops){NULL,encode,commit}));
 sequence[0]=note(0,119,50,0,0);sequence[1]=note(1,360,300,7,3);sequence[2]=sequence[1];count=3;reload(&p);
 assert(p.row_count==3&&p.rows[0]==0&&p.rows[1]==1&&p.rows[2]==127); /* Ascending numeric rows, rendered bottom-up. */
 p.left=960;p.span=960;p.top=2;p.rows_visible=1;reload(&p);
 assert(p.left==960&&p.top==2&&p.cursor_tick==0&&p.cursor_row==0);
 p.left=p.top=0;p.span=3840;p.rows_visible=6;
 assert(pr_browse(&p,1)&&p.focused==0&&p.cursor_tick==119);pr_select(&p,0);
 assert(pr_browse(&p,1)&&p.focused==1);pr_select(&p,1);assert(pr_selected(&p)==2);
 assert(pr_browse(&p,1)&&p.focused==2);pr_select(&p,1);assert(pr_selected(&p)==3);
 assert(pr_browse(&p,-1)&&p.focused==1);pr_select(&p,1);assert(pr_selected(&p)==2&&!p.notes[1].selected);
 assert(pr_browse(&p,2)&&p.focused==0); /* Both directions wrap through exact duplicates. */
 pr_cursor(&p,360,1);pr_select(&p,0);assert(p.notes[1].selected&&!p.notes[2].selected);
 pr_select(&p,0);assert(!p.notes[1].selected&&p.notes[2].selected);pr_select(&p,1);assert(pr_selected(&p)==2);
 reload(&p);assert(pr_selected(&p)==2); /* Exact duplicates survive snapshot selection reconciliation. */
 pr_select_all(&p);assert(pr_selected(&p)==3);
 struct step_event first=sequence[0].event;unsigned before=commits;
 assert(pr_edit(&p,-9999,0,-9999,0));assert(!commits&&!memcmp(&first,&sequence[0].event,sizeof(first)));
 assert(p.plan.removed_count==3&&p.plan.added[0].time==0&&p.plan.added[1].time==241);
 assert(dur(p.plan.added)==1&&dur(p.plan.added+1)==251);
 assert(pr_apply(&p)&&commits==before+1);reload(&p);assert(pr_selected(&p)==3);
 assert(sequence[1].event.data[34]==37&&sequence[1].event.data[36]==3&&sequence[1].event.data[22]==0x5a);
 undo(&p);assert(count==3&&!memcmp(&sequence[0].event,&first,sizeof(first)));redo(&p);assert(sequence[0].event.time==0&&dur(&sequence[0].event)==1);
 undo(&p);pr_select_all(&p);assert(pr_quantize(&p,PR_SNAP_START));reload(&p);
 assert(sequence[0].event.time==0&&dur(&sequence[0].event)==50);
 assert(sequence[1].event.time==480&&sequence[2].event.time==480&&count==3);undo(&p);redo(&p);
 pr_select_all(&p);assert(pr_copy(&p));pr_cursor(&p,960,0);assert(pr_paste(&p));reload(&p);assert(count==6&&pr_selected(&p)==3);
 unsigned clipboard_count=p.clip_count;struct pr_note clipboard_first=p.clipboard[0];
 pr_reset_view(&p);reload(&p);assert(p.clip_count==clipboard_count&&!memcmp(p.clipboard,&clipboard_first,sizeof(clipboard_first)));
 assert(sequence[4].event.time-sequence[3].event.time==480);undo(&p);assert(count==3);redo(&p);assert(count==6);
 pr_select_all(&p);assert(pr_delete(&p,1));reload(&p);assert(!count&&p.clip_count==6);undo(&p);assert(count==6);redo(&p);assert(!count);
 undo(&p);pr_select_all(&p);before=commits;assert(!pr_edit(&p,0,127,0,0)&&!p.plan.title&&commits==before);
 pr_view_layer(&p,PR_TUNE,1);pr_select_all(&p);assert(pr_selected(&p)==4);
 assert(pr_edit(&p,0,2,0,0)&&pr_apply(&p));reload(&p);
 for(unsigned i=0;i<count;++i)if(sequence[i].sound==1)assert(sequence[i].tune==9&&sequence[i].slice==3);
 pr_view_layer(&p,PR_CHOP,1);pr_select_all(&p);assert(pr_edit(&p,0,1,0,0)&&pr_apply(&p));reload(&p);
 for(unsigned i=0;i<count;++i)if(sequence[i].sound==1)assert(sequence[i].tune==9&&sequence[i].slice==4);
 pr_select_all(&p);refused=1;before=commits;assert(pr_edit(&p,40,0,0,0)&&!pr_apply(&p)&&commits==before);refused=0;
 assert(pr_copy(&p));unsigned previous_clip=p.clip_count;struct pr_note previous_note=p.clipboard[0];
 refused=1;assert(!pr_delete(&p,1)&&p.clip_count==previous_clip&&!memcmp(&previous_note,p.clipboard,sizeof(previous_note)));refused=0;
 /* An unrelated native edit invalidates a preview before it can be applied. */
 assert(pr_edit(&p,40,0,0,0));++revision;reload(&p);assert(!p.plan.title&&!pr_apply(&p));
 pr_view_layer(&p,PR_PADS,0);pr_cursor(&p,0,0);pr_range(&p);pr_cursor(&p,1000,1);pr_range(&p);assert(pr_selected(&p)>0);
 int64_t exact=sequence[0].event.time;for(int z=0;z<10;++z){pr_zoom(&p,1,z);pr_zoom(&p,0,12+z);assert(sequence[0].event.time==exact);}
 assert(!pr_load(&p,sequence,PR_MAX_EVENTS+1,length,revision,NULL));
 pr_reset(&p);sequence[0]=note(0,0,0,0,0);sequence[1]=note(0,120,10000,0,0);count=2;++revision;reload(&p);
 pr_select_all(&p);assert(pr_edit(&p,120,0,0,0));assert(dur(p.plan.added)==0&&dur(p.plan.added+1)==10000);
 pr_cancel(&p);pr_view_layer(&p,PR_TUNE,0);
 assert(p.row_count==22&&p.rows[0]==117&&p.rows[p.row_count-1]==138);
 sequence[1].tune=60;++revision;reload(&p);
 assert(p.rows[0]==117&&p.rows[p.row_count-1]==183); /* Recorded outliers plus 3, never hidden. */
 pr_layout_range(&p,110,125);assert(p.rows[0]==107&&p.rows[p.row_count-1]==183);
 for(int z=0;z<10;++z){pr_zoom(&p,1,z);assert(p.span<=p.length&&p.left+p.span<=p.length);}
 assert(p.span==p.length&&p.left==0);
 pr_zoom(&p,1,p.zoom-1);assert(p.span<p.length); /* One encoder notch leaves Fit immediately. */
 pr_zoom(&p,1,0);p.follow=1;int64_t cursor=p.cursor_tick;
 pr_follow(&p,3000,1);assert(p.left<=3000&&p.left+p.span>3000&&p.cursor_tick==cursor);
 pr_follow(&p,0,1);assert(p.left==0);p.follow=0;pr_follow(&p,3000,1);assert(p.left==0);
 pr_reset(&p);count=1;sequence[0]=note(0,0,240,0,0);++revision;reload(&p);
 pr_select_all(&p);pr_zoom(&p,1,0);p.rows_visible=3;
 assert(pr_edit(&p,2400,80,0,0));int row=pr_row_index(&p,80);
 assert(row>=p.top&&row<p.top+p.rows_visible&&p.left<=2400&&p.left+p.span>=2640);
 assert(p.plan.added_count==1&&p.plan.added[0].data[32]==80&&!sequence[0].event.time&&sequence[0].sound==0);
 pr_zoom(&p,0,38);row=pr_row_index(&p,80);assert(row>=p.top&&row<p.top+p.rows_visible);
 pr_cancel(&p);pr_rows(&p);assert(pr_row_index(&p,80)<0);
 count=1;sequence[0]=note(0,0,0,0,0);++revision;reload(&p);pr_select_all(&p);pr_zoom(&p,1,0);
 assert(pr_edit(&p,p.span,0,0,0)&&p.preview_start<p.left+p.span); /* Zero-length note stays visible. */
 pr_cancel(&p);

 pr_view_layer(&p,PR_TUNE,0);pr_select_all(&p);assert(pr_edit(&p,1200,100,0,0));row=pr_row_index(&p,220);
 assert(row>=p.top&&row<p.top+p.rows_visible&&p.left<=1200&&p.left+p.span>=1201);
 pr_destroy(&p);puts("PASS Piano Roll: duplicate/polyphonic selection, bottom-up rows, move/length bounds, preview/commit, native-history adapter Undo+Redo, cut/copy/paste, selected snap, Tune/Chop independence, stale revision and zoom without mutation");return 0;
}
