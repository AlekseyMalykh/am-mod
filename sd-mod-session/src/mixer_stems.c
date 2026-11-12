#define _GNU_SOURCE
/* Song stems use the stock offline timeline and an independent Mixer.
 * Silent buses do not allocate files. Output contains Tracks A–H and
 * Master Direct when audible, alongside the native mixdown WAV. */
#include "mixer_stems.h"
#ifdef MPC_EARLY_STARTUP
#include "startup_project.h"
#endif

#include "mixer_live.h"
#include "mixer_archive.h"
#include "fx_native.h"
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define STEM_FILES 9u /* Tracks A-H and Master Direct */
#define OFFLINE_CAPACITY 8192u
struct stem_writer {FILE*file;uint64_t frames;};
struct stems_job {
    struct mixer_live*live;
    struct mixer_project project;
    struct fx_native_context fx;
    unsigned rate;
    uint64_t valid_blocks,missing_blocks,frames;
    struct stem_writer writer[STEM_FILES];
    char folder[1024];
    void *context,*native_writer;
    int failed;
};
static uintptr_t image;
static _Atomic unsigned armed;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static struct stems_job*job;            /* under mutex */
static _Thread_local void *song_context;
static void (*native_writer_close)(void*);
static _Atomic unsigned job_open;
static char last_folder[1024];
static _Atomic unsigned finished_count, failed_count;

