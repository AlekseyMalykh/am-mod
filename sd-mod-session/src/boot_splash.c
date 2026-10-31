#define _GNU_SOURCE
#include "mod_splash.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ROOT "/run/mpc-clone-trial-v1"
#define IMAGE ROOT "/boot-frame.png"
#define NEXT ROOT "/boot-frame.next"
#ifndef BOOT_SPLASH_CTL
#define BOOT_SPLASH_CTL "/usr/bin/az0x-splashctl"
#endif
static volatile sig_atomic_t stopping;
static unsigned char pixels[320*240*3],raw[240*(320*3+1)],compressed[sizeof(raw)+64];
static uint64_t now_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static void stop(int sig){(void)sig;stopping=1;}
static void rectangle(void*p,int x,int y,int w,int h,uint32_t colour){
    (void)p;
    if(x<0||y<0||w<1||h<1||x+w>320||y+h>240)return;
    for(int r=y;r<y+h;++r)for(int c=x;c<x+w;++c){unsigned char*v=pixels+(r*320+c)*3;v[0]=colour>>16;v[1]=colour>>8;v[2]=colour;}
}
static void be32(unsigned char*p,uint32_t n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
static uint32_t crc32(const unsigned char*p,size_t n,uint32_t crc){
    for(size_t k=0;k<n;++k){crc^=p[k];for(unsigned i=0;i<8;++i)crc=(crc>>1)^(0xedb88320u&-(crc&1u));}return crc;
}
static int chunk(FILE*f,const char*tag,const void*body,size_t n){
    unsigned char header[8],tail[4];be32(header,(uint32_t)n);memcpy(header+4,tag,4);
    uint32_t crc=crc32(header+4,4,UINT32_MAX);crc=crc32(body,n,crc);be32(tail,~crc);
    return fwrite(header,1,8,f)==8&&(!n||fwrite(body,1,n,f)==n)&&fwrite(tail,1,4,f)==4;
}
static int frame(unsigned ms,const char*path){
    memset(pixels,0,sizeof(pixels));
    /* White blocks gather into the familiar 68px mark. Colour starts with
     * isolated edge blocks while this assembly is still in progress. */
    for(unsigned i=0;i<16;++i){
        int x=126+(i%4)*18,y=86+(i/4)*18;
        int from_x=i%2?296:10,from_y=12+(int)i*13;
        unsigned delay=i*190,progress=ms>delay?ms-delay:0;
        if(progress>2450)progress=2450;
        x=from_x+(x-from_x)*(int)progress/2450;
        y=from_y+(y-from_y)*(int)progress/2450;
        rectangle(NULL,x,y,14,14,0xffffffff);
    }
    struct mod_splash_painter p={NULL,rectangle};mod_splash_frame(ms,320,240,&p);
    for(unsigned y=0;y<240;++y){raw[y*961]=0;memcpy(raw+y*961+1,pixels+y*960,960);}
    size_t out=0,pos=0;compressed[out++]=0x78;compressed[out++]=0x01;
    while(pos<sizeof(raw)){
        unsigned n=sizeof(raw)-pos>65535?65535:(unsigned)(sizeof(raw)-pos);
        compressed[out++]=(pos+n==sizeof(raw));compressed[out++]=n;compressed[out++]=n>>8;
        compressed[out++]=~n;compressed[out++]=~n>>8;memcpy(compressed+out,raw+pos,n);out+=n;pos+=n;
    }
    uint32_t a=1,b=0;for(size_t i=0;i<sizeof(raw);++i){a=(a+raw[i])%65521;b=(b+a)%65521;}
    be32(compressed+out,(b<<16)|a);out+=4;
    int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0)return 0;
    FILE*f=fdopen(fd,"wb");if(!f){close(fd);return 0;}
    unsigned char ihdr[13]={};be32(ihdr,320);be32(ihdr+4,240);ihdr[8]=8;ihdr[9]=2;
    int ok=fwrite("\211PNG\r\n\032\n",1,8,f)==8&&chunk(f,"IHDR",ihdr,sizeof(ihdr))&&chunk(f,"IDAT",compressed,out)&&chunk(f,"IEND",NULL,0);
    if(fclose(f))ok=0;
    return ok;
}
static int command(int show){
    pid_t pid=fork();if(pid<0)return 0;
    if(!pid){
        char owner[24];snprintf(owner,sizeof(owner),"%ld",(long)getppid());
        if(show)execl(BOOT_SPLASH_CTL,BOOT_SPLASH_CTL,"show-image","--keep","--pid",owner,IMAGE,(char*)NULL);
        else execl(BOOT_SPLASH_CTL,BOOT_SPLASH_CTL,"drop-display",(char*)NULL);
        _exit(127);
    }
    int status=0;uint64_t deadline=now_ms()+600;
    for(;;){
        pid_t done=waitpid(pid,&status,WNOHANG);if(done==pid)return WIFEXITED(status)&&!WEXITSTATUS(status);
        if(done<0&&errno!=EINTR)return 0;
        if(now_ms()>=deadline||(show&&stopping))break;
        usleep(10000);
    }
    kill(pid,SIGKILL);while(waitpid(pid,&status,0)<0&&errno==EINTR){}return 0;
}
int main(int argc,char**argv){
    if(argc==4&&!strcmp(argv[1],"--frame"))return frame((unsigned)strtoul(argv[2],NULL,10),argv[3])?0:1;
    if(argc==2&&!strcmp(argv[1],"--release"))return command(0)?0:1;
    if(argc!=1)return 2;
    struct stat st;struct statfs fs;
    if(lstat(ROOT,&st)||!S_ISDIR(st.st_mode)||st.st_uid||statfs(ROOT,&fs)||fs.f_type!=0x01021994)return 1;
    if(lstat("/run/az0x-splash-control",&st)||!S_ISSOCK(st.st_mode)){
        fprintf(stderr,"BOOT_SPLASH SKIP native display service unavailable\n");return 0;
    }
    struct sigaction sa={};sa.sa_handler=stop;sigemptyset(&sa.sa_mask);sigaction(SIGTERM,&sa,NULL);sigaction(SIGINT,&sa,NULL);
    uint64_t start=now_ms();unsigned shown=0;
    while(!stopping&&now_ms()-start<15000){
        if(!frame((unsigned)(now_ms()-start),NEXT)||rename(NEXT,IMAGE)||!command(1))break;
        ++shown;usleep(150000);
    }
    if(shown)(void)command(0);
    unlink(NEXT);unlink(IMAGE);
    fprintf(stderr,"BOOT_SPLASH END frames=%u\n",shown);return 0;
}
