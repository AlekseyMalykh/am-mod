/* SongScreen UI owner only. Producer sees arrangement_active, never these
 * values or native pointers. Timeline units are quarter-note beats (960). */
static uint64_t arrange_epoch,arrange_selected;
static struct pt_arr_clip arrange_clipboard;
static int arrange_row,arrange_top,arrange_rows=4,arrange_menu,arrange_pick;
static int64_t arrange_cursor,arrange_left,arrange_span=30720,arrange_grid=960;
static const int64_t arrange_grids[]={240,480,960,1920,3840};
static const char*const arrange_actions[]={"Add track","Insert pattern","Copy block","Paste block",
    "Move selected here","Extend one cycle","Trim end to cursor","Join next block",
    "Mute track","Solo track","Delete block","Song +16 beats","Song -16 beats"};
static int arrange_clamp(int x,int lo,int hi){return x<lo?lo:x>hi?hi:x;}
static void arrange_close(void){if(atomic_exchange(&arrangement_active,0))atomic_fetch_add(&input_epoch,1);arrange_menu=0;arrange_selected=0;memset(&arrange_clipboard,0,sizeof arrange_clipboard);}
static void arrange_open(void){
    unsigned transport=step_native_transport_state?step_native_transport_state():0;
    if(!(transport&2u)||(transport&5u)){say("Stop transport before Arrange");return;}
    arrange_epoch=pt_epoch();pt_expect_epoch(arrange_epoch);
    arrange_row=arrange_top=arrange_menu=arrange_pick=0;arrange_cursor=arrange_left=0;
    arrange_selected=0;memset(&arrange_clipboard,0,sizeof arrange_clipboard);arrange_span=30720;arrange_rows=4;
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
    if(arrange_epoch!=pt_epoch()){arrange_close();say("Project changed: reopen Arrange");repaint(screen);return;}
    struct pt_arrangement a;if(!pt_arr_info(&a)){arrange_close();return;}
    arrange_row=arrange_clamp(arrange_row,0,a.tracks?(int)a.tracks-1:0);
    if(arrange_cursor>a.length)arrange_cursor=a.length;
    if(arrange_cursor<0)arrange_cursor=0;
    if(arrange_span>a.length)arrange_span=a.length;
    if(arrange_left>a.length-arrange_span)arrange_left=a.length-arrange_span;
    if(arrange_left<0)arrange_left=0;
    if(arrange_row<arrange_top)arrange_top=arrange_row;
    if(arrange_row>=arrange_top+arrange_rows)arrange_top=arrange_row-arrange_rows+1;
    struct pt_arr_clip c;if(arrange_selected&&!arrange_clip(arrange_selected,&c))arrange_selected=0;
}
static void arrange_result(int ok){say(ok?"Arrangement updated; save project":pt_error()[0]?pt_error():"Block operation unavailable");arrange_tick();repaint(screen);}
static void arrange_cut(void){
    struct pt_arr_clip c;uint64_t right=0;
    if(!arrange_target(&c)){say("Select a block before Chop");return;}
    if(arrange_cursor<=c.start||arrange_cursor>=c.start+c.length){say("Chop cursor must be inside block");return;}
    int ok=pt_arr_cut(c.id,arrange_cursor,&right);if(ok)arrange_selected=right;arrange_result(ok);
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
    else if(arrange_pick==4){if(!have_clip||!have_track){say("Pin a block, then choose destination");return;}ok=pt_arr_move(c.id,t.id,arrange_cursor);}
    else if(arrange_pick==5){struct pt_entry e;int p=have_clip?pt_pattern_index(c.pattern):-1;if(p<0||!pt_info((unsigned)p,&e)){say("Select a block first");return;}ok=pt_arr_resize(c.id,c.length+e.length);}
    else if(arrange_pick==6){if(!have_clip){say("Select a block first");return;}ok=pt_arr_resize(c.id,arrange_cursor-c.start);}
    else if(arrange_pick==7){if(!have_clip){say("Select left block to join");return;}for(unsigned i=0;i<a.clips;++i){struct pt_arr_clip next;if(pt_arr_clip_at(i,&next)&&next.track==c.track&&next.start==c.start+c.length){id=next.id;break;}}if(!id){say("No adjacent right block");return;}ok=pt_arr_join(c.id,id);if(ok)arrange_selected=c.id;}
    else if(arrange_pick==8||arrange_pick==9){if(!have_track){say("Add a track first");return;}ok=pt_arr_track_state(t.id,arrange_pick==8?!t.muted:t.muted,arrange_pick==9?!t.solo:t.solo);}
    else if(arrange_pick==10){if(!have_clip){say("Select a block first");return;}arrange_selected=c.id;arrange_menu=3;return;}
    else ok=pt_arr_length(a.length+(arrange_pick==11?15360:-15360));
    if(ok)arrange_menu=0;
    arrange_result(ok);
}
static int arrange_button(unsigned code,int shifted){
    arrange_tick();if(!atomic_load(&arrangement_active))return 1;
    pt_expect_epoch(arrange_epoch);
    if(code==0x52||code==0x50){say("Arrange playback/record not connected");return 1;}
    if((code==PLUS||code==MINUS)&&shifted){arrange_result(pt_history(code==MINUS?-1:1));return 1;}
    if(code==B1){if(arrange_menu)arrange_menu=0;else{arrange_close();say("Sequential Song");}repaint(screen);return 1;}
    if(code==B2){arrange_menu=arrange_menu?0:1;arrange_pick=0;repaint(screen);return 1;}
    if(code==0x4b){if(!arrange_menu)arrange_cut();repaint(screen);return 1;}
    if(code==B3){
        if(arrange_menu==1)arrange_execute();
        else if(arrange_menu==2){struct pt_arr_track t;struct pt_entry e;uint64_t id=0;
            if(pt_arr_track_at((unsigned)arrange_row,&t)&&pt_info((unsigned)arrange_pick,&e)){
                int ok=pt_arr_add_clip(t.id,(unsigned)arrange_pick,arrange_cursor,e.length,&id);if(ok){arrange_selected=id;arrange_menu=0;}arrange_result(ok);
            }
        }else if(arrange_menu==3){int ok=pt_arr_remove(arrange_selected);if(ok){arrange_selected=0;arrange_menu=0;}arrange_result(ok);}
        else if(arrange_selected)arrange_selected=0;
        else{struct pt_arr_clip c;if(arrange_target(&c))arrange_selected=c.id;else say("Empty area: Actions > Insert pattern");}
    }else if(!arrange_menu&&(code==PLUS||code==MINUS))arrange_row+=code==PLUS?1:-1;
    arrange_tick();repaint(screen);return 1;
}
static int arrange_control(unsigned cc,unsigned value,int shifted){
    arrange_tick();if(!atomic_load(&arrangement_active))return 1;
    struct pt_arrangement a;if(!pt_arr_info(&a))return 1;
    int delta=value>64?(int)value-128:value==64?0:(int)value;
    if(arrange_menu){int n=arrange_menu==1?13:arrange_menu==2?(int)pt_count():0;
        if(n){if(cc==DATA_ENCODER)arrange_pick=arrange_clamp(arrange_pick+delta,0,n-1);else if(cc==0x14||cc==K1)arrange_pick=(int)value*(n-1)/127;}
    }else if(cc==DATA_ENCODER){
        if(shifted){int n=delta>0?delta:-delta;if(n>8)n=8;while(n--){if(delta>0&&arrange_span>960)arrange_span/=2;else if(delta<0&&arrange_span<a.length)arrange_span*=2;}if(arrange_span<960)arrange_span=960;}
        else arrange_cursor+=(int64_t)delta*arrange_grid;
    }else if(cc==0x14){if(shifted)arrange_rows=3+(int)value*3/128;else arrange_row=a.tracks?(int)value*((int)a.tracks-1)/127:0;}
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
    struct pt_arrangement a;if(!pt_arr_info(&a))return;
    colour(g,0xff000000u);rect(g,0,0,320,240);
    colour(g,0xffffffffu);text(g,arrange_menu?"Back":"Sequential",0,4,106,25,13);
    text(g,"Actions",107,4,106,25,13);colour(g,0xffffc247u);text(g,arrange_menu==3?"Delete":arrange_menu?"Choose":arrange_selected?"Unpin":"Pin block",214,4,106,25,13);
    text_at(g,a.read_only?"Arrange / read only":"Song / Arrange",6,34,308,23,17,0x21);
    if(arrange_menu==3){colour(g,0xffffffffu);text(g,"Delete selected block?",8,93,304,27,18);text(g,"Pattern stays in the library",8,126,304,22,13);}
    else if(arrange_menu){int n=arrange_menu==1?13:(int)pt_count(),first=arrange_pick>4?arrange_pick-4:0;
        for(int i=first;i<n&&i<first+6;++i){char label[80];if(arrange_menu==1)snprintf(label,sizeof label,"%s",arrange_actions[i]);else{struct pt_entry e;if(!pt_info((unsigned)i,&e))continue;snprintf(label,sizeof label,"%.40s / %lld q",e.name,(long long)(e.length/960));}
            int y=62+(i-first)*21;if(i==arrange_pick){colour(g,0xff303030u);rect(g,6,y,308,21);}colour(g,i==arrange_pick?0xffffc247u:0xffffffffu);text_at(g,label,11,y,298,21,13,0x21);
        }
    }else{
        int h=120/arrange_rows;
        for(int i=0;i<arrange_rows;++i){unsigned index=(unsigned)(arrange_top+i);struct pt_arr_track t;if(!pt_arr_track_at(index,&t))break;int y=76+i*h;
            colour(g,index==(unsigned)arrange_row?0xff252525u:0xff101010u);rect(g,0,y,312,h-1);
            char name[48];snprintf(name,sizeof name,"%s%.7s",t.muted?"M ":t.solo?"S ":"",t.name);colour(g,index==(unsigned)arrange_row?0xffffc247u:0xffffffffu);text_at(g,name,3,y,66,h-1,11,0x21);
            for(unsigned j=0;j<a.clips;++j){struct pt_arr_clip c;if(!pt_arr_clip_at(j,&c)||c.track!=t.id||c.start>=arrange_left+arrange_span||c.start+c.length<=arrange_left)continue;
                int x=c.start<arrange_left?72:arrange_x(c.start),end=c.start+c.length>arrange_left+arrange_span?312:arrange_x(c.start+c.length);if(end<=x)end=x+1;
                colour(g,c.muted||t.muted?0xff333333u:0xff344451u);rect(g,x,y+2,end-x,h-5);
                colour(g,c.id==arrange_selected?0xffffc247u:0xff71c1e8u);rect(g,x,y+2,end-x,2);
                struct pt_entry e;int p=pt_pattern_index(c.pattern);if(end-x>24&&p>=0&&pt_info((unsigned)p,&e)){colour(g,0xffffffffu);text_at(g,e.name,x+2,y+5,end-x-3,h-9,11,0x21);}
            }
        }
        for(int i=0;i<=4;++i){int x=72+60*i;char beat[24];snprintf(beat,sizeof beat,"%lldq",(long long)((arrange_left+arrange_span*i/4)/960+1));colour(g,0xff777777u);text_at(g,beat,x<290?x:283,59,34,14,10,0x21);colour(g,0xff444444u);rect(g,x,75,1,121);}
        if(arrange_cursor>=arrange_left&&arrange_cursor<=arrange_left+arrange_span){colour(g,0xffffc247u);rect(g,arrange_x(arrange_cursor),73,1,124);}
        if(!a.tracks){colour(g,0xffffffffu);text(g,"Actions > Add track",8,108,304,25,16);}
    }
    char status[96];snprintf(status,sizeof status,"q %.2f / Grid %.2f / %u blocks",1.+arrange_cursor/960.,arrange_grid/960.,a.clips);
    struct pt_arr_clip selected_clip;struct pt_entry selected_pattern;
    if(!arrange_menu&&arrange_target(&selected_clip)){
        int index=pt_pattern_index(selected_clip.pattern);
        if(index>=0&&pt_info((unsigned)index,&selected_pattern))snprintf(status,sizeof status,"q %.2f / Block %.2fq / Pattern %.2fq",1.+arrange_cursor/960.,selected_clip.length/960.,selected_pattern.length/960.);
    }
    colour(g,0xffaaaaaau);text_at(g,status,6,199,308,15,11,0x21);
    colour(g,0xffffc247u);text_at(g,toast_until&&now_ms()<toast_until?toast:"DATA time / Fader track / Chop split",6,215,308,19,11,0x21);
}
