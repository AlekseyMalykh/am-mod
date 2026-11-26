/* SongScreen UI owner only. Producer sees arrangement_active, never these
 * values or native pointers. Timeline units are quarter-note beats (960). */
#include "ui_list_geometry.h"
static uint64_t arrange_epoch,arrange_selected;
static struct pt_arr_clip arrange_clipboard;
static int arrange_row,arrange_top,arrange_rows=4,arrange_menu,arrange_pick;
static int64_t arrange_cursor,arrange_left,arrange_span=30720,arrange_grid=960;
static int arrange_range,arrange_data_held,arrange_data_moved,arrange_editing;
static int64_t arrange_anchor,arrange_new_length=3840;
static struct pt_arr_clip arrange_draft;
static uint64_t arrange_edit_revision;
static int arrange_edit_row;
static int64_t arrange_edit_cursor,arrange_edit_left;
static const int64_t arrange_grids[]={240,480,960,1920,3840};
static const char*const arrange_actions[]={"Add track","Insert pattern","Copy block","Paste block",
    "Move selected here","Extend one cycle","Trim end to cursor","Join next block",
    "Mute track","Solo track","Delete block","Song +16 beats","Song -16 beats","Open pattern","New empty pattern","Create variation","Render to current seq"};
/* Stage 1 of Arrange v2: whole song into the current stock sequence (one Undo). */
extern int step_native_render_arrangement(int,char*,unsigned) __attribute__((weak));
static uint64_t arrange_render_confirm_until;
#define ARRANGE_ACTIONS ((int)(sizeof arrange_actions/sizeof *arrange_actions))
/* ---- Arrange v2 stage 1b: Play/Stop through the stock Song transport ----
 * Play renders the whole arrangement into the Arrange slot (Seq 128), temporarily
 * replaces the current Song's steps with [Seq 128 x1] using the same native chain
 * commands as this Song screen, selects step 0 and forwards Play. Stop, the end of
 * playback or leaving Arrange restore the user's steps and row. UI thread only. */