static void put16(FILE*f,unsigned v){fputc((int)(v&255),f);fputc((int)((v>>8)&255),f);}
static void put32(FILE*f,uint32_t v){put16(f,v&0xffff);put16(f,v>>16);}
static void wav_header(FILE*f,unsigned rate,uint64_t frames)
{
    uint32_t data=(uint32_t)(frames*6);
    fwrite("RIFF",1,4,f);put32(f,36+data);fwrite("WAVEfmt ",1,8,f);put32(f,16);put16(f,1);put16(f,2);
    put32(f,rate);put32(f,rate*6);put16(f,6);put16(f,24);fwrite("data",1,4,f);put32(f,data);
}
static int stem_file_path(char*out,size_t size,const char*folder,const char*base_name,unsigned i)
{
    if(!out||!size||!folder||!base_name||i>=STEM_FILES)return 0;
    int n=i==8 ? snprintf(out,size,"%s/%s - Master.wav",folder,base_name) : snprintf(out,size,"%s/%s - track %c.wav",folder,base_name,'A'+(int)i);
    return n>0&&(size_t)n<size;
}
static int write_block(struct stem_writer*w,const float*l,const float*r,unsigned frames)
{
    if(!w->file)return 0;
    if(w->frames + frames > (UINT32_MAX-36u)/6u){
        fprintf(stderr,"STEMS WAV_4GB_LIMIT\n");return 0;
    }
    unsigned char buffer[6*256];unsigned done=0;
    while(done<frames){unsigned n=frames-done;if(n>256)n=256;
        for(unsigned i=0;i<n;++i)for(unsigned c=0;c<2;++c){
            float v=c?(r?r[done+i]:0.f):(l?l[done+i]:0.f);
            v=v==v?(v>1?1:v<-1?-1:v):0;
            int32_t s=(int32_t)lrintf(v*8388607.f);unsigned char*o=buffer+i*6+c*3;
            o[0]=(unsigned char)(s&255);o[1]=(unsigned char)((s>>8)&255);o[2]=(unsigned char)((s>>16)&255);}
        if(fwrite(buffer,6,n,w->file)!=n){
            fprintf(stderr,"STEMS WRITE_ERROR\n");return 0;
        }
        w->frames+=n;done+=n;}
    return 1;
}
static int write_stem(struct stems_job *j,unsigned bus,const struct mixer_pcm_view *view,unsigned count)
{
    struct stem_writer *w=j->writer+bus;
    if(!w->file){
        if(j->frames+count>(UINT32_MAX-36u)/6u)return 0;
        unsigned first=0;
        while(first<count&&fabsf(view->channel[0][first])<0.5f/8388607.f&&
              fabsf(view->channel[1][first])<0.5f/8388607.f)++first;
        if(first==count)return 1;
        char path[1100];
        const char *name=strrchr(j->folder,'/')+1;
        if(!stem_file_path(path,sizeof(path),j->folder,name,bus))return 0;
        w->file=fopen(path,"wbx");
        if(!w->file)return 0;
        wav_header(w->file,j->rate,0);
        /* Backfill the common timeline when a bus first becomes audible. */
        static const unsigned char silence[4096*6];
        while(w->frames<j->frames){
            uint64_t left=j->frames-w->frames;
            unsigned n=left>4096?4096:(unsigned)left;
            if(fwrite(silence,6,n,w->file)!=n)return 0;
            w->frames+=n;
        }
    }
    return write_block(w,view->channel[0],view->channel[1],count);
}
static int close_job(struct stems_job*j,int success)
{
    if(!j)return 0;
    unsigned files=0;
    for(unsigned i=0;i<STEM_FILES;++i)if(j->writer[i].file){
        ++files;
        FILE *f=j->writer[i].file;
        if(fseek(f,0,SEEK_SET))success=0;
        else wav_header(f,j->rate,j->writer[i].frames);
        if(fflush(f)||ferror(f)||fsync(fileno(f)))success=0;
        if(fclose(f))success=0;
    }
    if(!success&&j->folder[0]){
        const char *name=strrchr(j->folder,'/')+1;
        for(unsigned i=0;i<STEM_FILES;++i){
            char path[1100];
            if(stem_file_path(path,sizeof(path),j->folder,name,i))unlink(path);
        }
        rmdir(j->folder);
    }
    if(success&&!files){
        rmdir(j->folder);
        fprintf(stderr,"STEMS NO_AUDIBLE_TRACKS\n");
        success=0;
    }
    if(j->folder[0])fprintf(stderr,"STEMS %s folder=%s files=%u frames=%llu valid=%llu missing=%llu\n",
        success?"SAVED":"INCOMPLETE",j->folder,files,(unsigned long long)j->frames,
        (unsigned long long)j->valid_blocks,(unsigned long long)j->missing_blocks);
    mixer_live_destroy(j->live);free(j);
    return success;
}
/* Stock 2114c00 fills PositionInfo at context+8 before each Song block. */
static int stem_position(void*user,void*out,uint64_t*block)
{
    const struct stems_job*j=user;
    memcpy(out,(char*)j->context+8,88);
    *block=j->frames;
    return 1;
}
static void begin(void *context,void *writer,const char *path,unsigned rate,int with_fx)
{
    if(atomic_load(&job_open))return;
    char full[1024];
    if(!path||path[0]!='/'||strnlen(path,sizeof(full))>=sizeof(full)){
        fprintf(stderr,"STEMS INVALID_EXPORT_PATH\n");return;
    }
#ifdef MPC_EARLY_STARTUP
    static const char export_path[]="/data/UserContent/Samples/User/Export/";
    int internal_ok=!strncmp(path,export_path,sizeof(export_path)-1)&&startup_song_export_ready();
    if(!internal_ok&&!startup_song_export_external(path)){
        fprintf(stderr,"STEMS SD_EXPORT_UNAVAILABLE path=%s prefix_ok=%d\n",path,!strncmp(path,export_path,sizeof(export_path)-1));return;
    }
#endif
    snprintf(full,sizeof(full),"%s",path);
    char*slash=strrchr(full,'/'),*dot=strrchr(full,'.');
    if(!slash||!slash[1]){fprintf(stderr,"STEMS INVALID_EXPORT_NAME\n");return;}
    if(dot&&dot>slash)*dot=0;
    slash=strrchr(full,'/');
    const char*base_name=slash+1;
    if(!*base_name||strlen(full)>840||strlen(base_name)>100){fprintf(stderr,"STEMS NAME_TOO_LONG\n");return;}
    struct stems_job*j=calloc(1,sizeof(*j));if(!j)return;
    uint64_t revision=0;unsigned mode=0;
    if(!mixer_archive_snapshot(&j->project,&revision,&mode)||!mixer_project_valid(&j->project)){
        fprintf(stderr,"STEMS MIXER_SNAPSHOT_UNAVAILABLE\n");free(j);return;
    }
    struct fx_backend backend,*processors=NULL;
    int needs_native=0;
    for(unsigned i=0;i<=MIXER_TRACKS;++i){
        struct mixer_channel*ch=i==MIXER_TRACKS?&j->project.master:j->project.track+i;
        if(!with_fx||ch->fx_bypass)ch->fx.count=0;
        for(unsigned slot=0;slot<ch->fx.count;++slot){
            if(ch->fx.stage[slot].kind==FX_STAGE_NATIVE)needs_native=1;
        }
    }
    if(needs_native){
        if(!fx_native_application_formats(image,&j->fx)){
            fprintf(stderr,"STEMS FX_CONTEXT_UNAVAILABLE\n");free(j);return;
        }
        j->context=context;
        j->fx.position=stem_position;j->fx.position_user=j;
        uint64_t block;
        stem_position(j,j->fx.initial_position,&block);
        memcpy(&j->fx.bpm,j->fx.initial_position,8);
        memcpy(&j->fx.numerator,j->fx.initial_position+8,4);
        memcpy(&j->fx.denominator,j->fx.initial_position+12,4);
        fx_native_backend(&j->fx,&backend);processors=&backend;
    }
    j->rate=rate>=8000&&rate<=192000?rate:44100;
    const struct mixer_live_ops*ops=mixer_live_native_ops();
    enum mixer_bus_result error;
    j->live=ops?mixer_live_prepare(&j->project,0,j->rate,OFFLINE_CAPACITY,1,
                                  (void*)1,(void*)1,ops,processors,&error):NULL;
    if(!j->live){fprintf(stderr,"STEMS OFFLINE_PREPARE_FAILED\n");free(j);return;}
    snprintf(j->folder,sizeof(j->folder),"%s",full);
    /* Never overwrite a prior export. The whole folder must be new. */
    if(mkdir(j->folder,0777)){
        fprintf(stderr,"STEMS FOLDER_EXISTS_OR_UNWRITABLE %s errno=%d\n",j->folder,errno);
        j->folder[0]=0;
        close_job(j,0);return;
    }
    j->context=context;
    j->native_writer=writer;
    fprintf(stderr,"STEMS BEGIN folder=%s rate=%u\n",j->folder,j->rate);
    pthread_mutex_lock(&mutex);
    /* Do not steal an active export or overwrite an unfinished job. */
    if(job){pthread_mutex_unlock(&mutex);close_job(j,0);return;}
    job=j;
    snprintf(last_folder,sizeof(last_folder),"%s",j->folder);
    atomic_store(&job_open,1);
    pthread_mutex_unlock(&mutex);
}
/* The stock render frame hook provides the actual export writer and
 * timeline. Source 0 is Song, with a single primary mixdown file. */
