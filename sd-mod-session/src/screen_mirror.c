
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "screen_mirror_frame.h"

#ifndef MIRROR_ADDRESS
#define MIRROR_ADDRESS "192.168.155.1"
#endif
#ifndef MIRROR_PORT
#define MIRROR_PORT 8081
#endif
static unsigned char frame[MIRROR_BMP_BYTES];

static uint64_t milliseconds(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC,&now);
    return (uint64_t)now.tv_sec*1000+(unsigned long)now.tv_nsec/1000000;
}

static int load_frame(const char *path)
{
    FILE *file=fopen(path,"rb");
    if(!file)return 0;
    int ok=fread(frame,1,sizeof(frame),file)==sizeof(frame)&&
           frame[0]=='B'&&frame[1]=='M';
    fclose(file);
    return ok;
}

static const char page[]=
    "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Am MOD Screen Mirror</title><style>"
    "body{margin:0;background:#121212;color:#eee;font:16px system-ui;display:grid;place-content:center;min-height:100vh;text-align:center}"
    "img{width:min(100vw,133vh);image-rendering:pixelated}#status{padding:1em}</style>"
    "<img id=screen alt='MPC Sample screen' hidden><div id=status>Waiting for a frame</div>"
    "<script>const s=document.getElementById('screen'),t=document.getElementById('status');"
    "async function update(){try{const r=await fetch('/frame.bmp',{cache:'no-store'});if(!r.ok)throw Error();"
    "const next=URL.createObjectURL(await r.blob()),old=s.src;s.src=next;if(old.startsWith('blob:'))URL.revokeObjectURL(old);"
    "s.hidden=false;t.hidden=true}catch(e){s.hidden=true;t.hidden=false;t.textContent='Screen unavailable. Check Screen Mirror on the MPC.'}"
    "setTimeout(update,200)}update()</script>";

static int send_all(int client,const void *data,size_t size)
{
    const char *bytes=data;
    while(size){
        ssize_t sent=send(client,bytes,size,MSG_NOSIGNAL);
        if(sent<=0)return 0;
        bytes+=sent;size-=(size_t)sent;
    }
    return 1;
}

static void reply(int client,const char *status,const char *type,const void *data,size_t size)
{
    char header[256];
    int length=snprintf(header,sizeof(header),
        "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n",
        status,type,size);
    if(length>0&&(size_t)length<sizeof(header)&&send_all(client,header,(size_t)length))send_all(client,data,size);
}

static void serve(int client,const char *flag,const char *frame_path)
{
    const struct timeval timeout={.tv_usec=250000};
    setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    char request[1024];
    size_t length=0;
    uint64_t start=milliseconds();
    do{
        ssize_t received=recv(client,request+length,sizeof(request)-1-length,0);
        if(received<=0)return;
        length+=(size_t)received;request[length]=0;
    }while(!strchr(request,'\n')&&length<sizeof(request)-1&&milliseconds()-start<250);
    if(access(flag,F_OK))return;
    if(!strncmp(request,"GET / HTTP/",11)){
        reply(client,"200 OK","text/html; charset=utf-8",page,sizeof(page)-1);
    }else if(!strncmp(request,"GET /frame.bmp HTTP/",20)){
        if(load_frame(frame_path))reply(client,"200 OK","image/bmp",frame,sizeof(frame));
        else reply(client,"503 Service Unavailable","text/plain","No screen frame",15);
    }else reply(client,"404 Not Found","text/plain","Not found",9);
}

static int listen_usb(void)
{
    int fd=socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC,0);
    if(fd<0)return -1;
    const int enable=1;
    setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&enable,sizeof(enable));
    struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons(MIRROR_PORT)};
    if(inet_pton(AF_INET,MIRROR_ADDRESS,&address.sin_addr)!=1||
       bind(fd,(void*)&address,sizeof(address))||listen(fd,2)){
        close(fd);return -1;
    }
    return fd;
}

int main(int argc,char **argv)
{
    if(argc!=3)return 2;
    for(;;){
        if(access(argv[1],F_OK)){usleep(250000);continue;}
        int listener=listen_usb();
        if(listener<0){sleep(1);continue;}
        while(!access(argv[1],F_OK)){
            struct pollfd ready={.fd=listener,.events=POLLIN};
            if(poll(&ready,1,200)<=0)continue;
            int client=accept4(listener,NULL,NULL,SOCK_CLOEXEC);
            if(client<0)continue;
            serve(client,argv[1],argv[2]);
            close(client);
        }
        close(listener);
    }
}
