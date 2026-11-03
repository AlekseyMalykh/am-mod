#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <dlfcn.h>
#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <ucontext.h>
static char metadata[1024];static uint64_t fake_ms=100;
static int recovery_clock(clockid_t id,struct timespec*t){(void)id;t->tv_sec=fake_ms/1000;t->tv_nsec=(fake_ms%1000)*1000000;return 0;}
#define PROJECT_RECOVERY_STORAGE metadata
#define clock_gettime recovery_clock
#include "src/project_recovery.c"
#undef clock_gettime
#include "src/music_archive.h"
#include "src/music_settings.h"
#include "src/compressor.h"
static uintptr_t base,guard=0x123456;
static const char*phase="setup";
static void fault(int sig,siginfo_t*i,void*ctx){ucontext_t*u=ctx;fprintf(stderr,"FAIL recovery phase=%s sig=%d pc=%lx lr=%lx address=%p\n",phase,sig,(unsigned long)(u->uc_mcontext.pc-base),(unsigned long)(u->uc_mcontext.regs[30]-base),i->si_addr);_exit(1);}
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
            const char *name=(void*)(memory+str+s->st_name);void *fn=s->st_shndx!=SHN_UNDEF?(void*)(base+s->st_value):dlsym(RTLD_DEFAULT,name);
            if(fn) *out=(uintptr_t)fn+r->r_addend;
        }
    }
    fclose(f);F((void*)base,0x7164398,void*)=&guard;
}