void mixer_stems_song_frame(void *context,void *writer,const char *path,unsigned rate)
{
    song_context=context;
    if(!writer)return;
    unsigned request=atomic_exchange(&armed,0);
    if(!request)return;
    begin(context,writer,path,rate,request==2);
    if(!atomic_load(&job_open))atomic_fetch_add(&failed_count,1);
}
int mixer_stems_available(void){return image!=0;}
void mixer_stems_disarm(void){
    unsigned was_armed=atomic_exchange_explicit(&armed,0u,memory_order_acq_rel);
    if(was_armed)fprintf(stderr,"STEMS DISARM previous=1\n");
}
void mixer_stems_arm(int with_fx){if(mixer_stems_available())atomic_store(&armed,with_fx?2u:1u);fprintf(stderr,"STEMS ARMED available=%d\n",mixer_stems_available());}
int mixer_stems_armed(void){return atomic_load(&armed)!=0;}
const char*mixer_stems_last_folder(void){return last_folder;}
unsigned mixer_stems_finished(void){return atomic_load(&finished_count);}
unsigned mixer_stems_failed(void){return atomic_load(&failed_count);}
int mixer_stems_offline_block(void*engine,void*clock,unsigned frames,uintptr_t program_vptr)
{
    if(!song_context||!atomic_load(&job_open))return 0;
    pthread_mutex_lock(&mutex);
    struct stems_job*j=job;
    if(!j||j->context!=song_context){
        pthread_mutex_unlock(&mutex);return 0;
    }
    if(!frames||frames>OFFLINE_CAPACITY){j->failed=1;pthread_mutex_unlock(&mutex);return 0;}
    /* Native writes only the remaining frames from the last render block. */
    uint64_t remaining;
    memcpy(&remaining,(char*)song_context+0x9f8,sizeof(remaining));
    unsigned count=remaining<frames?(unsigned)remaining:frames;
    mixer_live_rebind(j->live,engine,NULL);
    int applied=mixer_live_render_offline(j->live,engine,clock,frames,program_vptr);
    int valid=applied;
    for(unsigned bus=0;bus<STEM_FILES;++bus){
        struct mixer_pcm_view view;
        if(!applied||!mixer_live_bus_view(j->live,bus,&view)||view.frames!=frames){
            valid=0;
            continue;
        }
        if(!j->failed)j->failed=!write_stem(j,bus,&view,count);
    }
    j->frames+=count;
    if(valid)++j->valid_blocks;
    else ++j->missing_blocks;
    pthread_mutex_unlock(&mutex);
    return 1;
}
static void writer_close(void *writer)
{
    if(!song_context||!atomic_load(&job_open)){
        native_writer_close(writer);
        return;
    }
    pthread_mutex_lock(&mutex);
    struct stems_job *j=job;
    if(!j||j->native_writer!=writer||j->context!=song_context){
        pthread_mutex_unlock(&mutex);
        native_writer_close(writer);
        return;
    }
    uint64_t remaining,position;
    memcpy(&remaining,(char*)song_context+0x9f8,sizeof(remaining));
    memcpy(&position,(char*)writer+0xe0,sizeof(position));
    unsigned cancelled=__atomic_load_n((unsigned char*)song_context+0xa18,__ATOMIC_ACQUIRE);
    native_writer_close(writer);
    if(close_job(j,!cancelled&&!remaining&&position==j->frames&&
                    !j->failed&&j->valid_blocks&&!j->missing_blocks))
        atomic_fetch_add(&finished_count,1);
    else atomic_fetch_add(&failed_count,1);
    job=NULL;
    song_context=NULL;
    atomic_store(&job_open,0);
    pthread_mutex_unlock(&mutex);
}