#define ARRANGE_SLOT 127
extern int step_native_render_arrangement_slot(void*,int,int,char*,unsigned) __attribute__((weak));
extern void phone_native_inject_button(unsigned,unsigned) __attribute__((weak));
static int arrange_play_state; /* 0 idle, 1 waiting for swapped song, 2 playing, 3 stopping */
static int arrange_saved_seq[512],arrange_saved_rep[512],arrange_saved_count=-1,arrange_saved_row;
static void *arrange_saved_song,*arrange_saved_sequencer;static uint64_t arrange_saved_epoch;
static uint64_t arrange_play_forward_until; /* our injected Play passes to the stock Song */
static unsigned arrange_forwarded; /* 1 Play, 2 Stop: press forwarded, let its release through too */
static int arrange_forwarded_release(unsigned code){
    unsigned bit=code==0x52?1u:code==0x51?2u:0u;
    if(!bit||!(arrange_forwarded&bit))return 0;
    arrange_forwarded&=~bit;return 1;
}
static uint64_t arrange_play_deadline,arrange_play_confirm_until;
static int arrange_was_running;
static int arrange_song_alive(void*sequencer,void*song){
    if(!sequencer||!song)return 0;
    if(FIELD(sequencer,0x1270,void*)==song)return 1;
    unsigned n=FIELD(sequencer,0x126c,unsigned);void**songs=FIELD(sequencer,0x1260,void**);
    for(unsigned i=0;songs&&i<n&&i<64;++i)if(songs[i]==song)return 1;
    return 0;
}
static void arrange_song_restore(void){
    if(arrange_saved_count<0)return;
    void *sequencer=arrange_saved_sequencer,*song=arrange_saved_song;
    if(arrange_saved_epoch!=pt_epoch()||!arrange_song_alive(sequencer,song)){report("ARRANGE_PLAY restore skipped: project changed");arrange_saved_count=-1;return;}
    void *ctx;FN(0x19a0b50,void(*)(void**,void*))(&ctx,sequencer);
    FN(0x19a3a40,void(*)(void**,void*))(&ctx,song);
    for(int i=0;i<arrange_saved_count;++i){
        FN(0x19a0b50,void(*)(void**,void*))(&ctx,sequencer);
        FN(0x19a2d20,void(*)(void**,void*,long,int,int))(&ctx,song,(long)i,arrange_saved_seq[i],arrange_saved_rep[i]);
    }
    pending_row=arrange_saved_row;pending_count=arrange_saved_count;pending_until=now_ms()+1500;
    {char m[96];snprintf(m,sizeof m,"ARRANGE_PLAY restored steps=%d row=%d",arrange_saved_count,arrange_saved_row);report(m);}
    arrange_saved_count=-1;arrange_saved_song=arrange_saved_sequencer=NULL;
}
static void arrange_play_stop(int inject_stop){
    if(inject_stop&&(arrange_play_state==2||arrange_play_state==3)&&phone_native_inject_button){phone_native_inject_button(0x51,1);phone_native_inject_button(0x51,0);}
    arrange_play_state=0;arrange_was_running=0;
    arrange_song_restore();
}
static void arrange_play_start(void){
    struct song_ref r;
    if(!song_ref(&r)){say("Song unavailable");return;}
    if(r.count>512){say("Song has too many steps");return;}
    if(!step_native_render_arrangement_slot||!phone_native_inject_button){say("Playback unavailable in this build");return;}
    uint64_t now=now_ms();
    char msg[96];int rr=step_native_render_arrangement_slot(r.sequencer,ARRANGE_SLOT,arrange_play_confirm_until&&now<arrange_play_confirm_until,msg,sizeof msg);
    arrange_play_confirm_until=rr==-2?now+6000:0;
    if(rr<0){say(msg);return;}
    /* Save the user's steps, then swap in the single Arrange step. */
    arrange_saved_count=r.count;arrange_saved_row=r.row;arrange_saved_song=r.song;arrange_saved_sequencer=r.sequencer;arrange_saved_epoch=pt_epoch();
    for(int i=0;i<r.count;++i)step_at(&r,i,&arrange_saved_seq[i],&arrange_saved_rep[i]);
    void *ctx;FN(0x19a0b50,void(*)(void**,void*))(&ctx,r.sequencer);
    FN(0x19a3a40,void(*)(void**,void*))(&ctx,r.song);
    FN(0x19a0b50,void(*)(void**,void*))(&ctx,r.sequencer);
    FN(0x19a2d20,void(*)(void**,void*,long,int,int))(&ctx,r.song,0L,ARRANGE_SLOT,1);
    arrange_play_state=1;arrange_play_deadline=now+1500;
    {char m[128];snprintf(m,sizeof m,"ARRANGE_PLAY swap saved=%d row=%d %s",arrange_saved_count,arrange_saved_row,msg);report(m);}
    say("Arrange: preparing playback");
}
/* Called every UI tick while Arrange is open. */
static void arrange_play_tick(void){
    unsigned transport=step_native_transport_state?step_native_transport_state():0;
    int running=(transport&3u)==3u;
    if(arrange_play_state==1){
        struct song_ref r;int seq=-1,rep=0;
        int ready=song_ref(&r)&&r.count==1&&step_at(&r,0,&seq,&rep)&&seq==ARRANGE_SLOT;
        if(ready){
            FN(0x14e19c0,void(*)(void*,int))((char*)r.controller+0x168,0);
            arrange_play_forward_until=now_ms()+400;phone_native_inject_button(0x52,1);phone_native_inject_button(0x52,0);
            arrange_play_state=2;arrange_was_running=0;arrange_play_deadline=now_ms()+3000;
            report("ARRANGE_PLAY started");say("Arrange playing (Stop to return)");
        }else if(now_ms()>arrange_play_deadline){report("ARRANGE_PLAY swap timeout");say("Playback failed: song not ready");arrange_play_stop(0);}
    }else if(arrange_play_state==2){
        if(running)arrange_was_running=1;
        else if(arrange_was_running||now_ms()>arrange_play_deadline){report("ARRANGE_PLAY stopped");arrange_play_stop(0);say("Arrange stopped");}
    }else if(arrange_play_state==3){
        if(!running||now_ms()>arrange_play_deadline){report("ARRANGE_PLAY stopped by Stop");arrange_play_stop(0);say("Arrange stopped");}
    }
}
static int arrange_clamp(int x,int lo,int hi){return x<lo?lo:x>hi?hi:x;}
static void arrange_close(void){
    if(arrange_play_state)arrange_play_stop(1);
    if(atomic_exchange(&arrangement_editor,0)&&step_pattern_close)step_pattern_close();
    if(atomic_exchange(&arrangement_active,0))atomic_fetch_add(&input_epoch,1);
    arrange_menu=0;arrange_selected=0;memset(&arrange_clipboard,0,sizeof arrange_clipboard);
    arrange_range=arrange_data_held=arrange_data_moved=arrange_editing=0;
}
static void arrange_open(void){
    unsigned transport=step_native_transport_state?step_native_transport_state():0;
    if(!(transport&2u)||(transport&5u)){say("Stop transport before Arrange");return;}
    arrange_epoch=pt_epoch();pt_expect_epoch(arrange_epoch);
    arrange_row=arrange_top=arrange_menu=arrange_pick=0;arrange_cursor=arrange_left=0;
    arrange_selected=0;memset(&arrange_clipboard,0,sizeof arrange_clipboard);arrange_span=30720;arrange_rows=4;
    arrange_range=arrange_data_held=arrange_data_moved=arrange_editing=0;
    atomic_store(&export_menu_active,0);atomic_fetch_add(&input_epoch,1);atomic_store(&arrangement_active,1);
    say("Arrange editor; playback not connected");repaint(screen);
}
static int arrange_clip(uint64_t id,struct pt_arr_clip*out){
    struct pt_arrangement a;if(!id||!pt_arr_info(&a))return 0;
    for(unsigned i=0;i<a.clips;++i)if(pt_arr_clip_at(i,out)&&out->id==id)return 1;
    return 0;
}
static int arrange_target(struct pt_arr_clip*out){
    if(arrange_selected)return arrange_clip(arrange_selected,out);
    struct pt_arrangement a;struct pt_arr_track t;
    if(!pt_arr_info(&a)||!pt_arr_track_at((unsigned)arrange_row,&t))return 0;
    for(unsigned i=0;i<a.clips;++i)if(pt_arr_clip_at(i,out)&&out->track==t.id&&arrange_cursor>=out->start&&arrange_cursor<out->start+out->length)return 1;
    return 0;
}
static void arrange_tick(void){
    if(arrange_play_state)arrange_play_tick();
    if(arrange_epoch!=pt_epoch()){arrange_new_length=3840;arrange_close();say("Project changed: reopen Arrange");repaint(screen);return;}
    struct pt_arrangement a;if(!pt_arr_info(&a)){arrange_close();return;}
    if(atomic_load(&arrangement_editor)&&(!step_pattern_active||!step_pattern_active())){
        atomic_store(&arrangement_editor,0);atomic_fetch_add(&input_epoch,1);repaint(screen);
    }
    arrange_row=arrange_clamp(arrange_row,0,a.tracks?(int)a.tracks-1:0);
    if(arrange_cursor>a.length)arrange_cursor=a.length;
    if(arrange_cursor<0)arrange_cursor=0;
    if(arrange_span>a.length)arrange_span=a.length;
    if(arrange_left>a.length-arrange_span)arrange_left=a.length-arrange_span;
    if(arrange_left<0)arrange_left=0;
    if(arrange_row<arrange_top)arrange_top=arrange_row;
    if(arrange_row>=arrange_top+arrange_rows)arrange_top=arrange_row-arrange_rows+1;
    arrange_top=arrange_clamp(arrange_top,0,a.tracks>(unsigned)arrange_rows?(int)a.tracks-arrange_rows:0);
    struct pt_arr_clip c;if(arrange_selected&&!arrange_clip(arrange_selected,&c))arrange_selected=0;
}
static void arrange_result(int ok){say(ok?"Arrangement updated; save project":pt_error()[0]?pt_error():"Block operation unavailable");arrange_tick();repaint(screen);}
static void arrange_cut(void){
    struct pt_arr_clip c;uint64_t right=0;
    if(!arrange_target(&c)){say("Select a block before Chop");return;}
    if(arrange_cursor<=c.start||arrange_cursor>=c.start+c.length){say("Chop cursor must be inside block");return;}
    int ok=pt_arr_cut(c.id,arrange_cursor,&right);if(ok)arrange_selected=right;arrange_result(ok);
}
static void arrange_redraw(void){if(screen&&atomic_load(&shown)&&atomic_load(&arrangement_active))repaint(screen);}
static void arrange_separated(uint64_t id){
    if(arrange_epoch!=pt_epoch()||!atomic_load(&arrangement_active))return;
    struct pt_arr_clip c;struct pt_arrangement a;
    if(pt_arr_clip_by_id(id,&c)&&pt_arr_info(&a)){
        arrange_selected=id;arrange_cursor=c.start;
        for(unsigned i=0;i<a.tracks;++i){struct pt_arr_track t;if(pt_arr_track_at(i,&t)&&t.id==c.track){arrange_row=(int)i;break;}}
    }
    arrange_tick();say("Part ready: DATA time / B3 Edit / Chop");repaint(screen);
}
static void arrange_edit_pattern(void){
    struct pt_arr_clip c;struct pt_entry p;
    if(!arrange_target(&c)){say("Choose a block to edit");return;}
    int index=pt_pattern_index(c.pattern);
    if(index<0||!pt_info((unsigned)index,&p)){say("Pattern unavailable");return;}
    int64_t phase=c.offset;
    if(arrange_cursor>=c.start&&arrange_cursor<c.start+c.length)phase=(phase+(arrange_cursor-c.start)%p.length)%p.length;
    if(!step_pattern_open||!step_pattern_active||!step_pattern_close||!step_pattern_button||!step_pattern_control||!step_pattern_pad||!step_pattern_paint||
       !(step_pattern_open_clip?step_pattern_open_clip(c.id,phase,arrange_redraw,arrange_separated):step_pattern_open(c.pattern,phase,arrange_redraw))){
        say("Pattern editor unavailable; stop transport");return;
    }
    arrange_menu=0;arrange_range=arrange_data_held=arrange_data_moved=0;atomic_store(&arrangement_editor,1);atomic_fetch_add(&input_epoch,1);repaint(screen);
}
static void arrange_new_pattern(void){
    struct pt_arrangement a;struct pt_arr_track row;struct pt_entry p;uint64_t route=0,id=0;
    if(!pt_arr_info(&a)||a.read_only){say("Arrangement is read only");return;}
    uint64_t track=pt_arr_track_at((unsigned)arrange_row,&row)?row.id:0;
    int64_t nearest=-1;
    for(unsigned i=0;i<a.clips;++i){struct pt_arr_clip c;if(pt_arr_clip_at(i,&c)&&c.track==track&&c.start<=arrange_cursor&&c.start>=nearest){route=c.pattern;nearest=c.start;}}
    if(!route&&pt_info(0,&p))route=p.id;
    int64_t start=arrange_cursor,length=arrange_new_length;
    if(arrange_range){start=arrange_anchor<arrange_cursor?arrange_anchor:arrange_cursor;length=(arrange_anchor>arrange_cursor?arrange_anchor:arrange_cursor)-start+arrange_grid;}
    if(!pt_arr_new_pattern(track,route,start,length,&id)){arrange_result(0);return;}
    arrange_new_length=length;arrange_cursor=start;arrange_selected=id;arrange_menu=arrange_range=0;
    if(!a.clips){arrange_span=length*2;if(arrange_span<7680)arrange_span=7680;}
    if(start<arrange_left||start+length>arrange_left+arrange_span)arrange_left=start;
    arrange_edit_pattern();arrange_selected=0;arrange_tick();repaint(screen);
}
static void arrange_begin_edit(void){
    struct pt_arrangement a;if(!arrange_target(&arrange_draft)||!pt_arr_info(&a)||a.read_only){say("Select an editable block");return;}
    arrange_selected=arrange_draft.id;arrange_edit_revision=a.revision;arrange_editing=1;
    arrange_edit_row=arrange_row;arrange_edit_cursor=arrange_cursor;arrange_edit_left=arrange_left;
    arrange_range=arrange_data_held=arrange_data_moved=0;
    say("Preview: DATA time / Fader track / K3 length");repaint(screen);
}
static void arrange_release(unsigned code){
    if(code!=0x6f||!arrange_data_held)return;
    arrange_data_held=0;
    if(!arrange_data_moved){struct pt_arr_clip c;arrange_range=0;if(arrange_selected)arrange_selected=0;else if(arrange_target(&c))arrange_selected=c.id;}
    arrange_data_moved=0;repaint(screen);
}
static void arrange_execute(void){
    struct pt_arrangement a;struct pt_arr_track t;struct pt_arr_clip c;
    if(!pt_arr_info(&a))return;
    int have_track=pt_arr_track_at((unsigned)arrange_row,&t),have_clip=arrange_target(&c),ok=0;
    uint64_t id=0;
    if(arrange_pick==0){char name[32];snprintf(name,sizeof name,"Track %u",a.tracks+1);ok=pt_arr_add_track(name,&id);if(ok)arrange_row=(int)a.tracks;}
    else if(arrange_pick==1){if(!have_track){say("Add a track first");return;}if(!pt_count()){say("Create Parts in Events > Piano Roll");return;}arrange_menu=2;arrange_pick=0;return;}
    else if(arrange_pick==2){if(!have_clip){say("No block to copy");return;}arrange_clipboard=c;arrange_menu=0;say("Copied; move cursor, Actions > Paste");return;}
    else if(arrange_pick==3){if(!have_track||!arrange_clipboard.pattern){say("Copy a block first");return;}ok=pt_arr_paste(&arrange_clipboard,t.id,arrange_cursor,&id);if(ok)arrange_selected=id;}
    else if(arrange_pick==4){if(!have_clip||!have_track){say("Select a block, then choose destination");return;}ok=pt_arr_move(c.id,t.id,arrange_cursor);}
    else if(arrange_pick==5){struct pt_entry e;int p=have_clip?pt_pattern_index(c.pattern):-1;if(p<0||!pt_info((unsigned)p,&e)){say("Select a block first");return;}ok=pt_arr_resize(c.id,c.length+e.length);}
    else if(arrange_pick==6){if(!have_clip){say("Select a block first");return;}ok=pt_arr_resize(c.id,arrange_cursor-c.start);}
    else if(arrange_pick==7){if(!have_clip){say("Select left block to join");return;}for(unsigned i=0;i<a.clips;++i){struct pt_arr_clip next;if(pt_arr_clip_at(i,&next)&&next.track==c.track&&next.start==c.start+c.length){id=next.id;break;}}if(!id){say("No adjacent right block");return;}ok=pt_arr_join(c.id,id);if(ok)arrange_selected=c.id;}
    else if(arrange_pick==8||arrange_pick==9){if(!have_track){say("Add a track first");return;}ok=pt_arr_track_state(t.id,arrange_pick==8?!t.muted:t.muted,arrange_pick==9?!t.solo:t.solo);}
    else if(arrange_pick==10){if(!have_clip){say("Select a block first");return;}arrange_selected=c.id;arrange_menu=3;return;}
    else if(arrange_pick==13){arrange_edit_pattern();return;}
    else if(arrange_pick==14){arrange_new_pattern();return;}
    else if(arrange_pick==15){if(!have_clip){say("Choose a block first");return;}ok=pt_arr_variation(c.id);}
    else if(arrange_pick==16){
        if(!step_native_render_arrangement){say("Render unavailable in this build");return;}
        struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);uint64_t now=(uint64_t)ts.tv_sec*1000+(uint64_t)ts.tv_nsec/1000000;
        char msg[96];int r=step_native_render_arrangement(arrange_render_confirm_until&&now<arrange_render_confirm_until,msg,sizeof msg);
        arrange_render_confirm_until=r==-2?now+6000:0;
        say(msg);if(r>=0)arrange_menu=0;return;
    }
    else ok=pt_arr_length(a.length+(arrange_pick==11?15360:-15360));
    if(ok)arrange_menu=0;
    arrange_result(ok);
}
static int arrange_button(unsigned code,int shifted){
    arrange_tick();if(!atomic_load(&arrangement_active))return 1;
    pt_expect_epoch(arrange_epoch);
    if(atomic_load(&arrangement_editor)){step_pattern_button(code,1,shifted);arrange_tick();repaint(screen);return 1;}
    if(code==B1&&shifted){arrange_close();say("Sequential Song");repaint(screen);return 1;}
    if(code==B3&&shifted&&!arrange_editing){arrange_menu=arrange_menu?0:1;arrange_pick=0;arrange_data_held=0;repaint(screen);return 1;}
    if(code==0x52&&now_ms()<arrange_play_forward_until){arrange_forwarded|=1u;return 0;} /* our injected Play goes to the stock Song */
    if(code==0x52&&!shifted){if(arrange_play_state)say("Arrange is playing");else arrange_play_start();repaint(screen);return 1;}
    if(code==0x51&&arrange_play_state==1){arrange_play_stop(0);say("Arrange playback cancelled");repaint(screen);return 1;}
    if(code==0x51&&arrange_play_state==2){arrange_play_state=3;arrange_play_deadline=now_ms()+2000;arrange_forwarded|=2u;return 0;} /* stock Stop, restore when stopped */
    if(code==0x50){say("Arrange record not connected yet");return 1;}
    if((code==PLUS||code==MINUS)&&shifted){arrange_editing=0;arrange_result(pt_history(code==MINUS?-1:1));return 1;}
    if(code==B1){if(arrange_editing){arrange_editing=0;arrange_row=arrange_edit_row;arrange_cursor=arrange_edit_cursor;arrange_left=arrange_edit_left;say("Block edit cancelled");}else if(arrange_menu)arrange_menu=0;else if(arrange_selected||arrange_range){arrange_selected=0;arrange_range=0;}else{arrange_close();say("Sequential Song");}arrange_data_held=0;repaint(screen);return 1;}
    if(arrange_editing){if(code==B3){int ok=pt_arr_edit_clip(arrange_draft.id,arrange_draft.track,arrange_draft.start,arrange_draft.length,arrange_edit_revision);if(ok){arrange_editing=0;arrange_new_length=arrange_draft.length;}arrange_result(ok);}return 1;}
    if(code==0x6f&&!arrange_menu){arrange_data_held=1;arrange_data_moved=0;arrange_anchor=arrange_cursor;return 1;}
    if(code==B2){if(arrange_menu)arrange_menu=0;else{arrange_range=!arrange_range;arrange_anchor=arrange_cursor;arrange_selected=0;}repaint(screen);return 1;}
    if(code==0x4b){if(!arrange_menu)arrange_cut();repaint(screen);return 1;}
    if(code==B3){
        if(arrange_menu==1)arrange_execute();
        else if(arrange_menu==2){struct pt_arr_track t;struct pt_entry e;uint64_t id=0;
            if(pt_arr_track_at((unsigned)arrange_row,&t)&&pt_info((unsigned)arrange_pick,&e)){
                int ok=pt_arr_add_clip(t.id,(unsigned)arrange_pick,arrange_cursor,e.length,&id);if(ok){arrange_selected=id;arrange_menu=0;}arrange_result(ok);
            }
        }else if(arrange_menu==3){int ok=pt_arr_remove(arrange_selected);if(ok){arrange_selected=0;arrange_menu=0;}arrange_result(ok);}
        else if(arrange_selected)arrange_begin_edit();
        else{struct pt_arr_clip c;if(arrange_target(&c)&&!arrange_range)arrange_edit_pattern();else arrange_new_pattern();}
    }else if(!arrange_menu&&(code==PLUS||code==MINUS))arrange_row+=code==PLUS?1:-1;
    arrange_tick();repaint(screen);return 1;
}
static int arrange_control(unsigned cc,unsigned value,int shifted){
    arrange_tick();if(!atomic_load(&arrangement_active))return 1;
    struct pt_arrangement a;if(!pt_arr_info(&a))return 1;
    if(atomic_load(&arrangement_editor)){step_pattern_control(cc,value,shifted);arrange_tick();return 1;}
    int delta=value>64?(int)value-128:value==64?0:(int)value;
    if(arrange_editing){
        if(cc==DATA_ENCODER)arrange_draft.start+=(int64_t)delta*(shifted?1:arrange_grid);
        else if(cc==0x10)arrange_draft.start=(int64_t)value*(a.length-arrange_draft.length)/127;
        else if(cc==0x14||cc==0x11){int index=cc==0x14?ui_list_fader(value,(int)a.tracks-1):(int)value*((int)a.tracks-1)/127;struct pt_arr_track row;if(pt_arr_track_at((unsigned)index,&row)){arrange_draft.track=row.id;arrange_row=index;}}
        else if(cc==0x12){int64_t available=a.length-arrange_draft.start;int64_t units=available/arrange_grid;arrange_draft.length=units>0?(1+(int64_t)value*(units-1)/127)*arrange_grid:available;}
        if(arrange_draft.start<0)arrange_draft.start=0;
        if(arrange_draft.start>a.length-arrange_draft.length)arrange_draft.start=a.length-arrange_draft.length;
        arrange_cursor=arrange_draft.start;arrange_tick();
        if(arrange_cursor<arrange_left)arrange_left=arrange_cursor;
        if(arrange_cursor>arrange_left+arrange_span)arrange_left=arrange_cursor-arrange_span;
        repaint(screen);return 1;
    }
    if(arrange_data_held&&!arrange_menu&&(cc==DATA_ENCODER||cc==0x14)&&!shifted){arrange_data_moved=1;arrange_range=1;arrange_selected=0;}
    if(arrange_menu){int n=arrange_menu==1?ARRANGE_ACTIONS:arrange_menu==2?(int)pt_count():0;
        if(n){if(cc==DATA_ENCODER)arrange_pick=arrange_clamp(arrange_pick+delta,0,n-1);else if(cc==0x14)arrange_pick=ui_list_fader(value,n-1);else if(cc==K1)arrange_pick=(int)value*(n-1)/127;}
    }else if(cc==DATA_ENCODER){
        if(shifted){int n=delta>0?delta:-delta;if(n>8)n=8;while(n--){if(delta>0&&arrange_span>960)arrange_span/=2;else if(delta<0&&arrange_span<a.length)arrange_span*=2;}if(arrange_span<960)arrange_span=960;}
        else arrange_cursor+=(int64_t)delta*arrange_grid;
    }else if(cc==0x14){if(shifted)arrange_rows=6-(int)value*3/127;else arrange_row=ui_list_fader(value,(int)a.tracks-1);}
    else if(cc==0x10){int relative=arrange_row-arrange_top;arrange_top=(int)value*(a.tracks>(unsigned)arrange_rows?(int)a.tracks-arrange_rows:0)/127;arrange_row=arrange_top+relative;}
    else if(cc==0x11)arrange_grid=arrange_grids[value*5/128];
    else if(cc==0x12)arrange_left=(int64_t)value*(a.length>arrange_span?a.length-arrange_span:0)/127;
    arrange_tick();
    if(cc==DATA_ENCODER&&!shifted&&!arrange_menu){if(arrange_cursor<arrange_left)arrange_left=arrange_cursor;if(arrange_cursor>arrange_left+arrange_span)arrange_left=arrange_cursor-arrange_span;}
    repaint(screen);return 1;
}
static int arrange_x(int64_t tick){return 72+(int)((tick-arrange_left)*240/arrange_span);}
static void arrange_paint(void*g){
    arrange_tick();if(!atomic_load(&arrangement_active))return;
    if(atomic_load(&arrangement_editor)&&step_pattern_paint(g,atomic_load(&export_shift)))return;
    struct pt_arrangement a;if(!pt_arr_info(&a))return;
    colour(g,0xff000000u);rect(g,0,0,320,240);
    struct pt_arr_clip under;int have_under=arrange_target(&under);
    colour(g,0xffffffffu);text(g,atomic_load(&export_shift)?"Song":arrange_editing?"Cancel":"Back",0,4,106,25,13);
    text(g,arrange_editing?"":arrange_menu?"Close":arrange_range?"Range On":"Range",107,4,106,25,13);colour(g,0xffffc247u);text(g,arrange_editing?"Apply":atomic_load(&export_shift)?"Actions":arrange_menu==3?"Delete":arrange_menu?"Choose":arrange_selected?"Edit":have_under&&!arrange_range?"Open":"New",214,4,106,25,13);
    text_at(g,arrange_menu==1?"Arrange / Actions":arrange_menu==2?"Insert pattern":a.read_only?"Arrange / read only":"Song / Arrange",6,34,arrange_menu?236:308,23,17,0x21);
    if(arrange_menu==3){colour(g,0xffffffffu);text(g,"Delete selected block?",8,93,304,27,18);text(g,"Pattern stays in the library",8,126,304,22,13);}
    else if(arrange_menu){int n=arrange_menu==1?ARRANGE_ACTIONS:(int)pt_count(),first=ui_list_first(arrange_pick,n,6);
        char position[32];snprintf(position,sizeof position,"%d / %d",n?arrange_pick+1:0,n);colour(g,0xffaaaaaau);text_at(g,position,245,36,67,20,12,0x22);
        for(int i=first;i<n&&i<first+6;++i){char label[80];if(arrange_menu==1)snprintf(label,sizeof label,"%s",arrange_actions[i]);else{struct pt_entry e;if(!pt_info((unsigned)i,&e))continue;snprintf(label,sizeof label,"%.40s / %lld q",e.name,(long long)(e.length/960));}
            int y=62+(i-first)*22;if(i==arrange_pick){colour(g,0xff303030u);rect(g,6,y,302,22);}colour(g,i==arrange_pick?0xffffc247u:0xffffffffu);text_at(g,label,11,y,294,22,13,0x21);
        }
        int offset,thumb=ui_list_thumb(first,n,6,132,&offset);colour(g,0xff303030u);rect(g,314,62,2,132);colour(g,0xffffc247u);rect(g,314,62+offset,2,thumb);
    }else{
        int h=120/arrange_rows;
        for(int i=0;i<arrange_rows;++i){unsigned index=(unsigned)(arrange_top+i);struct pt_arr_track t;if(!pt_arr_track_at(index,&t))break;int y=76+i*h;
            colour(g,index==(unsigned)arrange_row?0xff252525u:0xff101010u);rect(g,0,y,312,h-1);
            char name[48];snprintf(name,sizeof name,"%s%.7s",t.muted?"M ":t.solo?"S ":"",t.name);colour(g,index==(unsigned)arrange_row?0xffffc247u:0xffffffffu);text_at(g,name,3,y,66,h-1,11,0x21);
            for(unsigned j=0;j<a.clips;++j){struct pt_arr_clip c;if(!pt_arr_clip_at(j,&c))continue;if(arrange_editing&&c.id==arrange_draft.id)c=arrange_draft;if(c.track!=t.id||c.start>=arrange_left+arrange_span||c.start+c.length<=arrange_left)continue;
                int x=c.start<arrange_left?72:arrange_x(c.start),end=c.start+c.length>arrange_left+arrange_span?312:arrange_x(c.start+c.length);if(end<=x)end=x+1;
                colour(g,c.muted||t.muted?0xff333333u:0xff344451u);rect(g,x,y+2,end-x,h-5);
                colour(g,c.id==arrange_selected?0xffffc247u:0xff71c1e8u);rect(g,x,y+2,end-x,2);
                struct pt_entry e;int p=pt_pattern_index(c.pattern);if(end-x>24&&p>=0&&pt_info((unsigned)p,&e)){colour(g,0xffffffffu);text_at(g,e.name,x+2,y+5,end-x-3,h-9,11,0x21);}
            }
            if(arrange_range&&index==(unsigned)arrange_row){int64_t left=arrange_anchor<arrange_cursor?arrange_anchor:arrange_cursor,right=(arrange_anchor>arrange_cursor?arrange_anchor:arrange_cursor)+arrange_grid;int x=left<arrange_left?72:arrange_x(left),end=right>arrange_left+arrange_span?312:arrange_x(right);if(end>x&&x<312&&end>72){colour(g,0xffffc247u);rect(g,x,y,end-x,1);rect(g,x,y+h-2,end-x,1);}}
        }
        for(int i=0;i<=4;++i){int x=72+60*i;char beat[24];snprintf(beat,sizeof beat,"%lldq",(long long)((arrange_left+arrange_span*i/4)/960+1));colour(g,0xff777777u);text_at(g,beat,x<290?x:283,59,34,14,10,0x21);colour(g,0xff444444u);rect(g,x,75,1,121);}
        if(arrange_cursor>=arrange_left&&arrange_cursor<=arrange_left+arrange_span){colour(g,0xffffc247u);rect(g,arrange_x(arrange_cursor),73,1,124);}
        if(!a.tracks){colour(g,0xffffffffu);text(g,"B3 New: create a pattern",8,108,304,25,16);}
        int offset,thumb=ui_list_thumb(arrange_top,(int)a.tracks,arrange_rows,120,&offset);
        colour(g,0xff303030u);rect(g,315,76,2,120);colour(g,0xffffc247u);rect(g,315,76+offset,2,thumb);
    }
    char status[96];snprintf(status,sizeof status,"q %.2f / Grid %.2f / %u blocks",1.+arrange_cursor/960.,arrange_grid/960.,a.clips);
    struct pt_arr_clip selected_clip;struct pt_entry selected_pattern;
    if(!arrange_menu&&arrange_target(&selected_clip)){
        int index=pt_pattern_index(selected_clip.pattern);
        if(index>=0&&pt_info((unsigned)index,&selected_pattern))snprintf(status,sizeof status,"q %.2f / Block %.2fq / Pattern %.2fq",1.+arrange_cursor/960.,selected_clip.length/960.,selected_pattern.length/960.);
    }
    if(arrange_editing)snprintf(status,sizeof status,"Preview q %.2f / Length %.2fq",1.+arrange_draft.start/960.,arrange_draft.length/960.);
    colour(g,0xffaaaaaau);text_at(g,arrange_menu?"DATA / Fader: choose":status,6,199,308,15,11,0x21);
    colour(g,0xffffc247u);text_at(g,toast_until&&now_ms()<toast_until?toast:arrange_menu?"B1 Back / B3 Choose":"DATA select / Shift B3 Actions / Chop",6,215,308,19,11,0x21);
}