#define N(o,T) ((T)(base+(o)))
static unsigned char saver[0x200],services[0x80],things[0x1400],preferences[0x200],save_screen[0x1200];
static void*native_saver=saver,*modal;static unsigned invalidations,field_count;static char saved_payload[PROJECT_RECOVERY_BYTES],saved_settings[128],captions[2048];
static void stub(uintptr_t off,void*fn){uint32_t words[]={0x58000050,0xd61f0200};memcpy((void*)(base+off),words,8);memcpy((void*)(base+off+8),&fn,8);}
static void*top_modal(int index){assert(index==0);return modal;}
static void*service_entry(void*p,const uintptr_t*key){assert(p==services&&*key==base+0x6e0d598);return &native_saver;}
static void invalidated(void*p,uint64_t a,uint64_t b){assert(p==save_screen&&!a&&!b);++invalidations;}
static int shown(void*p){return p==save_screen;}
static int unblocked(void*p){assert(p==save_screen);return 0;}
static unsigned name_seeds,storage_seeds,rebuilds;
static void seed_name(void*p,const uint64_t*s,int notify){assert(p==save_screen+0x9f8&&notify==1&&!strcmp((const char*)*s,"TestB"));++name_seeds;}
static void rebuild_name(void*p){assert(p==save_screen+0x9f8);++rebuilds;}
static void seed_storage(void*p,float v){assert(p==save_screen&&v==1.f);++storage_seeds;}
static void colour(void*g,uint32_t c){(void)g;(void)c;}
static void rectangle(void*g,int x,int y,int w,int h){(void)g;assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=320&&y+h<=240);}
static void font_make(uint64_t*p,int type,float size){assert(!type&&size==12);*p=0;}
static void font_use(void*g,const uint64_t*p){(void)g;assert(!*p);}
static void font_end(void*p){(void)p;}
static void text(void*g,const uint64_t*s,int x,int y,int w,int h,int flags,int lines,float scale){(void)g;(void)x;(void)y;(void)w;(void)h;(void)flags;(void)lines;(void)scale;const char*p=(const char*)*s;assert(strlen(captions)+strlen(p)+2<sizeof(captions));strcat(captions,p);strcat(captions,"\n");}
static void set_string(void*p,const char*s){uint64_t value=0;N(0xf535c0,void(*)(uint64_t*,const char*))(&value,s);N(0xf1c6f0,void(*)(void*,const uint64_t*))(p,&value);N(0xf0a290,void(*)(uint64_t*))(&value);}
static const char*string_at(void*p){return F(p,0,const char*);}
static void init_property(void*p){memset(p,0,0x58);F(p,8,void*)=F(p,16,void*)=(char*)p+8;N(0xf1c620,void(*)(void*))((char*)p+0x50);}
static void write_field(void*archive,const char*key,const void*view){assert(archive==services);++field_count;const char*data=F(view,0,const char*);size_t size=F(view,8,size_t);if(!strcmp(key,"mpcSampleModRecoveryV1")){assert(size<sizeof(saved_payload));memcpy(saved_payload,data,size);saved_payload[size]=0;}else if(!strcmp(key,"mpcSampleModSettingsV1")){assert(size<sizeof(saved_settings));memcpy(saved_settings,data,size);saved_settings[size]=0;}else assert(!strcmp(key,"mpcSampleModV1"));}
static void save_original(void*p,void*a){assert(p==things&&a==services);}
static void load_original(void*p,void*a){(void)p;(void)a;}
static unsigned char archive_layers[5][0x180],frame[0x38],member[96],serial_data[0x80];static int read_ok=1;static const char*settings_input;
static int read_original(void*p,void*j){assert(p==serial_data&&j==services);music_archive_load(p,archive_layers,load_original);return read_ok;}
static void load_metadata(const char*p,int success){
 memset(archive_layers,0,sizeof(archive_layers));memset(frame,0,sizeof(frame));memset(member,0,sizeof(member));
 for(unsigned i=0;i<4;++i)F(archive_layers[i],8,void*)=archive_layers[i+1];
 F(archive_layers[4],0x150,void*)=frame+0x38;F(frame,0x30,unsigned)=1;
 unsigned count=0;
 if(p){F(member,8,const char*)="mpcSampleModRecoveryV1";F(member,0x16,unsigned short)=1<<10;F(member,24+8,const char*)=p;F(member,24+0x16,unsigned short)=1<<10;++count;}
 if(settings_input){unsigned char*m=member+48*count;F(m,8,const char*)="mpcSampleModSettingsV1";F(m,0x16,unsigned short)=1<<10;F(m,32,const char*)=settings_input;F(m,46,unsigned short)=1<<10;++count;}
 if(count){F(frame,0,void*)=member;F(frame,8,void*)=member+48*count;}
 F(serial_data,0x28,const char*)="C Major";read_ok=success;assert(music_archive_read(serial_data,services,read_original)==success);
}
static int button(unsigned code,unsigned down){unsigned char event[16]={0};event[8]=code;event[9]=down;return project_recovery_save_button(base,save_screen,event);}
static void*async_copy_serializer(void*p){assert(project_recovery_copy_payload());music_archive_save(things,services,save_original);return p;}
int main(int argc,char**argv){
 assert(argc==2);setbuf(stdout,NULL);struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);load_elf(argv[1],0x5000000000ULL);
 stub(0x10f87a0,top_modal);stub(0x143ad50,service_entry);stub(0x1149880,invalidated);stub(0x112fd50,shown);stub(0x112f6b0,unblocked);
 stub(0x102d5e0,colour);stub(0x102e000,rectangle);stub(0x1082810,font_make);stub(0x102dca0,font_use);stub(0x1070d60,font_end);stub(0x10817b0,text);stub(0x1d27d90,write_field);
 stub(0x11c4ec0,seed_name);stub(0x15bdb10,rebuild_name);stub(0x16480e0,seed_storage);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 F(saver,0,uintptr_t)=base+0x6d8cc00;F(saver,0x90,void*)=things;F(saver,0x50,void*)=preferences;F(things,0,uintptr_t)=base+0x6ecc648;
 init_property(things+0x908);init_property(things+0x960);init_property(things+0x9b8);N(0xf1c620,void(*)(void*))(preferences+0xa8);
 char temporary[]="/media/am-mod-recovery-tests/am-recovery-XXXXXX";assert(mkdtemp(temporary));char root[1024],original[1200],recovery[1200],data[1200];
 snprintf(root,sizeof(root),"%s/MPC-Sample",temporary);assert(!mkdir(root,0700));snprintf(original,sizeof(original),"%s/Projects",root);assert(!mkdir(original,0700));
 snprintf(original,sizeof(original),"%s/Projects/TestB.xpj",root);snprintf(recovery,sizeof(recovery),"%s/Projects/Mod-Exit-Last.xpj",root);snprintf(data,sizeof(data),"%s/Projects/Mod-Exit-Last_[ProjectData]",root);
 FILE*f=fopen(original,"wb");assert(f);fputs("Original B unchanged",f);fclose(f);
 strcpy(metadata,"/tmp/am-recovery-meta-XXXXXX");int fd=mkstemp(metadata);assert(fd>=0);assert(write(fd,root,strlen(root))==(ssize_t)strlen(root));close(fd);
 char encoded[PROJECT_RECOVERY_BYTES],name[129],relative[1024];
 assert(project_recovery_encode("TestB","Projects/TestB.xpj",encoded,sizeof(encoded)));
 assert(project_recovery_decode(encoded,name,sizeof(name),relative,sizeof(relative))&&!strcmp(name,"TestB")&&!strcmp(relative,"Projects/TestB.xpj"));
 const char*bad[]={"../TestB.xpj","Projects/../TestB.xpj","Projects//TestB.xpj","/Projects/TestB.xpj","Projects/Mod-Exit-Last.xpj","Projects/TestB.xpj/","Projects\\TestB.xpj"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);++i)assert(!project_recovery_encode("TestB",bad[i],encoded,sizeof(encoded)));
 assert(!project_recovery_encode("../Bad","",encoded,sizeof(encoded)));assert(!project_recovery_decode("AMR1\n00\n",name,sizeof(name),relative,sizeof(relative)));
 assert(project_recovery_encode("TestB","Projects/TestB.xpj",encoded,sizeof(encoded)));
 phase="native strings";music_archive_init(base);music_reset();set_string(things+0x9b0,original);set_string(things+0xa08,data);set_string(preferences+0xa8,"/data/TempProjects/Temp-Project.xpj");
 phase="copy archive";
 /* UI observes B before native shutdown redirects identity to Temp-Project. */
 project_recovery_tick(base,services);
 set_string(things+0x9b0,"/data/TempProjects/Temp-Project.xpj");
 project_recovery_capture(base,saver);project_recovery_begin_copy();assert(!strcmp(project_recovery_copy_payload(),encoded));project_recovery_end_copy();
 set_string(things+0x9b0,original);
 project_recovery_capture(base,saver);assert(!project_recovery_copy_payload());project_recovery_begin_copy();field_count=0;music_archive_save(things,services,save_original);assert(field_count==3&&!strcmp(saved_payload,encoded));project_recovery_end_copy();
 field_count=0;saved_payload[0]=0;music_archive_save(things,services,save_original);assert(field_count==2&&!saved_payload[0]);
 project_recovery_begin_copy();pthread_t writer;assert(!pthread_create(&writer,NULL,async_copy_serializer,NULL));assert(!pthread_join(writer,NULL));assert(!strcmp(saved_payload,encoded));project_recovery_end_copy();assert(!project_recovery_copy_payload());
 puts("PASS recovery metadata added only to exit-copy serialization; ordinary Save stays a normal project, bounded relative paths reject malformed/traversal inputs");
 set_string(things+0x9b0,recovery);load_metadata(encoded,1);
 modal=(void*)1;project_recovery_tick(base,services);assert(!strcmp(string_at(things+0x9b0),recovery));modal=NULL;
 project_recovery_tick(base,services);fake_ms+=2499;project_recovery_tick(base,services);assert(!strcmp(string_at(things+0x9b0),recovery));++fake_ms;
 unsigned char sequences[0x300];memcpy(sequences,things+0x1100,sizeof(sequences));project_recovery_tick(base,services);
 assert(!strcmp(string_at(things+0x9b0),original)&&!strcmp(string_at(things+0x958),"TestB")&&!strcmp(string_at(preferences+0xa8),"/data/TempProjects/Temp-Project.xpj"));assert(!strcmp(string_at(things+0xa08),data));assert(!memcmp(sequences,things+0x1100,sizeof(sequences)));
 assert(strstr(project_recovery_notice(),"TestB"));assert(first_save);project_recovery_tick(base,services);assert(first_save);
 puts("PASS actual native File/String property setter restores project path/name after quiet load; native Temp-Project autosave destination, recovery sample directory, sequences and original file remain unchanged");
 F(save_screen,0,uintptr_t)=base+0x6e1faa0;F(save_screen,0x9f8,uintptr_t)=base+0x6e18e20;
 project_recovery_save_paint(base,save_screen,NULL);assert(name_seeds==1&&storage_seeds==1&&rebuilds==1);
 project_recovery_save_paint(base,save_screen,NULL);assert(name_seeds==1&&storage_seeds==1); /* Subsequent edits survive paints. */
 assert(!button(0x0c,1)&&!button(0x0c,0)&&!save_seeded&&first_save); /* Native Cancel. */
 project_recovery_save_paint(base,save_screen,NULL);assert(name_seeds==2&&storage_seeds==2&&save_seeded);
 assert(button(0x0e,1)&&button(0x0e,1)&&button(0x0e,0));assert(first_save&&save_prompt);
 captions[0]=0;project_recovery_save_paint(base,save_screen,NULL);assert(strstr(captions,"Review")&&strstr(captions,"Existing project may be replaced."));
 assert(button(0x0c,1)&&button(0x0c,0)&&first_save&&!save_prompt);assert(button(0x0e,1)&&button(0x0e,0));assert(!button(0x0e,1)&&!button(0x0e,0)&&!first_save&&!save_prompt);
 assert(invalidations==3);puts("PASS first recovered Save requires separate real press/release confirmation, duplicate down cannot approve; Review cancels, final native button pair and subsequent ordinary Save pass through");
 project_recovery_capture(base,saver);project_recovery_begin_copy();assert(!strcmp(project_recovery_copy_payload(),encoded));project_recovery_end_copy();
 load_metadata(NULL,0);assert(pending[0]);load_metadata(NULL,1);project_recovery_tick(base,services);assert(!pending[0]&&!first_save&&!project_recovery_notice());
 set_string(things+0x9b0,recovery);project_recovery_tick(base,services);assert(!strcmp(string_at(things+0x9b0),recovery));
 puts("PASS second exit copy retains original identity; failed native read preserves recovery state; ordinary/legacy project without metadata clears it without guessing a name");
 assert(music_set_service_colour(MUSIC_SERVICE_SELECT,4)&&music_set_mod_setting(MUSIC_SETTING_WAVE,1)&&music_set_mod_setting(MUSIC_SETTING_SPECTRUM,1)&&music_set_mod_setting(MUSIC_SETTING_BLINK,3));
 compressor_knob(0,110);compressor_knob(1,91);compressor_knob(2,127);compressor_shift_knob(0,12);compressor_shift_knob(1,126);compressor_shift_knob(2,95);compressor_warm();
 assert(compressor_project_restore(compressor_project_snapshot()|1));
 uint64_t display=music_mod_settings_snapshot(),comp=compressor_project_snapshot();
 music_archive_save(things,services,save_original);assert(strlen(saved_settings)==33);
 assert(music_mod_restore_recommended(2)&&music_mod_restore_recommended(1)&&music_mod_restore_recommended(3));music_reset();assert(!compressor_type());
 settings_input=saved_settings;load_metadata(NULL,0);assert(compressor_project_snapshot()!=comp&&music_mod_settings_snapshot()!=display);
 load_metadata(NULL,1);assert(compressor_project_snapshot()==comp&&music_mod_settings_snapshot()==display&&compressor_type()==COMP_OPTO);
 struct comp_config restored;compressor_config(&restored);assert(restored.warm&&restored.gain==6);
 uint64_t prior_comp=compressor_project_snapshot();assert(!compressor_project_restore(UINT64_MAX)&&compressor_project_snapshot()==prior_comp);
 settings_input="AMS1 broken";load_metadata(NULL,1);assert(!compressor_type()&&music_mod_settings_snapshot()==display);
 settings_input=NULL;load_metadata(NULL,1);assert(!compressor_type());music_reset();assert(!compressor_type());
 puts("PASS project settings capsule restores palette/tints/pulse and Opto Type/threshold/ratio/gain/attack/release/knee/Warm only after successful native load; malformed/legacy projects reset Opto safely, New Project clears compressor; native Duck/Bypass untouched");
 f=fopen(original,"rb");assert(f);char contents[64]={0};assert(fread(contents,1,sizeof(contents)-1,f)>0);fclose(f);assert(!strcmp(contents,"Original B unchanged"));
 project_recovery_reset();set_string(things+0x9b0,"/data/TempProjects/Temp-Project.xpj");project_recovery_capture(base,saver);project_recovery_begin_copy();
 assert(project_recovery_decode(project_recovery_copy_payload(),name,sizeof(name),relative,sizeof(relative))&&!strcmp(name,"Recovered Project")&&!relative[0]);project_recovery_end_copy();
 puts("PASS shutdown Temp-Project handoff retains the previously observed UI identity; successful load and New Project invalidate old identity; native display is basename only");
 N(0xf0a290,void(*)(void*))(things+0x958);N(0xf0a290,void(*)(void*))(things+0x9b0);N(0xf0a290,void(*)(void*))(things+0xa08);N(0xf0a290,void(*)(void*))(preferences+0xa8);
 assert(!unlink(original)&&!unlink(metadata));snprintf(original,sizeof(original),"%s/Projects",root);assert(!rmdir(original)&&!rmdir(root)&&!rmdir(temporary));
 assert(!munmap((void*)base,0x7500000));return 0;
}
