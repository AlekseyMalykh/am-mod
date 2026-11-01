/* Execute the production ARM footer painter between real native takeover
 * calls. Graphics, formatted text and parameter storage are fixtures; this
 * checks renderer/timer state interference, not LCD speed or hardware ADC. */
#define main scaled_math_main
#include "check_scaled_native.c"
#undef main
static unsigned paints,timer_stops;
static unsigned char screen_fixture[0x100];
static double display_value(void*p){(void)p;return parameter;}
static double proportion(void*p,double v){(void)p;return v;}
static uint32_t ink(void*p,unsigned id,int inherited){(void)p;(void)id;(void)inherited;return 0xffffffff;}
static void rect(void*p,int x,int y,int w,int h){(void)p;(void)x;(void)y;(void)w;(void)h;++paints;}
static void colour(void*p,unsigned c){(void)p;(void)c;}
static void font(void*p,int flags,float size){(void)p;(void)flags;(void)size;}
static void graphics_font(void*p,void*f){(void)p;(void)f;}
static void text(void*p,void*s,int x,int y,int w,int h,int a,int lines,float scale){
 (void)p;(void)s;(void)x;(void)y;(void)w;(void)h;(void)a;(void)lines;(void)scale;
}
static float text_width(void*f,void*s){(void)f;(void)s;return 16;}
static int no_image(void*p){(void)p;return 0;}
static int no_footer(void*p,void*g){(void)p;(void)g;return 0;}
static void string_copy(const char**out,const char*const*in){*out=*in;}
static void timer_stop(void*p){(void)p;++timer_stops;}
extern void display_format(void);
extern void display_entry(void*,void*,void*);
/* Non-trivial native String return uses x8; painter is entered after the
 * original PACIASP, exactly as at the production native paint hook. */
__asm__(".text\n.global display_format\ndisplay_format:\n bti c\n"
        "adr x9,1f\n str x9,[x8]\n ret\n1: .asciz \"56\"\n.balign 4\n"
        ".global display_entry\ndisplay_entry:\n bti c\n mov x7,x0\n paciasp\n br x2\n");
static void display_setup(int mode){
 setup(mode);
 FIELD(screen_fixture,0,uintptr_t)=base+0x6e212a0;
 FIELD(slider_fixture,0,uintptr_t)=base+0x706e5d0;
 FIELD(slider_fixture,8,const char*)="Warp";
 FIELD(slider_fixture,0x18,void*)=screen_fixture;
 FIELD(slider_fixture,0x28,int)=106;FIELD(slider_fixture,0x2c,int)=38;
 FIELD((void*)base,0x723cde0,void*)=slider_fixture+0x1d8;
}
static void draw_many(unsigned n){
 unsigned char binding_before[sizeof(binding)],slider_before[sizeof(slider_fixture)];
 memcpy(binding_before,binding,sizeof(binding));memcpy(slider_before,slider_fixture,sizeof(slider_fixture));
 float before=parameter;unsigned before_writes=writes;
 uintptr_t before_focus=FIELD((void*)base,0x723cde0,uintptr_t);
 for(unsigned i=0;i<n;++i)display_entry(slider_fixture,NULL,(void*)(base+0x6d3b800));
 assert(!memcmp(binding,binding_before,sizeof(binding)));
 assert(!memcmp(slider_fixture,slider_before,sizeof(slider_fixture)));
 assert(parameter==before&&writes==before_writes);
 assert(FIELD((void*)base,0x723cde0,uintptr_t)==before_focus);
}
int main(int argc,char**argv){
 assert(argc==3);assert(scaled_math_main(2,argv)==0);
 FILE*f=fopen(argv[2],"rb");assert(f);unsigned char code[4096];size_t n=fread(code,1,sizeof(code),f);assert(n>0&&n<sizeof(code));fclose(f);
 memcpy((void*)(base+0x6d3b800),code,n);
 stub(0x11234b0,display_value);stub(0x1131fa0,ink);stub(0x102d5e0,colour);
 stub(0x1082810,font);stub(0x1070d60,noop_fixture);stub(0x102dca0,graphics_font);
 stub(0x10817b0,text);stub(0x107c6f0,text_width);stub(0x102e000,rect);
 stub(0x1033520,no_image);stub(0x1033540,no_image);stub(0xf1c640,string_copy);
 stub(0xf0a290,noop_fixture);stub(0x6d3c800,no_footer);
 stub(0xff1bc0,timer_stop);stub(0x1149d70,noop_fixture);
 FIELD((void*)base,0x706e5d0+0x180,void*)=display_format;
 FIELD((void*)base,0x706e5d0+0x190,void*)=proportion;
 __builtin___clear_cache((char*)base,(char*)base+0x7500000);
 const unsigned paths[][8]={{20,60,100,127,90,40,0,0},{100,60,20,0,40,90,127,127},{20,100,40,110,10,120,40,0}};
 for(int mode=0;mode<3;++mode)for(unsigned path=0;path<3;++path){
  float reference[8],previous[8],scaled[8];unsigned changed[8];
  display_setup(mode);
  for(unsigned i=0;i<8;++i){movement(paths[path][i]);reference[i]=parameter;changed[i]=writes;previous[i]=FIELD(binding,0x144,float);scaled[i]=FIELD(binding,0x148,float);}
  for(unsigned drawings=1;drawings<=100;drawings*=10){
   display_setup(mode);
   for(unsigned i=0;i<8;++i){
    draw_many(drawings);movement(paths[path][i]);draw_many(drawings);
    /* Native display timeout changes only its visibility flag and timer. */
    FIELD(slider_fixture,0x230,unsigned char)=1;
    FN(0x3b7d2f0,void(*)(void*))(slider_fixture+0x1f8);
    assert(FIELD(slider_fixture,0x230,unsigned char)==0);
    assert(parameter==reference[i]&&writes==changed[i]);
    assert(FIELD(binding,0x144,float)==previous[i]&&FIELD(binding,0x148,float)==scaled[i]);
   }
  }
 }
 assert(paints>10000&&timer_stops==216);
 puts("PASS production permanent K footer: drawing before/after every native movement leaves parameter, Scaled history and focus unchanged; real display timeout also unchanged; Pickup/Scaled/Jump, reversals and both limits (Graphics/value formatting are fixtures)");
 return 0;
}
