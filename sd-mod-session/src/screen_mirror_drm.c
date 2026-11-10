#define _GNU_SOURCE
#include "screen_mirror_frame.h"
#include <dlfcn.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

struct mirror_buffer {
    int fd;
    uint32_t id,handle,pitch;
    uint8_t bits;
};

static struct mirror_buffer buffers[8];
static unsigned char frame[MIRROR_BMP_BYTES];
static uint64_t last_frame;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t native_once=PTHREAD_ONCE_INIT;
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
}

static void capture_buffer(int fd,uint32_t id)
{
    int saved=errno;
    if(pthread_mutex_trylock(&lock))return;
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC,&time);
    uint64_t now=(uint64_t)time.tv_sec*1000+(unsigned long)time.tv_nsec/1000000;
    if(id&&now-last_frame>=180){
        last_frame=now;
        if(access(SCREEN_MIRROR_FLAG,F_OK))goto done;
        for(unsigned i=0;i<sizeof(buffers)/sizeof(buffers[0]);++i){
            const struct mirror_buffer *b=&buffers[i];
            if(b->id!=id||b->fd!=fd)continue;
            struct drm_mode_map_dumb offset={.handle=b->handle};
            if(ioctl(fd,DRM_IOCTL_MODE_MAP_DUMB,&offset))break;
            size_t size=(size_t)b->pitch*MIRROR_HEIGHT;
            const unsigned char *pixels=mmap(NULL,size,PROT_READ,MAP_SHARED,fd,(off_t)offset.offset);
            if(pixels!=MAP_FAILED){
                encode_frame(pixels,b->pitch,b->bits);
                munmap((void*)pixels,size);
                publish_frame();
            }
            break;
        }
    }
done:
    pthread_mutex_unlock(&lock);
    errno=saved;
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
                buffers[i]=(struct mirror_buffer){fd,*id,handle,pitch,bits};
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
        if(buffers[i].fd==fd&&buffers[i].id==id)buffers[i].id=0;
    pthread_mutex_unlock(&lock);
    return native_remove(fd,id);
}

int drmModePageFlip(int fd,uint32_t crtc,uint32_t id,uint32_t flags,void *data)
{
    pthread_once(&native_once,resolve_native);
    if(!native_flip){errno=ENOSYS;return -1;}
    int result=native_flip(fd,crtc,id,flags,data);
    if(!result)capture_buffer(fd,id);
    return result;
}

int drmModeSetCrtc(int fd,uint32_t crtc,uint32_t id,uint32_t x,uint32_t y,
                   uint32_t *connectors,int count,void *mode)
{
    pthread_once(&native_once,resolve_native);
    if(!native_set){errno=ENOSYS;return -1;}
    int result=native_set(fd,crtc,id,x,y,connectors,count,mode);
    if(!result&&!x&&!y)capture_buffer(fd,id);
    return result;
}
