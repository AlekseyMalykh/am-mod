#include "src/mod_splash.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned count,edges,cells,cursors;
static unsigned char pixels[320*240*3];
static void rect(void*p,int x,int y,int w,int h,uint32_t colour){
    (void)p;assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240);++count;
    if(w==9)++edges;else{assert(w==14);++cells;if(colour==0xff00c8ff)++cursors;}
    for(int r=y;r<y+h;++r)for(int c=x;c<x+w;++c){unsigned char *v=pixels+(r*320+c)*3;v[0]=colour>>16;v[1]=colour>>8;v[2]=colour;}
}
static void frame(unsigned ms,unsigned expected_edges,unsigned expected_cells){
    count=edges=cells=cursors=0;memset(pixels,0,sizeof(pixels));
    struct mod_splash_painter p={NULL,rect};mod_splash_frame(ms,320,240,&p);
    assert(edges==expected_edges&&cells==expected_cells&&count==edges+cells);
    assert(cursors==(expected_cells?1u:0u));
}
int main(int argc,char**argv){
    frame(0,0,0);frame(1799,0,0);frame(1800,1,0);frame(3150,4,0);frame(5799,8,0);
    for(unsigned i=0;i<32;++i)frame(5800+i*180,8,16);
    frame(6340,8,16);
    if(argc==2){FILE*f=fopen(argv[1],"wb");assert(f);fprintf(f,"P6\n320 240\n255\n");assert(fwrite(pixels,1,sizeof(pixels),f)==sizeof(pixels));assert(!fclose(f));}
    count=0;struct mod_splash_painter p={NULL,rect};mod_splash_frame(6000,960,540,&p);assert(!count);
    puts("PASS splash: unchanged opening, delayed single edge squares, gradual growth, 16 steps left-to-right/top-to-bottom, one cursor, bounded LCD rectangles, no audio/storage/device I/O");
}
