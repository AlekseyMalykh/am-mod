#define _GNU_SOURCE
#include "screen_mirror_frame.h"
#include <dlfcn.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/dma-buf.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

struct mirror_buffer {
    int fd,read_fd,dma_fd;
    uint32_t id,handle,pitch;
    uint8_t bits;
};

static struct mirror_buffer buffers[8];
struct mirror_display {int fd;uint32_t crtc;};
static struct mirror_display displays[4];
static unsigned char frame[MIRROR_BMP_BYTES];
static unsigned char published_frame[MIRROR_BMP_BYTES];
static int published_valid;
static uint64_t last_frame;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t native_once=PTHREAD_ONCE_INIT;
static pthread_once_t worker_once=PTHREAD_ONCE_INIT;
static int (*native_add)(int,uint32_t,uint32_t,uint8_t,uint8_t,uint32_t,uint32_t,uint32_t*);
static int (*native_remove)(int,uint32_t);
static int (*native_flip)(int,uint32_t,uint32_t,uint32_t,void*);
static int (*native_set)(int,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t*,int,void*);

static void resolve_native(void)
{
    native_add=dlsym(RTLD_NEXT,"drmModeAddFB");
    native_remove=dlsym(RTLD_NEXT,"drmModeRmFB");
    native_flip=dlsym(RTLD_NEXT,"drmModePageFlip");
    native_set=dlsym(RTLD_NEXT,"drmModeSetCrtc");
}

static void le32(unsigned char *out,uint32_t value)
{
    for(unsigned i=0;i<4;++i)out[i]=(unsigned char)(value>>(8*i));
}

static void encode_frame(const unsigned char *pixels,unsigned pitch,unsigned bits)
{
    frame[0]='B';frame[1]='M';le32(frame+2,sizeof(frame));le32(frame+10,54);
    le32(frame+14,40);le32(frame+18,MIRROR_WIDTH);
    le32(frame+22,(uint32_t)-(int32_t)MIRROR_HEIGHT);
    frame[26]=1;frame[28]=24;le32(frame+34,MIRROR_WIDTH*MIRROR_HEIGHT*3);
    for(unsigned y=0;y<MIRROR_HEIGHT;++y){
        const unsigned char *source=pixels+(size_t)y*pitch;
        unsigned char *target=frame+54+y*MIRROR_WIDTH*3;
        for(unsigned x=0;x<MIRROR_WIDTH;++x){
            if(bits==16){
                unsigned rgb=source[0]|((unsigned)source[1]<<8);
                *target++=(unsigned char)((rgb&31)*255/31);
                *target++=(unsigned char)(((rgb>>5)&63)*255/63);
                *target++=(unsigned char)(((rgb>>11)&31)*255/31);
            }else{
                *target++=source[0];*target++=source[1];*target++=source[2];
            }
            source+=bits/8;
        }
    }
}

static void publish_frame(void)
{
    /* A static LCD must not generate RAM filesystem writes at capture rate. */
    if(published_valid&&!memcmp(frame,published_frame,sizeof(frame))&&
       !access(SCREEN_MIRROR_FRAME,F_OK))return;
    static const char staging[]=SCREEN_MIRROR_FRAME ".tmp";
    int fd=open(staging,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_CLOEXEC,0600);
    if(fd<0)return;
    size_t done=0;
    while(done<sizeof(frame)){
        ssize_t n=write(fd,frame+done,sizeof(frame)-done);
        if(n<0&&errno==EINTR)continue;
        if(n<=0)break;
        done+=(size_t)n;
    }
    int closed=close(fd);
    if(closed||done!=sizeof(frame)||access(SCREEN_MIRROR_FLAG,F_OK)||
       rename(staging,SCREEN_MIRROR_FRAME))unlink(staging);
    else {memcpy(published_frame,frame,sizeof(frame));published_valid=1;}
}

static void capture_buffer(int fd,uint32_t crtc)
{
    int saved=errno;
    if(pthread_mutex_trylock(&lock))return;
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC,&time);
    uint64_t now=(uint64_t)time.tv_sec*1000+(unsigned long)time.tv_nsec/1000000;
    if(crtc&&now-last_frame>=40){
        if(access(SCREEN_MIRROR_FLAG,F_OK))goto done;
        int query=-1;
        for(unsigned i=0;i<sizeof(buffers)/sizeof(buffers[0]);++i)
            if(buffers[i].id&&buffers[i].fd==fd&&buffers[i].read_fd>=0){query=buffers[i].read_fd;break;}
        if(query<0)goto done;
        /* A successful PageFlip queues a future buffer. Only the framebuffer
         * actually selected by KMS is a candidate for a complete screenshot. */
        struct drm_mode_crtc current={.crtc_id=crtc};
        if(ioctl(query,DRM_IOCTL_MODE_GETCRTC,&current)||!current.mode_valid)goto done;
        for(unsigned i=0;i<sizeof(buffers)/sizeof(buffers[0]);++i){
            struct mirror_buffer *b=&buffers[i];
            if(b->id!=current.fb_id||b->fd!=fd||b->read_fd<0)continue;
            if(b->dma_fd<0){
                struct drm_prime_handle prime={.handle=b->handle,.flags=DRM_CLOEXEC};
                if(ioctl(b->read_fd,DRM_IOCTL_PRIME_HANDLE_TO_FD,&prime))break;
                b->dma_fd=prime.fd;
            }
            /* RGA renders into DMA memory. mmap alone can expose stale CPU
             * cache lines: new glyphs over an old menu, unlike the LCD. */
            struct dma_buf_sync sync={.flags=DMA_BUF_SYNC_START|DMA_BUF_SYNC_READ};
            if(ioctl(b->dma_fd,DMA_BUF_IOCTL_SYNC,&sync))break;
            struct drm_mode_map_dumb offset={.handle=b->handle};
            int complete=0;
            if(!ioctl(b->read_fd,DRM_IOCTL_MODE_MAP_DUMB,&offset)){
                size_t size=(size_t)b->pitch*MIRROR_HEIGHT;
                const unsigned char *pixels=mmap(NULL,size,PROT_READ,MAP_SHARED,b->read_fd,(off_t)offset.offset);
                if(pixels!=MAP_FAILED){
                    encode_frame(pixels,b->pitch,b->bits);
                    munmap((void*)pixels,size);
                    struct drm_mode_crtc after={.crtc_id=crtc};
                    complete=!ioctl(b->read_fd,DRM_IOCTL_MODE_GETCRTC,&after)&&after.mode_valid&&after.fb_id==b->id;
                }
            }
            sync.flags=DMA_BUF_SYNC_END|DMA_BUF_SYNC_READ;
            if(ioctl(b->dma_fd,DMA_BUF_IOCTL_SYNC,&sync))complete=0;
            if(complete){last_frame=now;publish_frame();}
            break;
        }
    }
