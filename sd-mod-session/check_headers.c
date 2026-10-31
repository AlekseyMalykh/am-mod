#define _GNU_SOURCE
/* GUI boundary fixture: confirmation buttons must retain their native paint.
 * No firmware model or button binding is executed by this layout regression. */
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "src/music_ui.c"
static unsigned paints;static uint32_t last_ink;static int last_x,last_y,last_w,last_h;
static char last_caption[96];
static unsigned red_exit,confirmation;
int note_native_current(char*name,size_t size){(void)name;(void)size;return 0;}
int note_native_tick(void){return 0;}
int music_archive_status(void){return 0;}
static int not_blocked(void*p){(void)p;return 0;}
static int last_row(void*p,int fallback){(void)p;(void)fallback;return 5;}
static void stub(uintptr_t off,void *fn)
{
    uint32_t op[]={0x58000050,0xd61f0200};
    memcpy((void*)(base+off),op,8);memcpy((void*)(base+off+8),&fn,8);
}
static int enabled(void *p) {(void)p;return 1;}
static void ink(void *p,unsigned c) {(void)p;last_ink=c;}
static void fill(void *g,int x,int y,int w,int h)
{(void)g;last_x=x;last_y=y;last_w=w;last_h=h;++paints;}
static void string_make(uint64_t *s,const char *v) {*s=(uintptr_t)v;}
static void discard(uint64_t *s) {(void)s;}
static void font_make(uint64_t *s,int style,float h) {(void)style;(void)h;*s=0;}
static void font_use(void *g,const uint64_t *s) {(void)g;(void)s;}
static void caption(void *g,const uint64_t *s,int x,int y,int w,int h,int flags,int lines,float scale)
{
    (void)g;(void)x;(void)y;(void)w;(void)h;(void)flags;(void)lines;(void)scale;
    snprintf(last_caption,sizeof(last_caption),"%s",(const char*)(uintptr_t)*s);
    if(!strcmp(last_caption,"Mod Off")&&last_ink==0xffff4040)++red_exit;
    if(!strcmp(last_caption,"Exit the mod?"))++confirmation;
}
int main(void)
{
    void *memory=mmap(NULL,0x1200000,7,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(memory!=MAP_FAILED);base=(uintptr_t)memory;
    stub(0x10f98d0,enabled);stub(0x102d5e0,ink);stub(0x102e000,fill);
    stub(0xf535c0,string_make);stub(0xf0a290,discard);stub(0x1082810,font_make);
    stub(0x102dca0,font_use);stub(0x1070d60,discard);stub(0x10817b0,caption);
    __builtin___clear_cache(memory,(char*)memory+0x1200000);
    _Alignas(8) unsigned char question[0x100]={0},button[0x230]={0},sample_root[0x100]={0};
    FIELD(question,0,uintptr_t)=base+0x6e1fe48;FIELD(sample_root,0,uintptr_t)=base+SAMPLE_VPTR;
    FIELD(button,0,uintptr_t)=base+0x706e940;FIELD(button,0x18,void*)=question;
    FIELD(button,0x28,int)=106;FIELD(button,0x2c,int)=29;FIELD(button,0xb8,const char*)="Answer1";
    FIELD(button,0x21c,int)=0;music_ui_tab(button,(void*)1);assert(!paints);
    FIELD(button,0xb8,const char*)="Answer2";music_ui_tab(button,(void*)1);assert(!paints);
    FIELD(button,0x18,void*)=sample_root;FIELD(button,0xb8,const char*)="Trim";
    FIELD(button,0x21c,int)=2;FIELD(button,0x214,int)=3;FIELD(button,0x218,int)=0;
    music_ui_tab(button,(void*)1);assert(paints==4 && !strcmp(last_caption,"Trim"));
    FIELD(sample_root,0,uintptr_t)=base+0x6e1e258;knob_underlay(sample_root,(void*)1);
    assert(paints==5&&last_ink==0xffffc247&&last_x==0&&last_y==198&&last_w==320&&last_h==42);
    stub(0x112fd50,enabled);stub(0x112f6b0,not_blocked);stub(0x11227b0,last_row);stub(0x1149880,enabled);
    __builtin___clear_cache(memory,(char*)memory+0x1200000);
    unsigned char project_root[0x600]={0},list[0x300]={0};FIELD(project_root,0,uintptr_t)=base+PROJECT_VPTR;FIELD(project_root,0x4c8,void*)=list;
    FIELD(list,0x18,void*)=project_root;FIELD(list,0x24,int)=40;FIELD(list,0x28,int)=320;FIELD(list,0x2c,int)=120;
    unsigned before_menu=paints;music_ui_project(project_root,(void*)1);assert(red_exit==0&&paints==before_menu);
    assert(!mkdir("/run/mpc-trial-app",0700));
    music_ui_request_project((void*)((uintptr_t)project_root|1));music_ui_tick();assert(key_page==2);music_ui_project(project_root,(void*)1);assert(confirmation==1&&red_exit==1);
    unsigned char event[16]={0};event[8]=0x0c;event[9]=1;assert(music_ui_button(event)&&!key_page);event[9]=0;music_ui_button(event);assert(access("/run/mpc-trial-app/mod-off.request",F_OK)!=0);
    music_ui_request_project((void*)((uintptr_t)project_root|1));music_ui_tick();event[8]=0x52;event[9]=1;assert(!music_ui_button(event)&&key_page==2);
    event[8]=0x0e;assert(music_ui_button(event));event[9]=0;music_ui_button(event);
    FILE*request_file=fopen("/run/mpc-trial-app/mod-off.request","rb");assert(request_file&&fgetc(request_file)=='1'&&fgetc(request_file)==EOF);fclose(request_file);struct stat st;assert(!stat("/run/mpc-trial-app/mod-off.request",&st)&&(st.st_mode&0777)==0600);assert(!unlink("/run/mpc-trial-app/mod-off.request"));
    FILE*sentinel=fopen("/run/mpc-trial-app/sentinel","wb");assert(sentinel);fputs("UNCHANGED",sentinel);fclose(sentinel);assert(!symlink("/run/mpc-trial-app/sentinel","/run/mpc-trial-app/mod-off.request"));event[9]=1;music_ui_button(event);event[9]=0;music_ui_button(event);sentinel=fopen("/run/mpc-trial-app/sentinel","rb");assert(sentinel&&fgetc(sentinel)=='U');fclose(sentinel);assert(!unlink("/run/mpc-trial-app/mod-off.request")&&!unlink("/run/mpc-trial-app/sentinel")&&!rmdir("/run/mpc-trial-app"));
    munmap(memory,0x1200000);puts("PASS native QuestionScreen captions left intact; compact tabs still painted");
}
