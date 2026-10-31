#include "resample_fast_plan.h"
#include <math.h>
#include <string.h>
static int ceil_ratio(__uint128_t numerator,__uint128_t denominator,uint64_t*out){
    if(!denominator)return 0;
    __uint128_t n=numerator/denominator+(numerator%denominator!=0);
    if(n>UINT64_MAX)return 0;
    *out=(uint64_t)n;return 1;
}
int resample_fast_plan_make(uint64_t source_frames,unsigned source_rate,
    uint64_t processed_frames,unsigned processed_rate,unsigned output_rate,
    unsigned bpm_milli,unsigned ppqn,unsigned beats_per_bar,
    uint64_t native_tick_limit,struct resample_fast_plan*out){
    if(!out)return 0;
    memset(out,0,sizeof(*out));
    if(!source_frames||source_rate<8000||source_rate>192000||processed_rate<8000||processed_rate>192000||
       output_rate<8000||output_rate>192000||bpm_milli<1000||bpm_milli>1000000||!ppqn||ppqn>96000||
       !beats_per_bar||beats_per_bar>64||!native_tick_limit)return 0;
    struct resample_fast_plan p={0};uint64_t processed;
    if(!ceil_ratio((__uint128_t)source_frames*output_rate,source_rate,&p.minimum_frames)||
       !ceil_ratio((__uint128_t)processed_frames*output_rate,processed_rate,&processed))return 0;
    if(processed>p.minimum_frames)p.minimum_frames=processed;
    if(!ceil_ratio((__uint128_t)p.minimum_frames*bpm_milli*ppqn,(__uint128_t)output_rate*60000,&p.minimum_ticks))return 0;
    uint64_t bar=(uint64_t)ppqn*beats_per_bar;
    if(!ceil_ratio(p.minimum_ticks,bar,&p.render_bars)||p.render_bars>UINT64_MAX/bar)return 0;
    p.render_ticks=p.render_bars*bar;
    if(p.render_ticks>native_tick_limit||!ceil_ratio((__uint128_t)p.render_ticks*output_rate*60000,(__uint128_t)bpm_milli*ppqn,&p.render_frames))return 0;
    *out=p;return 1;
}
int resample_fast_tail_init(struct resample_fast_tail*t,uint64_t minimum,uint64_t hold,uint64_t budget,float threshold){
    if(!t)return 0;
    memset(t,0,sizeof(*t));t->state=RFT_INVALID;
    if(!minimum||!hold||budget<minimum||hold>budget-minimum||!isfinite(threshold)||threshold<=0||threshold>=1)return 0;
    t->minimum=minimum;t->hold=hold;t->budget=budget;t->threshold=threshold;t->state=RFT_MORE;return 1;
}
unsigned resample_fast_tail_scan(struct resample_fast_tail*t,const float*l,const float*r,uint64_t frames){
    if(!t)return RFT_INVALID;
    if(t->state!=RFT_MORE)return t->state;
    if(!l||!r||frames>t->budget-t->scanned)return t->state=RFT_INVALID;
    for(uint64_t i=0;i<frames;++i){
        if(!isfinite(l[i])||!isfinite(r[i]))return t->state=RFT_INVALID;
        ++t->scanned;
        if(t->scanned>t->minimum){
            if(fabsf(l[i])<=t->threshold&&fabsf(r[i])<=t->threshold)++t->quiet;else t->quiet=0;
            if(t->quiet>=t->hold)return t->state=RFT_QUIET;
        }
    }
    if(t->scanned==t->budget)t->state=RFT_NEEDS_MORE_ROOM;
    return t->state;
}