int mixer_stems_install(uintptr_t base)
{
    if(image)return image==base;
    const uintptr_t entry=base+0x18b4500;
    const uint32_t expected[6]={0xd503233f,0xd101c3ff,0xa9017bfd,0x910043fd,0xa90463f7,0xaa0003f7};
    if(sysconf(_SC_PAGESIZE)!=4096||memcmp((void*)entry,expected,sizeof(expected)))return 0;
    unsigned char *memory=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(memory==MAP_FAILED)return 0;
    /* BTI, six PC-independent instructions, absolute continuation. */
    uint32_t code[10]={0xd503245f};
    memcpy(code+1,expected,sizeof(expected));
    code[7]=0x58000070;
    code[8]=0xd61f0200;
    uintptr_t next=entry+24;
    memcpy(memory,code,sizeof(code));
    memcpy(memory+40,&next,8);
    __builtin___clear_cache((char*)memory,(char*)memory+48);
    if(mprotect(memory,4096,PROT_READ|PROT_EXEC)){munmap(memory,4096);return 0;}
    native_writer_close=(void*)memory;
    void *page=(void*)(entry&~(uintptr_t)4095);
    if(mprotect(page,4096,PROT_READ|PROT_WRITE)){munmap(memory,4096);return 0;}
    uint32_t jump[4]={0xd503245f,0x58000070,0xd61f0200,0xd503201f};
    void *target=writer_close;
    memcpy((void*)entry,jump,sizeof(jump));
    memcpy((void*)(entry+16),&target,sizeof(target));
    __builtin___clear_cache((char*)entry,(char*)entry+24);
    if(mprotect(page,4096,PROT_READ|PROT_EXEC))_exit(121);
    image=base;
    fprintf(stderr,"MIXER_STEMS INSTALLED\n");
    return 1;
}
