#include "resample_destination.h"
#include <stdio.h>
#include <string.h>
static void copy(char*out,size_t capacity,const char*text){
    if(!text)text="";
    size_t n=strlen(text);if(n>=capacity){n=capacity-1;while(n&&((unsigned char)text[n]&0xc0)==0x80)--n;}
    memcpy(out,text,n);out[n]=0;
}
static int occupied(const struct resample_destination*d){return (int)((d->occupied[d->target_pad/64]>>(d->target_pad%64))&1);}
void resample_destination_init(struct resample_destination*d,const struct resample_destination_backend*b){
    memset(d,0,sizeof(*d));if(b)d->backend=*b;
}
int resample_destination_open(struct resample_destination*d,unsigned source,const char*sample,
    const char*sequence,int has_sample,int has_sequence,uint64_t epoch,const uint64_t used[2]){
    if(!d||d->page!=RD_CLOSED||source>=128||!epoch)return 0;
    d->page=RD_SOURCE;d->source_pad=source;d->source_epoch=epoch;d->bank=source/16;d->target_pad=128;
    d->kind=0;d->token=d->done=d->total=0;d->applied=d->cancel_retry=0;
    d->has_sample=!!has_sample;d->has_sequence=!!has_sequence;
    d->occupied[0]=used?used[0]:0;d->occupied[1]=used?used[1]:0;
    if(has_sample)d->occupied[source/64]|=UINT64_C(1)<<(source%64);
    copy(d->sample,sizeof(d->sample),sample);copy(d->sequence,sizeof(d->sequence),sequence);d->error[0]=0;
    return 1;
}
static void start(struct resample_destination*d,int overwrite){
    struct resample_destination_request request={++d->serial,d->source_epoch,d->kind,d->source_pad,d->target_pad,(unsigned)overwrite};
    if(!request.token)request.token=++d->serial;
    if(!d->backend.start||!d->backend.start(d->backend.user,&request)){
        copy(d->error,sizeof(d->error),"Cannot start resampling");return;
    }
    d->token=request.token;d->page=RD_RUNNING;d->done=d->total=0;d->error[0]=0;
}
int resample_destination_target(struct resample_destination*d,unsigned target){
    if(!d||d->page!=RD_TARGET||target>=128)return 0;
    d->target_pad=target;d->bank=target/16;d->error[0]=0;return 1;
}
int resample_destination_bank(struct resample_destination*d,unsigned bank){
    if(!d||d->page!=RD_TARGET||bank>=8)return 0;
    if(bank==d->bank)return 1;
    d->bank=bank;d->target_pad=128;d->error[0]=0;return 1;
}
int resample_destination_pad(struct resample_destination*d,unsigned pad,int down){
    if(!d||d->page==RD_CLOSED||pad>=16)return 0;
    if(down&&d->page==RD_TARGET)resample_destination_target(d,d->bank*16+pad);
    return 1; /* Select silently; release cannot trigger another mode. */
}
int resample_destination_button(struct resample_destination*d,unsigned button,int down){
    if(!d||button>=3)return 0;
    if(!down){int own=d->held[button];d->held[button]=0;return own;}
    if(d->held[button])return 1;
    if(d->page==RD_CLOSED)return 0;
    d->held[button]=1;
    if(d->page==RD_SOURCE){
        if(!button)d->page=RD_CLOSED;
        else if((button==1&&d->has_sequence)||(button==2&&d->has_sample)){
            d->kind=button==1?RESAMPLE_SEQUENCE:RESAMPLE_SOUND;d->page=RD_TARGET;d->target_pad=128;d->error[0]=0;
        }else copy(d->error,sizeof(d->error),button==1?"No current sequence":"Selected pad is empty");
    }else if(d->page==RD_TARGET){
        if(!button)d->page=RD_CLOSED;
        else if(button==1){d->page=RD_SOURCE;d->error[0]=0;}
        else if(d->target_pad>=128)copy(d->error,sizeof(d->error),"Choose a destination pad");
        else if(occupied(d)){d->page=RD_CONFIRM;d->error[0]=0;}
        else start(d,0);
    }else if(d->page==RD_CONFIRM){
        if(!button){d->page=RD_TARGET;d->error[0]=0;}
        else if(button==2)start(d,1);
    }else if(!button)resample_destination_cancel(d);
    return 1;
}
void resample_destination_retry_cancel(struct resample_destination*d){
    if(d&&d->page==RD_CANCELLING&&d->cancel_retry)
        d->cancel_retry=!d->backend.cancel||!d->backend.cancel(d->backend.user,d->token);
}
void resample_destination_cancel(struct resample_destination*d){
    if(!d||d->page==RD_CLOSED)return;
    if(d->page==RD_RUNNING||d->page==RD_CANCELLING){
        if(d->page!=RD_CANCELLING){d->page=RD_CANCELLING;d->cancel_retry=1;resample_destination_retry_cancel(d);}
    }else d->page=RD_CLOSED;
}
void resample_destination_progress(struct resample_destination*d,uint64_t token,uint64_t done,uint64_t total){
    if(!d||token!=d->token||d->page!=RD_RUNNING||!total||done>total||done<d->done||(d->total&&d->total!=total))return;
    d->done=done;d->total=total;
}
void resample_destination_finished(struct resample_destination*d,uint64_t token,int applied,const char*error){
    if(!d||token!=d->token||(d->page!=RD_RUNNING&&d->page!=RD_CANCELLING))return;
    if(applied){d->applied=1;d->page=RD_CLOSED;}
    else if(error&&*error){d->page=RD_ERROR;copy(d->error,sizeof(d->error),error);}
    else if(d->page==RD_CANCELLING)d->page=RD_CLOSED;
    else{d->page=RD_ERROR;copy(d->error,sizeof(d->error),"Resampling did not finish");}
    d->cancel_retry=0;
}
void resample_destination_paint(const struct resample_destination*d,const struct resample_painter*p){
    if(!d||!p||!p->rect||!p->text||d->page==RD_CLOSED)return;
    const uint32_t white=0xffffffffu,grey=0xffaaaaaau,amber=0xffffc247u,red=0xffff4040u;
    p->rect(p->user,0,5,320,235,0xff000000u);
    const char*b2="",*b3="";
    if(d->page==RD_SOURCE){b2="Seq";b3="Sample";}
    else if(d->page==RD_TARGET){b2="Source";b3="Render";}
    else if(d->page==RD_CONFIRM)b3="Replace";
    /* Header blocks, as in Chords. */
    const char*heads[3]={d->page==RD_CONFIRM?"Back":"Cancel",b2,b3};
    for(int i=0;i<3;++i)if(*heads[i]){
        uint32_t ink=white;
        if(d->page==RD_SOURCE&&((i==1&&!d->has_sequence)||(i==2&&!d->has_sample)))ink=grey;
        if(i==2&&d->page==RD_TARGET)ink=d->target_pad<128?amber:grey;
        if(i==2&&d->page==RD_CONFIRM)ink=red;
        p->rect(p->user,i*107+1,6,105,26,0xff202020u);
        p->text(p->user,heads[i],i*107+1,7,105,24,14,ink,1);
    }
    p->text(p->user,d->page==RD_SOURCE?"Resample":d->kind==RESAMPLE_SEQUENCE?"Resample Seq":"Resample Sample",8,40,304,28,20,white,d->page==RD_SOURCE);
    char text[112];
    if(d->page==RD_SOURCE){
        /* Two choice blocks under their buttons: B2 the sequence, B3 the pad. */
        const int bx[2]={8,166};
        for(int i=0;i<2;++i){p->rect(p->user,bx[i],80,146,96,0xff1c1c1cu);
            uint32_t edge=i==0?white:amber;
            p->rect(p->user,bx[i],80,146,2,edge);p->rect(p->user,bx[i],174,146,2,edge);
            p->rect(p->user,bx[i],80,2,96,edge);p->rect(p->user,bx[i]+144,80,2,96,edge);}
        p->text(p->user,"B2  Seq",8,86,146,20,14,grey,1);
        p->text(p->user,d->has_sequence?d->sequence:"No sequence",12,108,138,28,18,d->has_sequence?white:grey,1);
        p->text(p->user,"whole sequence",12,146,138,20,12,grey,1);
        p->text(p->user,"B3  Sample",166,86,146,20,14,grey,1);
        snprintf(text,sizeof(text),"%c%02u",'A'+d->source_pad/16,d->source_pad%16+1);
        p->text(p->user,text,170,106,138,26,22,amber,1);
        p->text(p->user,d->has_sample?d->sample:"Empty",170,134,138,20,13,d->has_sample?white:grey,1);
        p->text(p->user,"with its processing",170,154,138,18,11,grey,1);
    }else if(d->page==RD_TARGET||d->page==RD_CONFIRM){
        /* Source block on the left, the destination bank as a 4x4 grid laid
         * out like the pads: recorded pads dim, source outlined white,
         * free destination green, replacement red. */
        p->rect(p->user,8,72,104,104,0xff1c1c1cu);
        p->rect(p->user,8,72,104,2,amber);p->rect(p->user,8,174,104,2,amber);p->rect(p->user,8,72,2,104,amber);p->rect(p->user,110,72,2,104,amber);
        p->text(p->user,"Source",8,78,104,18,12,grey,1);
        if(d->kind==RESAMPLE_SEQUENCE){
            p->text(p->user,"Seq",8,100,104,26,20,amber,1);
            p->text(p->user,d->sequence,12,132,96,36,12,white,1);
        }else{
            snprintf(text,sizeof(text),"%c%02u",'A'+d->source_pad/16,d->source_pad%16+1);
            p->text(p->user,text,8,100,104,26,22,amber,1);
            p->text(p->user,d->sample,12,132,96,36,12,white,1);
        }
        p->text(p->user,">",112,110,16,26,20,white,1);
        snprintf(text,sizeof(text),"Bank %c",'A'+d->bank);
        p->text(p->user,text,130,62,182,14,12,grey,1);
        for(int row=0;row<4;++row)for(int col=0;col<4;++col){
            unsigned i=(unsigned)((3-row)*4+col),pad=d->bank*16+i;
            int x=130+col*46,y=78+row*25,w=43,h=22;
            int used=(int)((d->occupied[pad/64]>>(pad%64))&1),chosen=pad==d->target_pad;
            uint32_t fill=chosen?(used?red:0xff32c780u):used?0xff3a3a3au:0xff1c1c1cu;
            p->rect(p->user,x,y,w,h,fill);
            if(d->kind==RESAMPLE_SOUND&&pad==d->source_pad){
                p->rect(p->user,x,y,w,2,white);p->rect(p->user,x,y+h-2,w,2,white);
                p->rect(p->user,x,y,2,h,white);p->rect(p->user,x+w-2,y,2,h,white);}
            snprintf(text,sizeof(text),"%u",i+1);
            p->text(p->user,text,x,y+1,w,h-2,13,chosen?0xff000000u:used?white:grey,1);
        }
        if(d->target_pad<128)snprintf(text,sizeof(text),"To %c%02u: %s",'A'+d->target_pad/16,d->target_pad%16+1,
            d->page==RD_CONFIRM?"press again to replace":occupied(d)?"occupied, confirm to replace":"empty pad");
        else snprintf(text,sizeof(text),"Press pad / Turn DATA: select");
        p->text(p->user,text,8,182,304,20,14,d->page==RD_CONFIRM||(d->target_pad<128&&occupied(d))?red:d->target_pad<128?white:grey,1);
        if(d->page==RD_TARGET&&!d->error[0])
            p->text(p->user,"K1 or PAD BANK: bank   /   Turn DATA: pad",8,210,304,19,12,grey,1);
    }else{
        if(d->kind==RESAMPLE_SEQUENCE)snprintf(text,sizeof(text),"Seq: %s",d->sequence);
        else snprintf(text,sizeof(text),"%c%02u  %s",'A'+d->source_pad/16,d->source_pad%16+1,d->sample);
        p->text(p->user,text,8,86,304,26,18,amber,0);
        {
            p->text(p->user,d->page==RD_RUNNING?"Resampling...":d->page==RD_CANCELLING?"Cancelling...":"Resampling failed",8,128,304,30,22,d->page==RD_ERROR?red:white,0);
            if(d->page==RD_RUNNING){unsigned fill=d->total?(unsigned)((double)d->done/d->total*304):0;
                p->rect(p->user,8,179,304,5,0xff333333u);if(fill)p->rect(p->user,8,179,(int)fill,5,amber);}
        }
    }
    if(d->error[0])p->text(p->user,d->error,8,214,304,21,14,red,0);
}
