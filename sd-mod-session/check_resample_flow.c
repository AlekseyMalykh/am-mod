#include "src/resample_flow.h"
#include "src/resample_view.h"
#include "src/pad_render.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static struct resample_command commands[32];static unsigned read_at,write_at,fail_send,sequence_handoffs,applied,released;
static struct pad_render_job*job;static uint64_t worker_token;static struct resample_spec worker_spec;
static struct resample_flow flow;static unsigned mixing,owned;
static int enqueue(void*user,const struct resample_command*c){
    assert(user==&flow);if(fail_send){--fail_send;return 0;}assert(write_at-read_at<32);commands[write_at++%32]=*c;return 1;
}
static void reply(enum resample_reply kind){
    struct resample_update u={.reply=kind,.token=worker_token,.revision=worker_spec.revision,.rate=48000,.capture_total=16,.rendered=mixing,.velocity_supported=1};
    if(job){struct pad_render_progress p;pad_render_progress(job,&p);u.captured=p.source_done;u.capture_total=p.source_total;u.done=p.done;u.total=p.total;u.peak=p.peak;}
    resample_flow_update(&flow,&u);
}
static void pump(void){
    assert(read_at<write_at);struct resample_command c=commands[read_at++%32];
    if(c.action==RS_ACT_SEQUENCE){assert(!owned&&!job);++sequence_handoffs;return;}
    if(c.action==RS_ACT_RELEASE_STALE){assert(c.token!=worker_token||!owned);return;}
    if(c.action==RS_ACT_PREPARE){assert(!owned&&!job);worker_token=c.token;worker_spec=c.spec;owned=1;reply(RS_REPLY_PREPARED);return;}
    if(c.action==RS_ACT_CANCEL&&c.token==worker_token&&!owned)return; /* Late terminal cleanup. */
    assert(c.token==worker_token&&owned);worker_spec=c.spec;
    if(c.action==RS_ACT_RENDER){
        assert(!job);float pcm[32];for(unsigned i=0;i<32;++i)pcm[i]=.1f*sinf(i*.31f);
        struct pad_render_source s={pcm,16,2,48000};struct pad_render_request r={.first=0,.end=16,.repeats=c.spec.repeats,.tail_frames=(uint64_t)c.spec.tail_ms*48,.chain=c.spec.fx};enum pad_render_status error;
        job=pad_render_create(&s,&r,NULL,&error);assert(job);
        while(pad_render_step(job,7)==PAD_RENDER_WORKING)reply(RS_REPLY_PROGRESS);
        reply(RS_REPLY_RENDERED);
    }else if(c.action==RS_ACT_COMPARE){assert(job&&pad_render_output(job,NULL,NULL));mixing=c.rendered;reply(RS_REPLY_COMPARED);}
    else if(c.action==RS_ACT_CANCEL){mixing=0;pad_render_destroy(job);job=NULL;owned=0;++released;reply(RS_REPLY_CANCELLED);}
    else if(c.action==RS_ACT_APPLY){assert(job&&pad_render_output(job,NULL,NULL));++applied;mixing=0;pad_render_destroy(job);job=NULL;owned=0;reply(RS_REPLY_APPLIED);}
    else assert(c.action==RS_ACT_CHOOSE_FX||c.action==RS_ACT_EDIT_FX);
}
static void button(unsigned k){assert(resample_flow_button(&flow,0x6b+k,1));assert(resample_flow_button(&flow,0x6b+k,0));}
static void open(void){uint64_t occupied[2]={1,0};assert(resample_flow_open(&flow,0,"Test sample","Verse",1,1,occupied));}
static void sample(void){open();button(2);assert(flow.page==RS_PREPARING&&!owned&&!job);pump();assert(flow.page==RS_SAMPLE&&owned&&!job);}
static FILE*draw;
static void rect(void*u,int x,int y,int w,int h,uint32_t colour){assert(u==&flow&&x>=0&&y>=5&&w>=0&&h>=0&&x+w<=320&&y+h<=240);fprintf(draw,"R %d %d %d %d %08x\n",x,y,w,h,colour);}
static void text(void*u,const char*s,int x,int y,int w,int h,unsigned size,uint32_t colour,int centre){assert(u==&flow&&x>=0&&y>=5&&x+w<=320&&y+h<=240&&size>=12);fprintf(draw,"T %d %d %d %d %u %08x %d %s\n",x,y,w,h,size,colour,centre,s);}
static void paint(const char*path){draw=fopen(path,"w");assert(draw);struct resample_painter p={&flow,rect,text};resample_view_paint(&flow,&p);fclose(draw);}
static void paint_suffix(const char*root,const char*suffix){char path[512];snprintf(path,sizeof(path),"%s.%s",root,suffix);paint(path);}
int main(int argc,char**argv){
    assert(argc==2);struct resample_backend backend={&flow,enqueue};resample_flow_init(&flow,&backend);
    assert(resample_flow_shortcut(10,1,0)&&!resample_flow_shortcut(11,1,0)&&!resample_flow_shortcut(10,0,0)&&!resample_flow_shortcut(10,1,1));
    open();paint(argv[1]);assert(!strcmp(resample_flow_button_label(&flow,0),"Cancel")&&!strcmp(resample_flow_button_label(&flow,1),"Seq")&&!strcmp(resample_flow_button_label(&flow,2),"Sample"));
    assert(!resample_flow_button(&flow,0x52,1)&&!resample_flow_button(&flow,0x51,0)&&!resample_flow_knob(&flow,0,1));
    button(1);assert(flow.page==RS_CLOSED&&!owned&&!job);pump();assert(sequence_handoffs==1);
    open();assert(resample_flow_button(&flow,0x6b,1)&&flow.page==RS_CLOSED);assert(resample_flow_button(&flow,0x6b,0));
    sample();paint_suffix(argv[1],"sample");uint64_t first_token=flow.token;button(1);assert(flow.page==RS_FX);paint_suffix(argv[1],"fx");
    struct fx_chain_config chain={.count=1,.stage={{.kind=FX_STAGE_EQ}}};fx_eq_default(&chain.stage[0].eq);chain.stage[0].eq.band[1].gain_db=6;
    assert(resample_flow_set_fx(&flow,&chain));button(2);pump();button(1);assert(flow.spec.fx.stage[0].bypass);button(1);assert(!flow.spec.fx.stage[0].bypass);
    chain.count=FX_CHAIN_MAX+1;assert(!resample_flow_set_fx(&flow,&chain));
    /* Reordering carries every setting and bypass; selection follows the
     * stage. Holding a top key and releasing Shift cannot toggle Bypass. */
    struct fx_chain_config full={0};
    for(unsigned i=0;i<FX_CHAIN_MAX;++i){struct fx_stage s={.kind=FX_STAGE_EQ,.bypass=i==0};fx_eq_default(&s.eq);s.parameter[15]=(float)i;assert(fx_chain_add(&full,&s,48000)==FX_OK);}
    assert(resample_flow_set_fx(&flow,&full));struct fx_stage original=full.stage[0];uint64_t rev=flow.spec.revision;
    assert(!resample_flow_button(&flow,0x31,1));assert(!strcmp(resample_flow_button_label(&flow,1),"Up")&&!strcmp(resample_flow_button_label(&flow,2),"Down"));
    button(1);assert(flow.fx_slot==0&&flow.spec.revision==rev);
    for(unsigned i=1;i<FX_CHAIN_MAX;++i){button(2);assert(flow.fx_slot==i);}
    assert(!memcmp(&flow.spec.fx.stage[4],&original,sizeof(original)));button(2);assert(flow.fx_slot==4);
    assert(resample_flow_button(&flow,0x6c,1));assert(flow.fx_slot==3);rev=flow.spec.revision;
    assert(!resample_flow_button(&flow,0x31,0));assert(resample_flow_button(&flow,0x6c,1));assert(flow.spec.revision==rev);assert(resample_flow_button(&flow,0x6c,0));
    assert(flow.spec.fx.stage[3].bypass);paint_suffix(argv[1],"fx");
    chain.count=1;assert(resample_flow_set_fx(&flow,&chain));resample_flow_knob(&flow,0,-127);assert(!resample_flow_button(&flow,0x31,1));button(1);assert(flow.fx_slot==0);
    resample_flow_knob(&flow,0,127);assert(flow.fx_slot==chain.count);rev=flow.spec.revision;button(2);assert(flow.spec.revision==rev&&!strcmp(flow.message,"Select an effect"));
    assert(!resample_flow_button(&flow,0x31,0));button(0);assert(flow.page==RS_SAMPLE);
    resample_flow_knob(&flow,0,1);resample_flow_knob(&flow,2,-10);assert(flow.spec.repeats==2&&flow.spec.velocity==117);
    button(2);assert(flow.page==RS_RENDERING&&!job);paint_suffix(argv[1],"rendering");pump();assert(flow.page==RS_READY&&job&&!applied&&!mixing);
    char path[512];snprintf(path,sizeof(path),"%s.ready",argv[1]);paint(path);
    button(1);assert(flow.page==RS_SWITCHING&&!flow.compare_rendered);pump();assert(flow.page==RS_READY&&flow.compare_rendered&&mixing);
    assert(resample_flow_button(&flow,0x6d,1));assert(flow.page==RS_CONFIRM&&!applied);unsigned before_confirm=write_at;
    assert(resample_flow_button(&flow,0x6d,1)&&write_at==before_confirm&&!applied); /* Holding cannot confirm overwrite. */
    assert(resample_flow_button(&flow,0x6d,0));paint_suffix(argv[1],"confirm");button(0);assert(flow.page==RS_READY&&!applied);
    resample_flow_knob(&flow,0,1);resample_flow_knob(&flow,1,1);resample_flow_knob(&flow,2,2);assert(resample_flow_destination(&flow)==18);
    button(2);assert(flow.page==RS_APPLYING&&!applied);resample_flow_cancel(&flow);assert(flow.page==RS_CANCELLING);pump();assert(flow.page==RS_CLOSED&&flow.applied&&applied==1&&!job&&!owned&&!mixing);pump();
    struct resample_update stale={.token=first_token,.reply=RS_REPLY_RENDERED};resample_flow_update(&flow,&stale);pump();
    sample();button(2);pump();assert(flow.page==RS_READY);button(0);assert(flow.page==RS_CANCELLING);pump();assert(flow.page==RS_PREPARING);pump();assert(flow.page==RS_SAMPLE&&!job);
    resample_flow_knob(&flow,0,INT_MAX);resample_flow_knob(&flow,1,INT_MIN);assert(flow.spec.repeats==64&&flow.spec.tail_ms==0);
    fail_send=1;resample_flow_cancel(&flow);assert(flow.page==RS_CANCELLING&&flow.cancel_retry&&owned);resample_flow_retry_cancel(&flow);pump();assert(flow.page==RS_CLOSED&&!owned);
    sample();uint64_t cancelled=flow.token;resample_flow_cancel(&flow);
    struct resample_update late={.token=cancelled,.revision=flow.spec.revision,.reply=RS_REPLY_PREPARED,.capture_total=16,.rate=48000};resample_flow_update(&flow,&late);assert(flow.page==RS_CANCELLING);pump();assert(flow.page==RS_CLOSED);
    open();flow.has_sample=0;button(2);assert(flow.page==RS_CHOOSE&&!owned);button(0);
    open();fail_send=1;button(2);assert(flow.page==RS_CHOOSE&&!owned);button(0);
    sample();button(2);
    struct resample_update invalid={.token=flow.token,.revision=flow.spec.revision,.reply=RS_REPLY_RENDERED,.capture_total=16,.captured=16,.done=1,.total=2};
    resample_flow_update(&flow,&invalid);assert(flow.page==RS_ERROR&&applied==1);paint_suffix(argv[1],"error"); /* Model writes still only the earlier Apply. */
    /* Queued Render cannot be forgotten: Cancel cleans it after any reply. */
    resample_flow_cancel(&flow);pump();assert(flow.page==RS_CANCELLING);pump();assert(flow.page==RS_CLOSED&&!job&&!owned);
    assert(read_at==write_at&&released>=4);
    puts("PASS Shift+physical pad11 chooser Cancel/Seq/Sample, native Sequence handoff without sample job; paired button edges, Play/Stop passthrough, prepare/render/apply acknowledgements, real bounded PCM+EQ rendering, max5FX/bypass, Shift+B2/B3 reorder with settings/selection preserved and held-key protection, destination across banks, occupied confirmation, compare acknowledgement, no apply on render, cancellation retry/late replies/stale result cleanup; UI/worker transport and native mix/assignment remain adapters");
}