done:
    pthread_mutex_unlock(&lock);
    errno=saved;
}

static void*mirror_worker(void*unused)
{
    (void)unused;
    for(;;){
        if(!access(SCREEN_MIRROR_FLAG,F_OK)){
            struct mirror_display current[4];
            pthread_mutex_lock(&lock);memcpy(current,displays,sizeof(current));pthread_mutex_unlock(&lock);
            for(unsigned i=0;i<4;++i)if(current[i].crtc)capture_buffer(current[i].fd,current[i].crtc);
        }
        usleep(40000);
    }
    return NULL;
}
static void start_worker(void){pthread_t worker;if(!pthread_create(&worker,NULL,mirror_worker,NULL))pthread_detach(worker);}
static void remember_display(int fd,uint32_t crtc)
{
    pthread_mutex_lock(&lock);
    for(unsigned i=0;i<4;++i)if(!displays[i].crtc||(displays[i].fd==fd&&displays[i].crtc==crtc)){
        displays[i]=(struct mirror_display){fd,crtc};break;
    }
    pthread_mutex_unlock(&lock);
    pthread_once(&worker_once,start_worker);
}

int drmModeAddFB(int fd,uint32_t width,uint32_t height,uint8_t depth,uint8_t bits,
                 uint32_t pitch,uint32_t handle,uint32_t *id)
{
    pthread_once(&native_once,resolve_native);
    if(!native_add){errno=ENOSYS;return -1;}
    int result=native_add(fd,width,height,depth,bits,pitch,handle,id);
    if(!result&&id&&width==MIRROR_WIDTH&&height==MIRROR_HEIGHT&&
       ((bits==16&&depth==16)||((bits==24||bits==32)&&(depth==24||depth==32)))&&
       pitch>=width*(bits/8)&&pitch<=16384){
        pthread_mutex_lock(&lock);
        for(unsigned i=0;i<sizeof(buffers)/sizeof(buffers[0]);++i){
            if(!buffers[i].id||(buffers[i].fd==fd&&buffers[i].id==*id)){
                int copy=fcntl(fd,F_DUPFD_CLOEXEC,0);
                if(buffers[i].id){
                    if(buffers[i].read_fd>=0)close(buffers[i].read_fd);
                    if(buffers[i].dma_fd>=0)close(buffers[i].dma_fd);
                }
                buffers[i]=(struct mirror_buffer){fd,copy,-1,*id,handle,pitch,bits};
                break;
            }
        }
        pthread_mutex_unlock(&lock);
    }
    return result;
}

int drmModeRmFB(int fd,uint32_t id)
{
    pthread_once(&native_once,resolve_native);
    if(!native_remove){errno=ENOSYS;return -1;}
    pthread_mutex_lock(&lock);
    for(unsigned i=0;i<sizeof(buffers)/sizeof(buffers[0]);++i)
        if(buffers[i].id&&buffers[i].fd==fd&&buffers[i].id==id){
            if(buffers[i].read_fd>=0)close(buffers[i].read_fd);
            if(buffers[i].dma_fd>=0)close(buffers[i].dma_fd);
            buffers[i].id=0;
        }
    pthread_mutex_unlock(&lock);
    return native_remove(fd,id);
}

int drmModePageFlip(int fd,uint32_t crtc,uint32_t id,uint32_t flags,void *data)
{
    pthread_once(&native_once,resolve_native);
    if(!native_flip){errno=ENOSYS;return -1;}
    int result=native_flip(fd,crtc,id,flags,data);
    if(!result)remember_display(fd,crtc);
    return result;
}

int drmModeSetCrtc(int fd,uint32_t crtc,uint32_t id,uint32_t x,uint32_t y,
                   uint32_t *connectors,int count,void *mode)
{
    pthread_once(&native_once,resolve_native);
    if(!native_set){errno=ENOSYS;return -1;}
    int result=native_set(fd,crtc,id,x,y,connectors,count,mode);
    if(!result&&!x&&!y)remember_display(fd,crtc);
    return result;
}
