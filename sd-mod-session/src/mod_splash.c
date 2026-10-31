#include "mod_splash.h"

/* Pure decoration scene. The boot helper supplies the stock-style white mark.
 * No framebuffer access, pad/MIDI output, storage, audio or startup delays. */
void mod_splash_frame(uint64_t ms,int width,int height,const struct mod_splash_painter *p){
    if(!p||!p->rectangle||width!=320||height!=240||ms<1800)return;
    static const int edges[8][2]={{28,30},{282,198},{282,30},{28,198},{75,17},{236,211},{236,17},{75,211}};
    static const uint32_t colours[8]={0xffffc247,0xff00c8ff,0xff32c780,0xff8830ff,0xffff20b0,0xff00c8ff,0xff32c780,0xffffc247};
    unsigned count=1+(unsigned)((ms-1800)/450);
    if(count>8)count=8;
    for(unsigned n=0;n<count;++n)p->rectangle(p->context,edges[n][0],edges[n][1],9,9,colours[n]);
    if(ms<5800)return;
    unsigned step=(unsigned)(((ms-5800)/180)%16);
    const unsigned pattern=(1u<<0)|(1u<<3)|(1u<<6)|(1u<<10)|(1u<<12);
    for(unsigned n=0;n<16;++n){
        int x=126+(int)(n%4)*18,y=86+(int)(n/4)*18;
        uint32_t colour=(pattern&(1u<<n))?0xff32c780:0xffeeeeee;
        if(n==step)colour=0xff00c8ff;
        p->rectangle(p->context,x,y,14,14,colour);
    }
}
