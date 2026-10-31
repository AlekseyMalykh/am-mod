#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <dlfcn.h>
#include <elf.h>
#include <signal.h>
#include <stdlib.h>
#include <ucontext.h>
#include <sys/stat.h>
static const char*test_path(const char*);
#define WORKFLOW_STORAGE_META test_path("/storage-meta")
#define WORKFLOW_INTERNAL_ROOT test_path("/Internal")
#include "src/workflow_ui.c"
static uintptr_t guard=0x123456;
static char test_root[]="/tmp/mpc-workflow-check-XXXXXX";
static const char*test_path(const char*suffix){static char buffers[16][1024];static unsigned next;char*out=buffers[next++%16];int n=snprintf(out,1024,"%s%s",test_root,suffix);assert(n>=0&&n<1024);return out;}

static void stub(uintptr_t off,void*fn){uint32_t w[]={0x58000050,0xd61f0200};memcpy((void*)(base+off),w,8);memcpy((void*)(base+off+8),&fn,8);}
static void fault_signal(int sig,siginfo_t*i,void*ctx){ucontext_t*u=ctx;fprintf(stderr,"FAIL workflow %d native=%lx addr=%p lr=%lx x20=%lx x21=%lx caller=%lx saved21=%lx\n",sig,(unsigned long)(u->uc_mcontext.pc-base),i->si_addr,(unsigned long)(u->uc_mcontext.regs[30]-base),(unsigned long)u->uc_mcontext.regs[20],(unsigned long)u->uc_mcontext.regs[21],(unsigned long)(((uintptr_t*)u->uc_mcontext.regs[29])[1]-base),(unsigned long)((uintptr_t*)u->uc_mcontext.regs[29])[4]);_exit(1);}
static void load_elf(const char *path,uintptr_t address)
{
    FILE *f=fopen(path,"rb");assert(f);Elf64_Ehdr h;assert(fread(&h,1,sizeof(h),f)==sizeof(h));
    unsigned char *memory=mmap((void*)address,0x7500000,7,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
    assert(memory==(void*)address);base=address;
    Elf64_Phdr ph[16];assert(h.e_phnum<=16);fseek(f,(long)h.e_phoff,SEEK_SET);assert(fread(ph,sizeof(*ph),h.e_phnum,f)==h.e_phnum);
    Elf64_Dyn *dynamic=NULL;
    for(unsigned i=0;i<h.e_phnum;++i) if(ph[i].p_type==PT_LOAD) {
        assert(ph[i].p_vaddr+ph[i].p_memsz<0x7500000);fseek(f,(long)ph[i].p_offset,SEEK_SET);
        assert(fread(memory+ph[i].p_vaddr,1,ph[i].p_filesz,f)==ph[i].p_filesz);
    }
    for(unsigned i=0;i<h.e_phnum;++i) if(ph[i].p_type==PT_DYNAMIC) dynamic=(void*)(memory+ph[i].p_vaddr);
    assert(dynamic);uintptr_t rela=0,relasz=0,jmp=0,jmpsz=0,str=0,sym=0;
    for(Elf64_Dyn *d=dynamic;d->d_tag;++d) switch(d->d_tag) {
        case DT_RELA:rela=d->d_un.d_ptr;break;case DT_RELASZ:relasz=d->d_un.d_val;break;
        case DT_JMPREL:jmp=d->d_un.d_ptr;break;case DT_PLTRELSZ:jmpsz=d->d_un.d_val;break;
        case DT_STRTAB:str=d->d_un.d_ptr;break;case DT_SYMTAB:sym=d->d_un.d_ptr;break;
    }
    uintptr_t starts[2]={rela,jmp},sizes[2]={relasz,jmpsz};
    for(unsigned k=0;k<2;++k) for(size_t i=0;i<sizes[k]/sizeof(Elf64_Rela);++i) {
        Elf64_Rela *r=(void*)(memory+starts[k]+i*sizeof(*r));uintptr_t *out=(void*)(memory+r->r_offset);
        if(ELF64_R_TYPE(r->r_info)==R_AARCH64_RELATIVE) *out=base+r->r_addend;
        else {
            Elf64_Sym *s=(void*)(memory+sym+ELF64_R_SYM(r->r_info)*sizeof(*s));
            const char *name=(void*)(memory+str+s->st_name);void *fn=dlsym(RTLD_DEFAULT,name);
            if(fn) *out=(uintptr_t)fn+r->r_addend;
        }
    }
    fclose(f);FIELD((void*)base,0x7164398,void*)=&guard;
}

static unsigned char file_root[0x1400],surface_controls[0x2300],file_model[0x60],browser[0x600],service[0x400];
static uint64_t paths[4];static int loads;
static void*is_visible;static int test_chop_mode;
static int component_visible(void*p){return p==is_visible||(is_visible&&test_chop_mode&&p==(char*)is_visible+0xa10);}
static int unblocked(void*p){(void)p;return 0;}
static unsigned invalidations;
static void invalidate(void*p,uint64_t a,uint64_t size){(void)p;(void)a;(void)size;++invalidations;}
static void load_file(void*p,const struct file_result*f,const unsigned char*confirmed){assert(p==service+0x30&&f->file&&*confirmed==1);assert(strstr(filename(f),".xpj"));++loads;}
static void put_string(void*p,const char*s){FN(0xf535c0,void(*)(void*,const char*))(p,s);}
static void button_event(unsigned c,int down){unsigned char e[16]={0};e[8]=(unsigned char)c;e[9]=(unsigned char)down;workflow_button(e,workflow_ticket());}
static char last_text[512];
static unsigned mono_labels;
static uint32_t ink;static float height;static FILE*draw;
static void set_colour(void*g,uint32_t c){(void)g;ink=c;}
static void fill(void*g,int x,int y,int w,int h){(void)g;assert(x>=0&&y>=5&&x+w<=320&&y+h<=240);if(draw)fprintf(draw,"R %d %d %d %d %08x\n",x,y,w,h,ink);}
static void font_make(uint64_t*f,int style,float h){(void)style;*f=0;height=h;}
static void font_use(void*g,const uint64_t*f){(void)g;(void)f;}
static void font_end(uint64_t*f){(void)f;}
static void output_text(void*g,const uint64_t*s,int x,int y,int w,int h,int flags,int lines,float scale){(void)g;(void)flags;(void)lines;(void)scale;snprintf(last_text,sizeof(last_text),"%s",(char*)(uintptr_t)*s);if(!strcmp(last_text,"Cancel"))mono_labels|=1;if(!strcmp(last_text,"Left"))mono_labels|=2;if(!strcmp(last_text,"Right"))mono_labels|=4;if(!strcmp(last_text,"Stereo > Mono"))mono_labels|=8;if(!strcmp(last_text,"Source"))mono_labels|=16;if(!strcmp(last_text,"Target"))mono_labels|=32;if(draw)fprintf(draw,"T %d %d %d %d %08x %.1f @%x %s\n",x,y,w,h,ink,height,flags,(char*)(uintptr_t)*s);}
static int copy_calls,chop_calls,all_sounds;
int step_native_sample_chop(void){return test_chop_mode;}
void step_native_audition_bind(void*q,void*t,void*r){(void)q;(void)t;(void)r;}
static struct batch_copy_progress test_progress;
static unsigned copy_begins;
int note_native_pad_index(unsigned*out){*out=0;return 1;}
static unsigned mono_calls,mono_last_target,mono_last_channel;
int batch_mono_begin(uintptr_t b,void*c,void*s,unsigned from,unsigned to,unsigned channel){assert(b==base&&c==(void*)atomic_load(&copy_context)&&s==(void*)atomic_load(&copy_services)&&from==0);++mono_calls;mono_last_target=to;mono_last_channel=channel;test_progress=(struct batch_copy_progress){BATCH_JOB_PREPARE,0,1};return 1;}
int step_native_browser_bank(int bank){static int current;if(bank>=0)current=bank;return current;}
int batch_copy_submit(uintptr_t b,void*c,const unsigned*s,const unsigned*d,unsigned n){assert(b==base&&c==(void*)atomic_load(&copy_context)&&n==2&&s[0]==0&&s[1]==17&&d[0]!=d[1]);++copy_calls;return 1;}
int batch_copy_begin(uintptr_t b,void*c,const unsigned*s,const unsigned*d,unsigned n){assert(!copy_begins);++copy_begins;batch_copy_submit(b,c,s,d,n);test_progress=(struct batch_copy_progress){BATCH_JOB_PREPARE,0,n};return 1;}
int batch_copy_begin_events(uintptr_t b,void*c,const unsigned*s,const unsigned*d,unsigned n,int events){assert(events==batch_events);return batch_copy_begin(b,c,s,d,n);}
void batch_copy_poll(struct batch_copy_progress*p){*p=test_progress;}
int batch_copy_release(void){if(test_progress.phase&&test_progress.phase<BATCH_JOB_DONE)return 0;test_progress=(struct batch_copy_progress){0};return 1;}
int batch_chop_submit(uintptr_t b,void*c,void*services,unsigned sound,const unsigned*s,const unsigned*d,unsigned n){assert(b==base&&c==(void*)atomic_load(&copy_context)&&services==(void*)1&&sound==0&&n==16);for(unsigned i=0;i<n;++i){assert(s[i]>0&&s[i]<=16&&d[i]!=sound);for(unsigned j=0;j<i;++j)assert(d[i]!=d[j]);}++chop_calls;return 1;}
int batch_chop_begin(uintptr_t b,void*c,void*services,unsigned sound,const unsigned*s,const unsigned*d,unsigned n){int ok=batch_chop_submit(b,c,services,sound,s,d,n);test_progress=(struct batch_copy_progress){BATCH_JOB_PREPARE,0,n};return ok;}
static unsigned char chop_sample[0x138],chop_backing[0x100],chop_properties[32][0x368];static uintptr_t chop_slices[32];
void step_layer_sample(void*i,int layer,uintptr_t*out,uintptr_t fn){(void)i;assert(!layer&&fn==base+0x2803b90);out[0]=(uintptr_t)chop_sample;out[1]=0;}
static unsigned char copy_context_data[0x100],copy_current[0x100],copy_mode[0x90],copy_controls[0x3400],copy_screen[0x148],copy_program[0x1400],sounds[128][0x2300];
static unsigned char browser_things[0xc0];static int browser_context_live,sample_loads,loaded_pad,selected_pad,copy_closes;
void *note_native_context(void){return browser_context_live?browser_things:NULL;}
static void select_pad(void*p,int n){assert(p==browser_things&&n>=0&&n<128);selected_pad=n;}
static void sample_action(void*p,const struct file_result*f){assert(p==service+0x10&&f->file);assert(!strcmp(filename(f),"Kick drum.wav"));loaded_pad=selected_pad;++sample_loads;}
static unsigned char copy_layers[128][0xb00];
static unsigned browser_previews,browser_preview_stops,located_rows;static int simulate_navigation;
static unsigned private_previews;static int64_t private_first,private_last;
static void private_sample_receiver(void*p,const uintptr_t*sample,unsigned flags){
 assert(p==volume_service&&sample[0]==(uintptr_t)chop_sample&&flags==4);
 FIELD(p,0x3e0,uintptr_t)=sample[0];unsigned char payload[80]={0};FIELD(payload,8,void*)=p;FIELD(payload,0x10,unsigned)=1;
 assert(workflow_preview_worker(payload));assert(FIELD(payload,0x10,unsigned)==4);
 private_first=FIELD(payload,0x18,int64_t);private_last=FIELD(payload,0x20,int64_t);assert(private_last>private_first);
 FIELD(p,0x2e8,unsigned char)=1;workflow_preview_played(payload);
 assert(FIELD(payload,0x28,int64_t)==private_first&&FIELD(payload,0x30,int64_t)==private_last&&!FIELD(payload,0x38,unsigned));++private_previews;
}
static int native_selected_row;static unsigned listing_refreshes;
static int list_selection(void*p){assert(p==file_root+0xff0);return native_selected_row;}
static void refresh_listing(void*p){assert(p==browser+0x458);++listing_refreshes;}
static unsigned native_delete_requests;
static void native_delete(void*p,const int*mode){assert(p==service&&mode==(int*)(file_root+0x1388)&&*mode==FILE_SAMPLES);assert(strstr((char*)(uintptr_t)FIELD(service,0x1b0,uint64_t),"/Samples/User/DeleteMe.wav"));++native_delete_requests;}
static unsigned chop_auditions,chop_stops,chop_audition_slice;
int step_native_chop_audition(void*p,unsigned src,unsigned sl,int down){assert(p==batch_program&&src==slice_sound&&sl<128);if(down){++chop_auditions;chop_audition_slice=sl;}else ++chop_stops;return 1;}
static void stop_browser_preview(void*p){assert(p==volume_service);FIELD(p,0x2e8,unsigned char)=0;++browser_preview_stops;}
static void replace_private_voice(void*p,unsigned port,unsigned note,unsigned force,float fade){assert(p==copy_program&&!port&&!note&&force==1&&fade==0.f);}
static void preview_file(void*p,const struct file_result*f,int automatic){assert(p==volume_service&&f->file&&strstr(filename(f),".wav")&&!automatic);++browser_previews;}
static void selected_file(void*p,const struct file_result*f){
 assert(f->file);
 if(p==service+0x160){FN(0xf1d180,void(*)(void*,const void*))(service+0x1b0,f);return;}if(p==service+0x240)return;
 assert(p==service+0x1b8||p==service+0x298);
 /* Selected-file assignment never navigates. The old fixture wrongly did. */
}
static int allow_directory(void*p,const struct file_result*f){(void)p;assert(f->file);return 1;}
static void directory_label(void*p,const void*name){(void)p;assert(name);}
static void directory_changed(void*p,uint64_t slot,uint64_t immediate){assert(p==browser+0x10&&slot==0x10&&immediate==1);}
static void set_directory(void*p,const struct file_result*f,int directories,int files_too){
 assert(p==browser+0x458&&directories==1&&files_too==1&&simulate_navigation);
 put_string(browser+0x4a0,(char*)(uintptr_t)f->file);put_string(file_model+0x38,(char*)(uintptr_t)f->file);put_string(&paths[0],(char*)(uintptr_t)locate_file);FIELD(file_model,0x48,void*)=paths+1;
}
static void select_list_row(void*p,int row){assert(p==file_root+0xff0&&row==0);++located_rows;}
static unsigned preview_calls,preview_releases,preview_last;
int batch_copy_preview(uintptr_t b,void*c,unsigned src,int down){assert(b==base&&c==(void*)atomic_load(&copy_context)&&src<128);if(down){++preview_calls;preview_last=src;}else{++preview_releases;assert(src==preview_last);}return 1;}
unsigned batch_copy_preview_status(void){return BATCH_PREVIEW_IDLE;}
static unsigned event_preview_calls,event_preview_stops;
int batch_event_preview(uintptr_t b,void*c,void*p,const void*e,unsigned key,unsigned mode){assert(b==base&&c==(void*)atomic_load(&copy_context)&&p==copy_program&&e&&key<16&&mode<10);++event_preview_calls;return 1;}
int batch_event_preview_stop(uintptr_t b,void*c,int cancel){assert(b==base&&c==(void*)atomic_load(&copy_context)&&(cancel==0||cancel==1));++event_preview_stops;return 1;}
unsigned batch_event_preview_pending(void){return 0;}
static int held_native_exit;
void batch_copy_hold_native_exit(int hold){held_native_exit=hold;}
void phone_release_copy_gesture(void){assert(held_native_exit);}
static unsigned test_exit_state,exit_starts;
int batch_copy_exit_begin(uintptr_t b,void*c){assert(b==base&&c==(void*)atomic_load(&copy_context)&&!test_exit_state);++exit_starts;test_exit_state=BATCH_EXIT_PENDING;return 1;}
unsigned batch_copy_exit_poll(void){return test_exit_state;}
void batch_copy_exit_reset(void){if(test_exit_state!=BATCH_EXIT_PENDING)test_exit_state=0;}
static void complete_exit(void){assert(test_exit_state==BATCH_EXIT_PENDING);++copy_closes;test_exit_state=BATCH_EXIT_DONE;workflow_tick();assert(!batch_leaving&&!batch&&!workflow_owns(3,0));}
static void*get_instrument(void*p,int i){assert(p==copy_program);return i>=0&&i<128?sounds[i]:NULL;}
static int has_sample(void*p){return all_sounds||p==sounds[0]||p==sounds[17]||p==sounds[32];}
static int get_sound_index(void*p,void*i){assert(p==copy_program&&i==sounds[0]);return 0;}
static unsigned char derived_copy[0x148];static void*threads=(void*)1;
static int unlocked(void*p){(void)p;return 0;}
static unsigned char volume_registry[0x80],volume_object[0x400],master_parameter[0x80];
static void*raw_surface_owner;static void*surface_provider=&raw_surface_owner;
static void*service_entry(void*p,void*type){assert(type&&*(uintptr_t*)type!=base+0x6e7d470);assert(p==service||p==volume_registry);if(*(uintptr_t*)type==base+0x6e08e40)return &surface_provider;if(*(uintptr_t*)type==base+0x6e0d438){static void*browsing;browsing=service;return &browsing;}return &threads;}
static void primary_base(void*p,void*t){assert(p==derived_copy&&t==threads);}
static void copy_base(void*p,void*s,void*a,void*b){assert(p==derived_copy+0x48&&s==service&&!a&&!b);memcpy(p,copy_context_data,sizeof(copy_context_data));workflow_copy_context(p,(void*)1);}
int main(int argc,char**argv){assert(argc==3);struct sigaction sa={0};sa.sa_sigaction=fault_signal;sa.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&sa,NULL);sigaction(SIGBUS,&sa,NULL);sigaction(SIGILL,&sa,NULL);load_elf(argv[1],0x1800000000ULL);
char short_caption[64];ui_caption(short_caption,sizeof(short_caption),"АБВГДЕЖЗ",6);assert(!strcmp(short_caption,"АБВ..."));
struct phone_input saved_query=query;phone_init(&query,128);assert(phone_sync(&query,"abcdefghijklmnopqrstuvXYZ",25));query_view(short_caption,sizeof(short_caption));assert(strstr(short_caption,"XYZ"));query.cursor=2;query_view(short_caption,sizeof(short_caption));assert(!strncmp(short_caption,"ab|c",4));query=saved_query;
stub(0x112fd50,component_visible);stub(0x112f6b0,unblocked);stub(0x1149880,invalidate);stub(0x102d5e0,set_colour);stub(0x102e000,fill);stub(0x1082810,font_make);stub(0x102dca0,font_use);stub(0x1070d60,font_end);stub(0x10817b0,output_text);
stub(0x286bbe0,get_instrument);stub(0x286bc20,get_sound_index);stub(0x2804760,has_sample);stub(0x28f9d60,select_pad);stub(0x15001a0,selected_file);stub(0x3b71f20,select_list_row);
stub(0x14fc8f0,native_delete);stub(0x3b71e80,list_selection);stub(0x11012d0,refresh_listing);
stub(0x1deee80,allow_directory);stub(0x3b88a60,directory_label);stub(0x11015b0,set_directory);
stub(0x1db0e80,directory_changed);
stub(0xb51828,unlocked);stub(0xb51808,unlocked);stub(0x143ad50,service_entry);stub(0x20be8c0,preview_file);stub(0x20beec0,stop_browser_preview);
stub(0x20be470,private_sample_receiver);stub(0x286d8a0,replace_private_voice);
__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
FIELD(chop_sample,0x130,void*)=chop_backing;FIELD(chop_backing,4,unsigned)=32000;
for(unsigned n=0;n<128;++n){FIELD(sounds[n],0x22e8,void*)=copy_layers[n];FIELD(copy_layers[n],0x740+0x78,int64_t)=0;FIELD(copy_layers[n],0x740+0xd0,int64_t)=1000;}
char storage_name[80],storage_address[512];assert(storage_caption("/media/MPCMOD/MPC-Sample/Samples/User",storage_name,sizeof(storage_name),storage_address,sizeof(storage_address))&&!strcmp(storage_name,"SD: MPCMOD")&&!strcmp(storage_address,"SD:MPCMOD/MPC-Sample/Samples/User"));
assert(storage_caption("/data/MPC-Sample",storage_name,sizeof(storage_name),storage_address,sizeof(storage_address))&&!strcmp(storage_name,"Internal RAM")&&!strcmp(storage_address,"RAM:/MPC-Sample"));assert(!storage_caption("/database",storage_name,sizeof(storage_name),storage_address,sizeof(storage_address)));
assert(mkdtemp(test_root));FILE*f=fopen(test_path("/Kick drum.wav"),"wb");assert(f);fclose(f);f=fopen(test_path("/Snare.wav"),"wb");assert(f);fclose(f);
put_string(&paths[0],test_path("/Kick drum.wav"));put_string(&paths[1],test_path("/Snare.wav"));put_string(&paths[2],test_path(""));
FIELD(file_model,0x30,void*)=browser;FIELD(file_model,0x40,void*)=paths;FIELD(file_model,0x48,void*)=paths+3;
put_string(browser+0x4a0,test_path(""));put_string(file_model+0x38,test_path(""));
FIELD(file_root,0x100,void*)=surface_controls;FIELD(file_root,0x120,void*)=service;FIELD(file_root,0xff0+0x188,void*)=file_model;
FIELD(service,0x20,void*)=(void*)1;FIELD(service,0x28,uintptr_t)=(uintptr_t)sample_action;
FIELD(service,0x40,void*)=(void*)1;FIELD(service,0x48,uintptr_t)=(uintptr_t)load_file;
/* Actual native FileBrowsingActions constructor installs the live preview
 * closure. No fake DI lookup and no Copy context exists at this point. */
unsigned char native_bindings[0x320]={0},actions_object[8]={0};
FN(0x1501380,void(*)(void*,void*,void*,void*,void*,void*,void*))(actions_object,native_bindings,(void*)1,(void*)1,volume_object,(void*)1,(void*)1);
memcpy(service+0x90,native_bindings+0x90,0x20);
assert(FIELD(service,0x90,void*)==volume_object&&FIELD(service,0xa0,uintptr_t)==base+0x1500620&&FIELD(service,0xa8,uintptr_t)==base+0x1500560);
FIELD(browser_things,0xb8,void*)=copy_program;FIELD(browser_things,0x60,void*)=sounds[0];browser_context_live=1;
assert(FIELD((void*)base,0x6da3d18,uintptr_t)==base+0x19d0c80); /* address point, not vtable header */
FIELD(volume_object,0,uintptr_t)=base+0x6da3d18;FIELD(volume_object,0xc8,void*)=volume_object+0xc8;FIELD(volume_object,0xd0,void*)=volume_object+0xc8;FIELD(volume_object,0x110,float)=1.f;
FIELD(volume_object,0x358,void*)=copy_program;
raw_surface_owner=(void*)((uintptr_t)surface_controls-0x6c0);assert(!atomic_load(&copy_services)&&!atomic_load(&copy_context));workflow_registry_bind(volume_registry);
/* Fail closed before Main Volume is captured, even with a valid preview. */
files=file_root;assert(!workflow_preview_prepare(service,FILE_SAMPLES)&&FIELD(volume_object,0x110,float)==0.f&&!browser_previews);
FIELD(master_parameter,4,float)=1.f;FIELD(master_parameter,0x60,unsigned)=1;
workflow_master_volume_bind(master_parameter,surface_controls+0x2ea8);
assert(workflow_preview_prepare(service,FILE_SAMPLES)&&volume_ready&&volume_percent==75);
for(unsigned n=0;n<2;++n){void*c=file_root+(n?0x11d0:0x418);FIELD(c,0x18,void*)=file_root;FIELD(c,0x20,int)=0;FIELD(c,0x24,int)=n?40:6;FIELD(c,0x28,int)=n?320:106;FIELD(c,0x2c,int)=20;}
put_string(browser+0x4a0,"/media/MPCMOD/MPC-Sample/Samples/User");storage_paint(file_root,(void*)1);assert(!strcmp(last_text,"SD:MPCMOD/MPC-Sample/Samples/User"));put_string(browser+0x4a0,test_path(""));
FIELD(file_root,0x418+0x28,int)=0;FIELD(file_root,0x11d0+0x28,int)=0;

FN(0xf1c620,void(*)(void*))(service+0x1b0);FN(0xf1c620,void(*)(void*))(service+0x230);FN(0xf1c620,void(*)(void*))(service+0x238);FN(0xf1c620,void(*)(void*))(service+0x310);FN(0xf1c620,void(*)(void*))(service+0x318);
/* RTTI and actual callbacks pin Samples=0/Projects=1 and distinguish the
 * sample load, project load and preview functions; old tests swapped these. */
assert(strstr((char*)FIELD((void*)base,0x6e0d468,uintptr_t),"onSampleSelected"));
assert(strstr((char*)FIELD((void*)base,0x6e0d478,uintptr_t),"onProjectSelected"));
files=file_root;atomic_store(&files_id,(uintptr_t)files);is_visible=files;
search_scope=0;FIELD(surface_controls,0x22a0,unsigned char)=1;workflow_tick();button_event(0x31,1);button_event(0x0e,1);assert(search&&workflow_owns(3,0));workflow_tick();assert(!scanning&&entry_count==2&&match_count==2&&!loads);
unsigned char main_event[16]={0};main_event[8]=0x13;main_event[9]=0;
assert(!workflow_owns(1,0x13)&&!workflow_control(main_event,workflow_ticket()-1)); /* global even across modal tickets */
assert(phone_sync(&query,"KICK",4));filter();assert(match_count==1&&!strcmp(filename(&entries[matches[0]]),"Kick drum.wav"));
/* Stale producer events must never affect a reopened query. */
uint64_t old=workflow_ticket();button_event(0x0c,1);button_event(0x0e,1);workflow_tick();assert(search&&query.length==0);workflow_pad(14,1,100,old);assert(query.length==0);
workflow_pad(14,1,100,workflow_ticket());workflow_pad(14,0,200,workflow_ticket());assert(query.length==1);phone_tick(&query,1101);assert(query.cursor==1);phone_sync(&query,"KICK",4);filter();
draw=fopen(argv[2],"w");assert(draw);workflow_files_paint(files,(void*)1);fclose(draw);draw=NULL;
button_event(0x0d,1);assert(results&&!loads);button_event(0x0e,1);assert(browser_previews==1&&!loads&&!sample_loads&&search);
FIELD(surface_controls,0x22a0,unsigned char)=0;button_event(0x31,0);button_event(0x6f,1);assert(sample_loads==1&&search&&results&&match_count==1);close_search();
/* Same file screen and native callback path serves Open Project too. */
f=fopen(test_path("/project.xpj"),"wb");assert(f);fclose(f);put_string(&paths[0],test_path("/project.xpj"));FIELD(file_root,0x1388,int)=FILE_PROJECTS;start_search();workflow_tick();phone_sync(&query,"no match",8);filter();results=1;choose_result();assert(search&&!match_count&&!loads);close_search();
start_search();workflow_tick();phone_sync(&query,"project",7);filter();results=1;button_event(0x6f,1);assert(project_confirm&&!loads);button_event(0x0c,1);assert(!project_confirm&&!loads&&search);
button_event(0x6f,1);assert(project_confirm&&!loads);button_event(0x0e,1);assert(loads==1&&!search);FIELD(file_root,0x1388,int)=FILE_SAMPLES;
/* Samples0 -> onSampleSelected14fc5a0. Projects1 -> onProjectSelected14fc6a0.
 *14fb1c0 is Preview, not Load. Selecting a pad stays silent until Load. */
stub(0x286bbe0,get_instrument);stub(0x286bc20,get_sound_index);stub(0x2804760,has_sample);stub(0x28f9d60,select_pad);stub(0x15001a0,selected_file);
__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
FIELD(file_root,0x1388,int)=FILE_SAMPLES;
put_string(&paths[0],test_path("/Kick drum.wav"));start_search();workflow_tick();phone_sync(&query,"Kick",4);filter();results=1;
step_native_browser_bank(1);workflow_pad(4,1,0,workflow_ticket());assert(result_pad==20&&sample_loads==1);choose_result();assert(sample_loads==2&&loaded_pad==20&&search&&loads==1);workflow_pad(5,1,0,workflow_ticket());button_event(0x6f,1);assert(sample_loads==3&&loaded_pad==21&&search);close_search();
/* A preview emits the native preview-file property, not a sample load; same
 * result can be retriggered without losing selection or destination. */
start_search();workflow_tick();phone_sync(&query,"Kick",4);filter();results=1;
button_event(0x0e,1);button_event(0x0e,0);button_event(0x0e,1);
assert(browser_previews==2&&!search_preview_on&&sample_loads==3&&search&&match_count==1);
/* Turning mode back on plays current result; scrolling stops first and
 * auto-previews the new result, while Off remains silent on navigation. */
phone_sync(&query,"",0);filter();results=1;unsigned before_auto=browser_previews,stops_before=browser_preview_stops;
button_event(0x0e,1);assert(search_preview_on&&browser_previews==before_auto+1);
button_event(0x36,1);assert(browser_previews==before_auto+2&&browser_preview_stops>stops_before);
button_event(0x0e,1);assert(!search_preview_on&&!search_preview_file);unsigned after_off=browser_previews;
button_event(0x37,1);assert(browser_previews==after_off);
close_search();
/* Recursive read-only search finds nested audio, skips directories as results,
 * ignores symlink loops and closes all readers on repeated Cancel. */
mkdir(test_path("/sub"),0700);f=fopen(test_path("/sub/Deep.wav"),"wb");assert(f);fclose(f);
assert(!symlink(test_path(""),test_path("/sub/loop"))||errno==EEXIST);search_scope=1;start_search();
for(int n=0;n<100&&scanning;++n){workflow_tick();}
assert(!scanning&&entry_count==3);results=1;
char results_trace[1024];snprintf(results_trace,sizeof(results_trace),"%s.results",argv[2]);draw=fopen(results_trace,"w");assert(draw);workflow_files_paint(files,(void*)1);fclose(draw);draw=NULL;
phone_sync(&query,"Deep",4);filter();assert(match_count==1&&!strcmp(filename(&entries[matches[0]]),"Deep.wav"));close_search();
for(unsigned i=0;i<4;++i){search_scope=1;start_search();close_search();unsigned before=invalidations;FIELD(surface_controls,0x22a0,unsigned char)=0;workflow_tick();FIELD(surface_controls,0x22a0,unsigned char)=1;workflow_tick();assert(invalidations>before);}
/* Library scope starts at Samples even from its User subfolder; project
 * search uses Projects and skips audio. The directory scanner never writes. */
mkdir(test_path("/MPC-Sample"),0700);
mkdir(test_path("/MPC-Sample/Samples"),0700);
mkdir(test_path("/MPC-Sample/Samples/User"),0700);
mkdir(test_path("/MPC-Sample/Samples/Factory"),0700);
f=fopen(test_path("/MPC-Sample/Samples/Factory/Sibling.WAV"),"wb");assert(f);fclose(f);
/* The model cache can still name a previous folder. Actual BrowserLogic
 * current directory must win, including Library lookup from a subfolder. */
put_string(file_model+0x38,"/no-such-stale-cache");put_string(browser+0x4a0,test_path("/MPC-Sample/Samples/User"));search_scope=2;start_search();
for(int n=0;n<100&&scanning;++n){workflow_tick();}
assert(library_root_found&&!scanning&&entry_count==1&&!strcmp(filename(&entries[0]),"Sibling.WAV"));close_search();
mkdir(test_path("/MPC-Sample/Projects"),0700);
mkdir(test_path("/MPC-Sample/Projects/Nested"),0700);
f=fopen(test_path("/MPC-Sample/Projects/Prod.xpj"),"wb");assert(f);fclose(f);
f=fopen(test_path("/MPC-Sample/Projects/Nested/Ignore.wav"),"wb");assert(f);fclose(f);
put_string(browser+0x4a0,test_path("/MPC-Sample/Projects/Nested"));FIELD(file_root,0x1388,int)=FILE_PROJECTS;start_search();
for(int n=0;n<100&&scanning;++n){workflow_tick();}
assert(library_root_found&&!scanning&&entry_count==1&&!strcmp(filename(&entries[0]),"Prod.xpj"));close_search();
/* Storage selection from factory project browser must find user projects,
 * never silently keep searching the factory kit presets. */
mkdir(test_path("/storage-meta"),0700);f=fopen(test_path("/storage-meta/save-path"),"w");assert(f);fprintf(f,"%s\n",test_path("/MPC-Sample"));fclose(f);
mkdir(test_path("/Internal"),0700);mkdir(test_path("/Internal/Projects"),0700);
f=fopen(test_path("/Internal/Projects/InternalSong.xpj"),"wb");assert(f);fclose(f);
f=fopen(test_path("/MPC-Sample/Projects/Kit.xpm"),"wb");assert(f);fclose(f);
put_string(browser+0x4a0,"/content/Projects");search_storage=0;start_search();for(int n=0;n<100&&scanning;++n)workflow_tick();
assert(search_storage==2&&!fault&&entry_count==1&&!strcmp(filename(&entries[0]),"Prod.xpj"));
unsigned char storage_control[16]={0};storage_control[8]=0x12;storage_control[9]=0;workflow_control(storage_control,workflow_ticket());for(int n=0;n<100&&scanning;++n)workflow_tick();
assert(search_storage==1&&!fault&&entry_count==1&&!strcmp(filename(&entries[0]),"InternalSong.xpj"));
storage_control[9]=127;workflow_control(storage_control,workflow_ticket());for(int n=0;n<100&&scanning;++n)workflow_tick();assert(search_storage==2&&entry_count==1);
close_search();search_storage=0;
put_string(file_model+0x38,test_path(""));put_string(browser+0x4a0,test_path(""));
/* Exact user scenario: find from root, then Shift+encoder reveals the file
 * inside MPC-Sample/Samples/User with a selected native row, without Load.
 * The same operation in Project search never asks to open the project. */
f=fopen(test_path("/MPC-Sample/Samples/User/Wanted.wav"),"wb");assert(f);fclose(f);
simulate_navigation=1;FIELD(file_root,0x1388,int)=FILE_SAMPLES;search_scope=2;start_search();for(int n=0;n<100&&scanning;++n)workflow_tick();phone_sync(&query,"Wanted",6);filter();results=1;
assert(match_count==1);button_event(0x31,1);button_event(0x6f,1);assert(!search&&locating&&sample_loads==3);workflow_tick();assert(!locating&&located_rows==1&&strstr((char*)(uintptr_t)FIELD(browser,0x4a0,uint64_t),"/Samples/User"));
button_event(0x31,0);FIELD(surface_controls,0x22a0,unsigned char)=0;
put_string(browser+0x4a0,test_path(""));put_string(file_model+0x38,test_path(""));FIELD(file_root,0x1388,int)=FILE_PROJECTS;start_search();for(int n=0;n<100&&scanning;++n)workflow_tick();phone_sync(&query,"Prod",4);filter();results=1;assert(match_count==1);
button_event(0x31,1);button_event(0x6f,1);workflow_tick();assert(!search&&!locating&&!project_confirm&&located_rows==2&&loads==1);button_event(0x31,0);FIELD(surface_controls,0x22a0,unsigned char)=0;
simulate_navigation=0;put_string(browser+0x4a0,test_path(""));put_string(file_model+0x38,test_path(""));put_string(&paths[0],test_path("/Kick drum.wav"));FIELD(file_model,0x48,void*)=paths+3;
/* K3 owns only Sample Select's unshifted control. Real native float-property
 * setter updates preview gain at110; the selected pad and load count stay. */
FIELD(file_root,0x1388,int)=FILE_SAMPLES;workflow_tick();assert(workflow_owns(1,0x12));
unsigned char volume_event[16]={0};volume_event[8]=0x12;volume_event[9]=64;assert(workflow_control(volume_event,workflow_ticket())==1&&volume_percent==50&&sample_loads==3&&loads==1);
volume_event[9]=0;workflow_control(volume_event,workflow_ticket());assert(FIELD(volume_object,0x110,float)==0.f);
volume_event[9]=127;workflow_control(volume_event,workflow_ticket());assert(FIELD(volume_object,0x110,float)==1.f);
/* The SAME active preview follows Main Volume on later ticks; K3=100 cannot
 * bypass Master mute. Invalid/missing gain fails silent, without file IO. */
FIELD(master_parameter,4,float)=0.f;workflow_tick();assert(volume_ready&&volume_percent==100&&FIELD(volume_object,0x110,float)==0.f);
FIELD(master_parameter,4,float)=.5f;workflow_tick();assert(FIELD(volume_object,0x110,float)==1.f); /* native output mixer applies Main once */
FIELD(master_parameter,4,float)=NAN;assert(!workflow_preview_prepare(service,FILE_SAMPLES)&&FIELD(volume_object,0x110,float)==0.f);
FIELD(master_parameter,4,float)=1.f;workflow_tick();assert(volume_ready);
FIELD(sounds[20],0x908,unsigned)=99;assert(!workflow_preview_prepare(service,FILE_SAMPLES)&&FIELD(volume_object,0x110,float)==0.f);FIELD(sounds[20],0x908,unsigned)=0;workflow_tick();assert(volume_ready);
FIELD(surface_controls,0x22a0,unsigned char)=1;workflow_tick();assert(workflow_control(volume_event,workflow_ticket())==0);FIELD(surface_controls,0x22a0,unsigned char)=0;workflow_tick();
/* Linked libraries and hidden pack folders are searched once, not omitted;
 * inode deduplication still prevents root/sub/loop recursion. */
mkdir(test_path("/.pack"),0700);f=fopen(test_path("/.pack/Hidden.wav"),"wb");assert(f);fclose(f);
assert(!symlink(test_path("/MPC-Sample/Samples/Factory"),test_path("/linked-pack")));
assert(browser_tree_start(test_path("/linked-pack"),0));char linked_path[1024];int linked_result;
do{linked_result=browser_tree_next(linked_path,sizeof(linked_path),16);}while(!linked_result);
assert(linked_result==1&&strstr(linked_path,"Sibling.WAV"));browser_tree_close();
FIELD(file_root,0x1388,int)=FILE_SAMPLES;search_scope=1;start_search();for(int n=0;n<100&&scanning;++n)workflow_tick();phone_sync(&query,"Hidden",6);filter();assert(match_count==1);close_search();
/* An unreadable root remains an explicit error, never a silent folder-only
 * fallback that falsely reports a complete recursive search. */
put_string(browser+0x4a0,"/mpc-no-such-directory-for-search-test");start_search();workflow_tick();assert(search&&fault&&!scanning&&!entry_count);close_search();put_string(browser+0x4a0,test_path(""));
/* Preview K3 only exists for an actual selected audio file. Native optional
 * selection observes the same row as stock Preview, including no selection. */
FIELD(file_root,0x1388,int)=FILE_SAMPLES;native_selected_row=-1;workflow_tick();assert(!selected_audio&&!workflow_owns(1,0x12));
native_selected_row=0;put_string(&paths[0],test_path(""));workflow_tick();assert(!selected_audio&&!workflow_owns(1,0x12));
put_string(&paths[0],test_path("/Kick drum.wav"));workflow_tick();assert(selected_audio&&workflow_owns(1,0x12));
FIELD(surface_controls,0x22a0,unsigned char)=1;workflow_tick();button_event(0x0d,1);assert(!search&&!batch); /* Shift+B2 Eject swallowed */
FIELD(surface_controls,0x22a0,unsigned char)=0;workflow_tick();
f=fopen(test_path("/MPC-Sample/Samples/User/DeleteMe.wav"),"wb");assert(f);fputs("audio",f);fclose(f);
put_string(&paths[0],test_path("/MPC-Sample/Samples/User/DeleteMe.wav"));
/* Actual stock B3 entry is used, not a new file deletion implementation.
 * Recording dialog boundary does not delete: hardware confirms/cancels. */
button_event(0x09,1);assert(native_delete_requests==1&&access(test_path("/MPC-Sample/Samples/User/DeleteMe.wav"),F_OK)==0);
button_event(0x09,0);assert(native_delete_requests==1);
put_string(&paths[0],test_path("/Snare.wav"));button_event(0x09,1);assert(native_delete_requests==1); /* nonUser guard */
put_string(&paths[0],test_path("/Kick drum.wav"));
/* A large library must not search only its first4096 files: the cap applies
 * to stored matching results. A new query restarts a truncated scan. */
mkdir(test_path("/large"),0700);for(unsigned n=0;n<4100;++n){char suffix[80];snprintf(suffix,sizeof(suffix),"/large/noise-%04u.wav",n);f=fopen(test_path(suffix),"wb");assert(f);fclose(f);}
f=fopen(test_path("/large/Needle.wav"),"wb");assert(f);fclose(f);
put_string(browser+0x4a0,test_path("/large"));start_search();for(unsigned n=0;n<5000&&scanning;++n)workflow_tick();assert(overflow&&entry_count==MAX_FILES);
phone_sync(&query,"Needle",6);filter();for(unsigned n=0;n<5000&&scanning;++n)workflow_tick();
assert(!scanning&&!overflow&&entry_count==1&&match_count==1&&!strcmp(filename(&entries[matches[0]]),"Needle.wav"));close_search();put_string(browser+0x4a0,test_path(""));
/* '#' is the fourth pad8 symbol and also cycles with encoder/+/- search input. */
phone_init(&query,64);phone_enable(&query,1);for(unsigned i=0;i<4;++i){phone_pad(&query,8,1,100+i*100);phone_pad(&query,8,0,110+i*100);}assert(query.text[0]=='#');
browser_context_live=0;FIELD(file_root,0x1388,int)=0;search_scope=0;step_native_browser_bank(0);
/* Mapping UI, bank browsing, manual targets and two-stage confirmation. */
stub(0x286bbe0,get_instrument);stub(0x2804760,has_sample);__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
FIELD(copy_context_data,0,uintptr_t)=base+0x6d8cd88;FIELD(copy_context_data,0x78,void*)=copy_current;FIELD(copy_context_data,0x98,void*)=copy_mode;FIELD(copy_mode,0x80,int)=0x20a7;FIELD(copy_current,0xb8,void*)=copy_program;FIELD(copy_screen,0x100,void*)=copy_controls;FIELD(copy_screen,0x108,void*)=copy_mode;
copy_root=copy_screen;atomic_store(&copy_id,(uintptr_t)copy_root);is_visible=copy_root;
/* Reproduce the hardware-only failure using the REAL most-derived ctor.
 * The context hook runs in the base ctor, then native code changes its vptr. */
stub(0x14fc8f0,native_delete);stub(0x3b71e80,list_selection);stub(0x11012d0,refresh_listing);
stub(0xb51828,unlocked);stub(0xb51808,unlocked);stub(0x143ad50,service_entry);stub(0x1c0f330,primary_base);stub(0x1518980,copy_base);
__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
FN(0x15173c0,void(*)(void*,void*,void*,void*))(derived_copy,service,NULL,NULL);
assert(atomic_load(&copy_context)==(uintptr_t)(derived_copy+0x48));
assert(FIELD(derived_copy,0x48,uintptr_t)==base+0x6e0dfd8&&FIELD(derived_copy,0x48,uintptr_t)!=base+0x6d8cd88);
assert(current_program()==copy_program);workflow_tick();
unsigned char b3[16]={0};b3[8]=0x0e;b3[9]=1;
assert(workflow_button(b3,workflow_ticket())==0&&!batch); /* Copy B3 stays native, even in Chop prototype. */
/* Execute the real native queued-button decoder and BoolProperty setter,
 * not an adapter which only updates this module's local Shift variable. */
void*shift_property=copy_controls+0x2250;FIELD(shift_property,8,void*)=(char*)shift_property+8;FIELD(shift_property,0x10,void*)=(char*)shift_property+8;
unsigned char midi_parser[0x18]={0},physical_shift[16]={0};FIELD(midi_parser,0x10,void*)=copy_controls;FIELD(physical_shift,0,void*)=midi_parser;physical_shift[8]=0x31;physical_shift[9]=1;
uint64_t before_batch_ticket=workflow_ticket();assert(workflow_button(physical_shift,before_batch_ticket)==0);FN(0x159f2f0,void(*)(void*))(physical_shift);assert(FIELD(copy_controls,0x22a0,unsigned char)==1);
button_event(0x0d,1);assert(batch&&!batch_page);
unsigned char delayed_shift_up[16]={0};memcpy(delayed_shift_up,physical_shift,16);delayed_shift_up[9]=0;
assert(workflow_button(delayed_shift_up,before_batch_ticket)==0&&!shift);FN(0x159f2f0,void(*)(void*))(delayed_shift_up);assert(FIELD(copy_controls,0x22a0,unsigned char)==0);
assert(!workflow_owns(2,0x31));
/* Other stale controls stay suppressed; Shift down/up always pass exactly once. */
unsigned char old_other[16]={0};old_other[8]=0x0e;old_other[9]=1;assert(workflow_button(old_other,before_batch_ticket)==1&&!batch_page);
delayed_shift_up[9]=1;assert(workflow_button(delayed_shift_up,before_batch_ticket)==0&&shift);
delayed_shift_up[9]=0;assert(workflow_button(delayed_shift_up,before_batch_ticket)==0&&!shift);
workflow_pad(0,1,0,workflow_ticket());button_event(0x36,1);assert(batch_bank==1);button_event(0x37,1);assert(batch_bank==0);button_event(0x23,1);workflow_pad(1,1,0,workflow_ticket());assert(selected_sources[0]&&selected_sources[17]);button_event(0x0e,1);assert(batch_page&&batch_count==2&&destination[0]!=destination[1]&&!copy_calls);
/* Encoder click auditions the source and never confirms/applies a copy.
 * +/- select rows in Targets, without changing bank. */
unsigned old_bank=(unsigned)batch_bank;batch_row=0;unsigned before_copy_preview=private_previews;button_event(0x6f,1);button_event(0x6f,0);assert(private_previews==before_copy_preview+1&&preview_source==source[0]&&preview_private&&!preview_calls&&!copy_calls&&!batch_confirm);
FIELD(volume_object,0x2e8,unsigned char)=0;button_event(0x6f,1);button_event(0x6f,0);assert(private_previews==before_copy_preview+2&&preview_source==source[0]);
for(unsigned code=0x51;code<=0x52;++code){assert(!workflow_owns(2,code));unsigned char transport[16]={0};transport[8]=code;transport[9]=1;assert(!workflow_button(transport,workflow_ticket()));transport[9]=0;assert(!workflow_button(transport,workflow_ticket()));assert(batch);}
button_event(0x6f,1);button_event(0x6f,0);
button_event(0x36,1);assert(batch_row==1&&(unsigned)batch_bank==old_bank&&preview_source==128);button_event(0x37,1);assert(batch_row==0&&(unsigned)batch_bank==old_bank);
unsigned auto_target=destination[0];pinned[0]=1;destination[0]=32;batch_bank=2;arrange();assert(destination[0]==32&&destination[1]!=32);destination[1]=32;unsigned overwrite;assert(!review(&overwrite));destination[0]=auto_target;pinned[0]=0;arrange();assert(review(&overwrite));
char path[1024];snprintf(path,sizeof(path),"%s.batch",argv[2]);draw=fopen(path,"w");assert(draw);batch_paint((void*)1);fclose(draw);draw=NULL;
/* Source green, occupied dim white, empty off; destinations white, the
 * current row's destination brighter (red if it would overwrite).
 * Source pads select their mapping row and never become self targets. */
batch_bank=0;batch_row=1;batch_palette();uint64_t light;
assert(workflow_light(0,&light)&&(uint32_t)light==0x32c780);
assert(workflow_light(15,&light)&&(uint32_t)light==0);
{uint64_t other,current;float a,b;uint32_t x,y;
 assert(workflow_light(destination[0]%16,&other)&&(uint32_t)other==0xffffff);
 assert(workflow_light(destination[1]%16,&current)&&(uint32_t)current==0xffffff);
 x=(uint32_t)(other>>32);y=(uint32_t)(current>>32);memcpy(&a,&x,4);memcpy(&b,&y,4);assert(b>a);}
batch_row=0;batch_palette();assert(workflow_light(destination[1]%16,&light)&&(uint32_t)light==0xffffff);
workflow_pad(0,1,0,workflow_ticket());assert(batch_row==0);
unsigned previous=destination[0];target_pad(source[0]);assert(destination[0]==previous&&strstr(batch_message,"itself"));
batch_bank=2;batch_palette();assert(workflow_light(0,&light)&&(uint32_t)light==0xffffff);
workflow_pad(0,1,0,workflow_ticket());assert(destination[0]==32&&pinned[0]);
batch_palette();assert(workflow_light(0,&light)&&(uint32_t)light==0xff2020);
batch_row=1;target_pad(32);assert(destination[1]!=32&&strstr(batch_message,"already assigned"));
batch_row=0;unsigned char k2[16]={0};k2[8]=0x11; /* Auto = first of17 slots */
workflow_control(k2,workflow_ticket());assert(!pinned[0]&&destination[0]!=32);
k2[9]=127;workflow_control(k2,workflow_ticket());assert(pinned[0]&&destination[0]==47);
/* Browsing banks is not a destructive reassignment of auto mappings. */
unsigned before_bank=destination[1];button_event(0x23,1);assert(batch_bank==3&&destination[1]==before_bank);
workflow_pad(2,1,0,workflow_ticket());assert(destination[0]==50&&pinned[0]);
/* Restore safe assignments for the mocked two-item dispatch. */
pinned[0]=0;arrange();batch_message[0]=0;
button_event(0x0e,1);assert(batch_confirm&&!copy_calls);button_event(0x0e,1);assert(batch_job&&!copy_calls&&!copy_begins);
workflow_tick();assert(!copy_begins); /* wait for first progress paint */
snprintf(path,sizeof(path),"%s.progress",argv[2]);draw=fopen(path,"w");assert(draw);batch_paint((void*)1);fclose(draw);draw=NULL;
assert(batch_job_presented);workflow_tick();assert(copy_calls==1&&copy_begins==1&&batch_job);
unsigned before_row=(unsigned)batch_row;previous=destination[0];
workflow_pad(5,1,0,workflow_ticket());workflow_control(k2,workflow_ticket());button_event(0x0e,1);button_event(0x0c,1);
assert(batch_job&&copy_begins==1&&(unsigned)batch_row==before_row&&destination[0]==previous);
unsigned char busy_shift[16]={0};busy_shift[8]=0x31;assert(workflow_button(busy_shift,workflow_ticket())==0);
test_progress=(struct batch_copy_progress){BATCH_JOB_COPY,1,2};workflow_tick();assert(batch_progress.done==1&&batch_progress.phase==BATCH_JOB_COPY);
snprintf(path,sizeof(path),"%s.copying",argv[2]);draw=fopen(path,"w");assert(draw);batch_paint((void*)1);fclose(draw);draw=NULL;
test_progress=(struct batch_copy_progress){BATCH_JOB_DONE,2,2};workflow_tick();assert(batch_job&&batch_progress.phase==BATCH_JOB_DONE);
snprintf(path,sizeof(path),"%s.done",argv[2]);draw=fopen(path,"w");assert(draw);batch_paint((void*)1);fclose(draw);draw=NULL;
button_event(0x0e,1);assert(!batch_job&&batch&&batch_leaving&&exit_starts==1&&!copy_closes);
assert(workflow_owns(3,0));workflow_tick();assert(batch_leaving);
unsigned char shift_release[16]={0};shift_release[8]=0x31;
assert(workflow_button(shift_release,workflow_ticket())==0&&!shift); /* release forwarded while leaving */
complete_exit();assert(copy_closes==1);
/* Cancel leaves the native Copy mode too, from Sources and Targets. */
for(unsigned page=0;page<2;++page){all_sounds=0;batch_start(0);if(page){selected_sources[0]=1;table();}button_event(0x0c,1);assert(batch_leaving&&batch);complete_exit();}
assert(copy_closes==3);
/* First free IN BANK has no silent rollover. All banks remains explicit. */
all_sounds=0;batch_start(0);selected_sources[0]=selected_sources[17]=1;table();
for(unsigned i=0;i<16;++i)occupied[i]=1;
batch_auto_bank=0;batch_layout=2;arrange();assert(destination[0]==128&&destination[1]==128&&!review(&(unsigned){0}));
batch_message[0]=0;snprintf(path,sizeof(path),"%s.no-space",argv[2]);draw=fopen(path,"w");assert(draw);batch_paint((void*)1);fclose(draw);draw=NULL;
batch_layout=0;arrange();assert(destination[0]>=16&&destination[0]<128&&destination[1]>=16&&destination[1]<128&&review(&(unsigned){0}));
batch_layout=2;batch_auto_bank=1;arrange();assert(destination[0]==16&&destination[1]==18);
pinned[0]=1;destination[0]=50;arrange();assert(destination[0]==50&&destination[1]==16);
unsigned char k3[16]={0};k3[8]=0x12;k3[9]=127;workflow_control(k3,workflow_ticket());assert(batch_layout==2);
k3[9]=64;workflow_control(k3,workflow_ticket());assert(batch_layout==1);
k3[9]=0;workflow_control(k3,workflow_ticket());assert(batch_layout==0);
assert(!batch_events);button_event(0x31,1);k3[9]=127;workflow_control(k3,workflow_ticket());assert(batch_events&&batch_layout==0);
button_event(0x31,0);assert(batch_events);k3[9]=64;workflow_control(k3,workflow_ticket());assert(batch_events&&batch_layout==1);
button_event(0x31,1);k3[9]=0;workflow_control(k3,workflow_ticket());assert(!batch_events&&batch_layout==1);button_event(0x31,0);

button_event(0x0c,1);complete_exit();
/* Across-bank selection accepts exactly 16; removing one frees one slot. */
all_sounds=1;batch_start(0);batch_bank=0;
for(unsigned i=0;i<16;++i)workflow_pad(i,1,0,workflow_ticket());
batch_bank=1;workflow_pad(0,1,0,workflow_ticket());assert(!selected_sources[16]&&strstr(batch_message,"16 / 16"));
batch_bank=0;workflow_pad(0,1,0,workflow_ticket());batch_bank=1;workflow_pad(0,1,0,workflow_ticket());assert(selected_sources[16]);
table();assert(batch_count==16);batch_close();
/* The same cap and target protection apply to multi-slice export. */
stub(0x286bc20,get_sound_index);__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
FIELD(copy_current,0x60,void*)=sounds[0];FIELD(sounds[0],0x22e8,void*)=copy_layers[0];
FIELD(chop_sample,0x130,void*)=chop_backing;FIELD(chop_backing,0x70,void*)=chop_slices;FIELD(chop_backing,0x78,void*)=chop_slices+32;
FIELD(chop_backing,4,unsigned)=32000;
for(unsigned i=0;i<32;++i){chop_slices[i]=(uintptr_t)chop_properties[i];FIELD(chop_properties[i],0x78,int64_t)=i*1000;FIELD(chop_properties[i],0xd0,int64_t)=(i+1)*1000;}
all_sounds=0;
#if MPC_EXPERIMENTAL_CHOP_EXPORT
unsigned char sample_screen[0xc40]={0},chop_current[0x680]={0};FIELD(chop_current,0xb8,void*)=copy_program;FIELD(chop_current,0x668,void*)=sounds[0];
FIELD(sample_screen,0x100,void*)=(void*)1;FIELD(sample_screen,0x108,void*)=copy_controls;FIELD(sample_screen,0x148,void*)=chop_current;FIELD(sample_screen,0x178,void*)=(void*)2;
is_visible=sample_screen;FIELD(copy_controls,0x22a0,unsigned char)=1;
test_chop_mode=0;workflow_sample_paint(sample_screen,(void*)1);
assert(!workflow_owns(2,0x0d)); /* B2 must reach the Lights handler outside Chop. */
unsigned char chord_b2[16]={0};chord_b2[8]=0x0d;chord_b2[9]=1;
assert(workflow_button(chord_b2,workflow_ticket())==0&&!batch); /* Ordinary Sample unchanged. */
test_chop_mode=1;snprintf(path,sizeof(path),"%s.chop-menu",argv[2]);draw=fopen(path,"w");assert(draw);workflow_sample_paint(sample_screen,(void*)1);fclose(draw);draw=NULL;
unsigned char chord_b1[16]={0};chord_b1[8]=0x0c;chord_b1[9]=1;
assert(workflow_button(chord_b1,workflow_ticket())==0); /* Extract stays native. */
 FIELD(sample_screen,0x148,void*)=NULL;assert(workflow_button(chord_b2,workflow_ticket())==1&&!batch&&strstr(batch_message,"Project changed"));
 FIELD(sample_screen,0x148,void*)=chop_current;
 /* Chop UI and Copy may use distinct CurrentThings wrappers for the SAME
  * Program; the visible ordinary Chop screen does not require Sample mode. */
 FIELD(copy_mode,0x80,int)=0x20a8;
unsigned char split_down[16]={0};split_down[8]=0x0e;split_down[9]=1;
assert(workflow_button(split_down,workflow_ticket())==0&&split_down[8]==0x0d&&atomic_load(&split_forwarded));
unsigned char shift_up[16]={0};shift_up[8]=0x31;
assert(workflow_button(shift_up,workflow_ticket())==0);
unsigned char split_up[16]={0};split_up[8]=0x0e;
assert(workflow_button(split_up,workflow_ticket()-1)==0&&split_up[8]==0x0d&&!atomic_load(&split_forwarded));
button_event(0x31,1);assert(workflow_button(chord_b2,workflow_ticket())==1);
assert(chop_host&&batch_surface()==sample_screen);
unsigned exits_before_chops=exit_starts;
#else
batch_start(1);
#endif
assert(batch&&chop_export&&slice_count==32);unsigned before_chop_preview=private_previews;
shift=0;button_event(0x6f,1);assert(!chop_auditions&&preview_private&&private_previews==before_chop_preview+1&&private_first==0&&private_last==1000);button_event(0x6f,0);assert(!chop_stops&&preview_source==0);
unsigned stops_click=browser_preview_stops;button_event(0x6f,1);assert(!preview_private&&preview_source==128&&browser_preview_stops>stops_click);button_event(0x6f,0);
unsigned char stopped_payload[80]={0};assert(!workflow_preview_worker(stopped_payload));
for(unsigned i=0;i<16;++i)workflow_pad(i,1,0,workflow_ticket());
button_event(0x36,1);assert(slice_page==1);workflow_pad(0,1,0,workflow_ticket());assert(!selected_sources[16]&&strstr(batch_message,"16 / 16"));
button_event(0x37,1);workflow_pad(0,1,0,workflow_ticket());button_event(0x36,1);workflow_pad(0,1,0,workflow_ticket());
batch_auto_bank=3;table();assert(batch_count==16);for(unsigned i=0;i<batch_count;++i)assert(destination[i]>=48&&destination[i]<64&&destination[i]!=slice_sound);
shift=0;button_event(0x6f,1);assert(!chop_auditions&&preview_private&&private_previews==before_chop_preview+2&&private_first==(int64_t)source[batch_row]*1000);button_event(0x6f,0);assert(!chop_stops&&preview_private);
button_event(0x36,1);assert(!preview_private&&preview_source==128);
destination[0]=slice_sound;assert(!review(&overwrite));arrange();assert(review(&overwrite));
snprintf(path,sizeof(path),"%s.chops",argv[2]);draw=fopen(path,"w");assert(draw);batch_paint((void*)1);fclose(draw);draw=NULL;
button_event(0x0e,1);assert(batch_confirm&&!chop_calls);button_event(0x0e,1);assert(batch_job&&!chop_calls);
workflow_tick();assert(!chop_calls); /* Progress must paint before queueing extraction. */
snprintf(path,sizeof(path),"%s.chop-progress",argv[2]);draw=fopen(path,"w");assert(draw);batch_paint((void*)1);fclose(draw);draw=NULL;
workflow_tick();assert(chop_calls==1&&batch_job);
test_progress=(struct batch_copy_progress){BATCH_JOB_DONE,16,16};workflow_tick();button_event(0x0e,1);
#if MPC_EXPERIMENTAL_CHOP_EXPORT
assert(!batch&&!batch_job&&!chop_host&&!batch_leaving&&exit_starts==exits_before_chops);
/* Cancel returns directly to Chop; no Copy modal exists in this workflow. */
FIELD(copy_controls,0x22a0,unsigned char)=1;workflow_sample_paint(sample_screen,(void*)1);
button_event(0x0d,1);assert(batch&&chop_host);button_event(0x0c,1);
assert(!batch&&!chop_host&&!batch_leaving&&exit_starts==exits_before_chops);
#else
complete_exit();
#endif
unsigned char event_note[56]={0};assert(current_program()==copy_program);
assert(workflow_event_preview(copy_program,event_note,2,1)==1&&event_preview_calls==1);
assert(!workflow_event_preview((void*)1,event_note,2,1)&&event_preview_calls==1);
workflow_event_preview_stop(1);assert(event_preview_stops==1&&!workflow_event_preview_pending());
FIELD(sample_screen,0,uintptr_t)=base+0x6e212a0;fault=0;
assert(workflow_mono_open(sample_screen));assert(mono&&workflow_owns(3,0)&&!workflow_owns(2,0x52));
FIELD(sample_screen,0x100,void*)=volume_registry;mono_labels=0;
workflow_sample_paint(sample_screen,(void*)1);
assert(mono_labels==63); /* Real Sample paint route must expose all choices. */
assert(workflow_light(mono_target%16,&light)&&((uint32_t)light==0xffffff||(uint32_t)light==0xff2020));
button_event(0x0c,1);assert(!mono&&!mono_calls);
assert(workflow_mono_open(sample_screen));mono_bank=1;workflow_pad(4,1,0,workflow_ticket());assert(mono_target==20);
button_event(0x0e,1);if(!mono_job){assert(mono_confirm==1&&!mono_calls);button_event(0x0e,1);}
assert(mono_job&&mono_calls==1&&mono_last_target==20&&mono_last_channel==1);
test_progress=(struct batch_copy_progress){BATCH_JOB_DONE,1,1};workflow_tick();button_event(0x0c,1);assert(!mono&&!mono_job);
puts("PASS mono destination workflow: Sample-host dialog paints Cancel/Left/Right and source/target; Cancel no job, target pads/banks, channel request, occupied confirmation, completion close and Play/Stop pass-through; backend is a receiver");
puts("PASS actual native File optional/current-directory getter and BrowserLogic root setter; selected-file property alone cannot navigate; reveal locates asynchronous native row for Sample and Project without loading; factory project search defaults to SD, K3 Internal/SD roots and XPJ-not-XPM filter; silent result-pad selection, native project confirmation and preview; bounded recursive search over4100 files, linked/hidden folders and inode cycle dedup; ordinary Chop Split gate and Sample108/148 offsets; Batch mapping/confirmation/paint; final GUI, listing/notification/property and audio callbacks remain fixtures");return 0;}
