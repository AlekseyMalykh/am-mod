/* Proposed UX only. Emit native-size painter traces under AArch64/QEMU.
 * Does not install hooks, open/save a Project, allocate Samples or record I/O.
 * Palette/geometry follow mixer_view.c / midi_ui.c / piano_roll_native.inc. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../src/resample_view.h"
#define FG 0xffffffffu
#define BG 0xff000000u
#define DIM 0xff9ba5b0u
#define PANEL 0xff11171du
#define LINE 0xff36424du
#define AMBER 0xffffc247u
#define RED 0xffff4747u
#define BLUE 0xff71c1e8u
#define VIOLET 0xffb5a3ffu
#define GREEN 0xffb6ec9cu
static FILE *output;
static void rect(void*u,int x,int y,int w,int h,uint32_t c){
    (void)u;assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240);
    fprintf(output,"R %d %d %d %d %08x\n",x,y,w,h,c);
}
static void text(void*u,const char*s,int x,int y,int w,int h,unsigned size,uint32_t c,int centre){
    (void)u;assert(s&&x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240&&size>=9);
    fprintf(output,"T %d %d %d %d %u %08x %d %s\n",x,y,w,h,size,c,centre,s);
}
static const struct resample_painter painter={NULL,rect,text};
static void r(int x,int y,int w,int h,uint32_t c){painter.rect(NULL,x,y,w,h,c);}
static void t(const char*s,int x,int y,int w,int h,int size,uint32_t c){painter.text(NULL,s,x,y,w,h,(unsigned)size,c,0);}
static void centre(const char*s,int x,int y,int w,int h,int size,uint32_t c){painter.text(NULL,s,x,y,w,h,(unsigned)size,c,1);}
static void border(int x,int y,int w,int h,uint32_t c){r(x,y,w,1,c);r(x,y+h-1,w,1,c);r(x,y,1,h,c);r(x+w-1,y,1,h,c);}
static void heads(const char*a,const char*b,const char*c){
    const char*names[]={a,b,c};for(int i=0;i<3;++i){int x=i*106,w=i==2?105:102;r(x+2,2,w,26,0xff242424u);centre(names[i],x+3,3,w-2,24,13,FG);}
}
static void knob(int col,const char*name,const char*value,int focus,float amount){
    int x=col*106,w=col==2?108:106;r(x+2,198,w-4,40,focus?0xff1c252eu:PANEL);r(x+2,198,w-4,1,focus?AMBER:LINE);
    centre(name,x+4,200,w-8,12,11,focus?AMBER:DIM);centre(value,x+4,214,w-8,18,16,focus?AMBER:FG);
    r(x+4,235,w-8,2,0xff3b3b3bu);int n=(int)((w-8)*amount);if(n>0)r(x+4,235,n,2,AMBER);
}
static void title(const char*name,const char*right){t(name,7,33,224,22,17,AMBER);t(right,237,35,76,18,11,DIM);}
static void hint(const char*s){t(s,6,181,308,15,10,DIM);}
struct note {int pitch;float start,length;};
static const struct note original[]={{60,0,.7f},{64,1,.7f},{67,2,.7f},{65,3,.7f},{60,4,.7f},{64,5,.7f},{67,6,.7f},{72,7,.7f}};
static struct note variation[8];
static void wave(int x,int y,int w,int h,int seed,uint32_t ink){
    for(int i=0;i<w;i+=3){float a=fabsf(sinf((float)(i+seed)*.237f)*sinf((float)(i+seed)*.081f));
        int n=1+(int)(a*(h/2-1));r(x+i,y+h/2-n,1,n*2,ink);}
}
static void mini_notes(int x,int y,int w,int h,int shift,uint32_t ink){
    const int pitches[]={0,5,2,7};for(int i=0;i<4;++i){int nx=x+i*w/4,nw=w/6;if(nw<1)nw=1;r(nx,y+2+(pitches[i]+shift)%(h-5),nw,2,ink);}
}
static void block(int start,int length,int row,const char*name,int kind,int selected,int seed){
    int x=57+start*16,y=75+row*26,w=length*16-2,h=23;
    r(x,y,w,h,PANEL);r(x,y,w,2,row==0?BLUE:row==1?AMBER:row==2?VIOLET:GREEN);
    if(kind)wave(x+2,y+5,w-4,13,seed,selected?AMBER:DIM);else mini_notes(x+2,y+5,w-4,13,seed,selected?AMBER:DIM);
    /* Label backing is part of the clip, not another container. */
    int labelw=(int)strlen(name)*6+4;if(labelw>w-3)labelw=w-3;
    if(w>=38){r(x+2,y+5,labelw,12,PANEL);t(name,x+3,y+4,labelw-2,13,10,selected?AMBER:FG);}
    if(selected)border(x,y,w,h,AMBER);
}
static void arrange(int frozen,int recording,int empty){
    heads("Sequence","Actions","Edit");title("Song 01  Arrange",recording?"REC":"1-16 bars");
    if(recording)t("R",306,34,10,18,13,RED);
    const char*labels[]={"Drums","Synth","Lead","Double"};
    for(int i=0;i<4;++i){t(labels[i],5,75+i*26,48,23,12,i==1?AMBER:FG);for(int n=0;n<=16;++n)r(56+n*16,73+i*26,1,25,0xff28313au);}
    for(int n=0;n<4;++n){char b[8];snprintf(b,sizeof b,"%d",1+n*4);t(b,57+n*64,57,35,15,11,DIM);}
    for(int i=0;i<8;++i)block(i*2,2,0,"",0,0,i);
    if(empty){border(57,101,62,23,AMBER);t("Empty",61,105,55,15,11,AMBER);}
    else block(0,recording?3:8,1,frozen?"B03  Frozen":recording?"Take 01":"Melody A",frozen,1,3);
    block(0,16,2,"A13  Vocal Lead",1,0,7);block(0,16,3,"A14  Vocal Double",1,0,19);
    if(recording){r(57+3*16,71,1,106,RED);hint("New Take / Auto length / MIDI -> Synth");}
    else hint(frozen?"Frozen: source retained / Edit Source":"MIDI patterns + pad sample triggers");
    knob(0,"Part","Synth",0,.25f);knob(1,"Position","1.1",1,0);knob(2,"Length",empty?"4 bars":recording?"Auto":"8 bars",0,.5f);
}
static void piano(int selecting,int editing,int changed,int inserted,int recording){
    heads(editing?"Cancel":"Song",selecting?"End range":"Select range",editing?"Apply":"Edit");
    title(recording?"Take 01  Pattern":"Melody A  Pattern",recording?"REC":"Tune / Synth");
    const int pitches[]={72,69,67,65,64,62,60};
    for(int i=0;i<7;++i){char name[8];const char*labels[]={"C5","A4","G4","F4","E4","D4","C4"};snprintf(name,sizeof name,"%s",labels[i]);t(name,3,76+i*13,25,13,11,DIM);r(31,88+i*13,281,1,0xff28313au);}
    for(int i=0;i<=8;++i){r(32+i*35,72,1,105,0xff28313au);if(i<8){char b[16];snprintf(b,sizeof b,"%d",i+1);t(b,34+i*35,58,25,13,10,DIM);}}
    for(int i=0;i<8;++i){const struct note*n=changed?variation+i:original+i;int row=6;for(int j=0;j<7;++j)if(n->pitch>=pitches[j]){row=j;break;}
        r(34+(int)(n->start*35),77+row*13,(int)(n->length*35),8,(editing||selecting)&&(i==4||i==6)?AMBER:0xff826528u);}
    if(inserted){r(34+5*35+17,77+5*13,12,8,AMBER);border(34+5*35+17,77+5*13,12,8,FG);}
    if(selecting){border(32+4*35,73,3*35,100,AMBER);r(32+4*35,73,1,100,FG);}
    if(recording){r(32+6*35,71,1,105,RED);t("REC",278,58,35,13,11,RED);}
    hint(selecting?"Hold DATA: time / Fader: rows":recording?"Rec in Pattern / pads or MIDI keys":editing?"Preview / Shift+K2: Velocity":"DATA: cursor / Fader: row / Shift: Zoom");
    knob(0,editing?"Move":"Scroll rows",editing?"+0":"C4-C5",0,.45f);
    knob(1,editing?"Pad / Pitch":"Grid",editing?(changed?"+2":"0"):"1/16",editing,.45f);
    knob(2,editing?"Length":"Scroll time",editing?"+0":"1.1",0,.2f);
}
static void list_row(int y,const char*a,const char*b,int selected){
    if(selected){r(5,y,310,24,0xff302412u);}
    t(a,10,y+1,167,22,13,selected?AMBER:FG);t(b,183,y+1,127,22,13,selected?AMBER:FG);
}
static void record_options(int mode){
    heads("Cancel","Mode","Arm");title("Record in Arrange","Synth");
    list_row(65,"Destination",mode==1?"This Pattern":"New Take",0);
    list_row(92,"Length",mode==0?"Auto":mode==1?"Loop: 4 bars":"Punch: 5-9",1);
    list_row(119,"Events",mode==1?"Overdub":"Add",0);
    list_row(146,"Automation","Off / Auto Arm",0);
    hint(mode==0?"No block required / Stop ends the take":mode==1?"Fixed loop / Rec + Play / Layer notes":"Only the selected range is recorded");
    knob(0,"Start",mode==2?"5.1":"1.1",0,0);knob(1,"Record mode",mode==0?"Auto":mode==1?"Loop":"Punch",1,.5f);knob(2,"Instrument","Synth",0,.6f);
}
static void render_options(void){
    heads("Cancel","FX: Dry","Render");title("Render to Pad","Review");
    list_row(65,"Source","Melody A",0);list_row(92,"Range","1-8 bars",0);list_row(119,"Tail","Include",0);list_row(146,"Target","B03 / Empty",1);
    hint("Create audio copy / Keep source notes");knob(0,"Scope","Pattern",0,.3f);knob(1,"FX","Dry",0,0);knob(2,"Destination","B03",1,.2f);
}
static void sample(void){
    heads("Song","Trim","Chop");title("A13  Vocal Lead","Sample");r(8,70,304,85,0xff080d12u);wave(10,73,298,79,7,DIM);r(19,69,1,87,AMBER);r(298,69,1,87,AMBER);
    t("Start",12,158,75,18,12,AMBER);t("End",260,158,48,18,12,AMBER);hint("Pad sample / Song stores its trigger event");knob(0,"Start","0",1,.05f);knob(1,"End","Full",0,.95f);knob(2,"Play mode","One Shot",0,.7f);
}
static void empty(void){
    heads("Cancel","","Create");title("Empty Pattern","Synth");list_row(66,"Name","Melody B",0);list_row(95,"Length","4 bars",1);list_row(124,"Content","No events",0);hint("Record live or insert events in Piano Roll");knob(0,"Part","Synth",0,.3f);knob(1,"Length","4 bars",1,.4f);knob(2,"Place at","9.1",0,.5f);
}
static void insert(void){
    heads("Back","","Choose");title("Piano Roll Actions","Pattern");list_row(62,"Insert note","At cursor",1);list_row(88,"Copy / Paste","Selection",0);list_row(114,"Quantize","Selection",0);list_row(140,"Automation","Open lane",0);hint("DATA: action / B3: choose");knob(0,"Time","5.3",0,.5f);knob(1,"Pitch","D4",1,.4f);knob(2,"Length","1/8",0,.2f);
}
static void auto_lane(void){
    heads("Pattern","Touch","Edit");title("Automation","Synth");t("Cutoff / Pattern lane",8,59,300,17,12,DIM);r(8,82,304,85,0xff080d12u);
    for(int x=10;x<308;++x){int y=150-(x-10)/5;r(x,y,1,2,AMBER);}r(108,126,5,5,FG);hint("Record applied values / Shared transport");knob(0,"Time","3.1",0,.25f);knob(1,"Cutoff","70%",1,.7f);knob(2,"Curve","Linear",0,.5f);
}
static const char*names[]={"mixed-arrange","pattern","selection","edit-preview","edited-pattern","record-auto","record-loop","record-punch","record-arrange","record-pattern","insert-menu","inserted-note","render-review","frozen-arrange","vocal-pad","empty-pattern","automation"};
int main(int argc,char**argv){
    assert(argc==2);memcpy(variation,original,sizeof original);variation[4].pitch=62;variation[6].pitch=69;
    assert(original[4].pitch==60&&original[6].pitch==67); /* Source retained. */
    for(unsigned i=0;i<sizeof names/sizeof *names;++i){char path[2048];assert(snprintf(path,sizeof path,"%s/%s.trace",argv[1],names[i])<(int)sizeof path);output=fopen(path,"w");assert(output);r(0,0,320,240,BG);
        switch(i){case 0:arrange(0,0,0);break;case 1:piano(0,0,0,0,0);break;case 2:piano(1,0,0,0,0);break;case 3:piano(0,1,1,0,0);break;case 4:piano(0,0,1,0,0);break;
        case 5:record_options(0);break;case 6:record_options(1);break;case 7:record_options(2);break;case 8:arrange(0,1,0);break;case 9:piano(0,0,1,0,1);break;case 10:insert();break;case 11:piano(0,0,1,1,0);break;case 12:render_options();break;case 13:arrange(1,0,0);break;case 14:sample();break;case 15:empty();break;case 16:auto_lane();break;}
        assert(!fclose(output));
    }
    puts("PASS proposed Song workflow painter: 17 bounded 320x240 scenes; no application hooks or I/O recording");return 0;
}
