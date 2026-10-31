#ifndef MPC_PREVIEW_GAIN_H
#define MPC_PREVIEW_GAIN_H
#include <math.h>
/* Native preview curves:0 = -60..0dB,1 = linear,2 = squared. Zero mutes.
 * Main is applied by the native output mixer, after this instrument. Never
 * multiply it a second time here; only gate silence/invalid Main and add K3. */
static inline float preview_normalized_gain(float main_gain,unsigned level,unsigned curve){
 if(!isfinite(main_gain)||main_gain<=0.f||main_gain>1.001f)return 0.f;
 float gain=(float)(level>100?100:level)/100.f;
 if(curve==1)return gain;
 if(curve==2)return sqrtf(gain);
 /* Native bbebe8 sets curve3 scale to +6dB: 0x3fff64c2. */
 if(curve==3)return sqrtf(gain/1.9952623844f);
 if(curve!=0)return 0.f;
 return gain>0.001f?fmaxf(0.f,fminf(1.f,1.f+log10f(gain)/3.f)):0.f;
}
#endif
