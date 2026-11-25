
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
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
static uint64_t frame_generation;
static int frame_valid;

static uint64_t milliseconds(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC,&now);
    return (uint64_t)now.tv_sec*1000+(unsigned long)now.tv_nsec/1000000;
}

static int load_frame(const char *path)
{
    FILE *file=fopen(path,"rb");
    if(!file){frame_valid=0;return 0;}
    static unsigned char incoming[MIRROR_BMP_BYTES];
    int ok=fread(incoming,1,sizeof(incoming),file)==sizeof(incoming)&&
           incoming[0]=='B'&&incoming[1]=='M';
    fclose(file);
    if(ok&&(!frame_valid||memcmp(frame,incoming,sizeof(frame)))){
        memcpy(frame,incoming,sizeof(frame));++frame_generation;
    }
    frame_valid=ok;
    return ok;
}

static const char page[]=
    "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Am MOD Screen Mirror</title><style>"
    "body{margin:0;background:#121212;color:#eee;font:16px system-ui;display:grid;place-content:center;min-height:100vh;text-align:center}"
    "img{width:min(100vw,133vh);image-rendering:pixelated}#status{padding:1em}</style>"
    "<img id=screen alt='MPC Sample screen' hidden><div id=status>Waiting for a frame</div>"
    "<script>const s=document.getElementById('screen'),t=document.getElementById('status');"
    "function show(b){const next=URL.createObjectURL(b),old=s.src;s.src=next;if(old.startsWith('blob:'))URL.revokeObjectURL(old);s.hidden=false;t.hidden=true}"
    "async function update(){try{const r=await fetch('/frame.bmp',{cache:'no-store',signal:AbortSignal.timeout(2500)});if(!r.ok)throw Error();show(await r.blob())}"
    "catch(e){s.hidden=true;t.hidden=false;t.textContent='Screen unavailable. Check Screen Mirror on the MPC.'}setTimeout(update,80)}"
    /* Fixed-size BMP records, explicitly parsed: no reliance on browser BMP
     * multipart support. Carry storage is exactly one frame, even if TCP
     * delivers several records or divides a header across reads. */
    "async function stream(){const c=new AbortController();let timer=setTimeout(()=>c.abort(),2500);try{"
    "const r=await fetch('/stream',{cache:'no-store',signal:c.signal});if(!r.ok||!r.body)throw Error();"
    "const reader=r.body.getReader(),b=new Uint8Array(230454);let n=0;for(;;){const v=await reader.read();if(v.done)throw Error();"
    "let p=0;while(p<v.value.length){const k=Math.min(b.length-n,v.value.length-p);b.set(v.value.subarray(p,p+k),n);n+=k;p+=k;"
    "if(n===b.length){if(b[0]!==66||b[1]!==77)throw Error();show(new Blob([b],{type:'image/bmp'}));n=0;clearTimeout(timer);timer=setTimeout(()=>c.abort(),5000)}}}}"
    "catch(e){c.abort();clearTimeout(timer);update()}}stream()</script>";

/* Fixed capacity, no frame queue, no worker per browser. A stalled client
 * retains at most one frame and never blocks capture or other clients. */
struct client {
    int fd,mode; /* 0 request, 1 reply then close, 2 stream */
    char request[1024];size_t received;
    unsigned char output[MIRROR_BMP_BYTES+512];size_t length,sent;
    uint64_t progress,last_frame,generation;
};
static struct client clients[4];
static void drop(struct client*c){if(c->fd>=0)close(c->fd);memset(c,0,sizeof(*c));c->fd=-1;}
static void reply(struct client*c,const char*status,const char*type,const void*data,size_t size)
{
    int n=snprintf((char*)c->output,512,
        "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n",status,type,size);
    memcpy(c->output+n,data,size);c->length=(size_t)n+size;c->mode=1;
}
static void request(struct client*c,const char*path)
{
    ssize_t n=recv(c->fd,c->request+c->received,sizeof(c->request)-1-c->received,0);
    if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))return;
    if(n<=0){drop(c);return;}
    c->received+=(size_t)n;c->request[c->received]=0;
    if(!strchr(c->request,'\n')){if(c->received==sizeof(c->request)-1)drop(c);return;}
    if(!strncmp(c->request,"GET / HTTP/",11))reply(c,"200 OK","text/html; charset=utf-8",page,sizeof(page)-1);
    else if(!strncmp(c->request,"GET /frame.bmp HTTP/",20)){
        if(load_frame(path))reply(c,"200 OK","image/bmp",frame,sizeof(frame));
        else reply(c,"503 Service Unavailable","text/plain","No screen frame",15);
    }else if(!strncmp(c->request,"GET /stream HTTP/",17)){
        static const char header[]="HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n";
        memcpy(c->output,header,sizeof(header)-1);c->length=sizeof(header)-1;c->mode=2;
    }else reply(c,"404 Not Found","text/plain","Not found",9);
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
    for(unsigned i=0;i<4;++i)clients[i].fd=-1;
    for(;;){
        if(access(argv[1],F_OK)){usleep(250000);continue;}
        int listener=listen_usb();
        if(listener<0){sleep(1);continue;}
        uint64_t last_read=0;
        while(!access(argv[1],F_OK)){
            uint64_t now=milliseconds();
            int streaming=0;for(unsigned i=0;i<4;++i)if(clients[i].fd>=0&&clients[i].mode==2)streaming=1;
            if(streaming&&now-last_read>=40){load_frame(argv[2]);last_read=now;}
            for(unsigned i=0;i<4;++i){struct client*c=clients+i;if(c->fd<0)continue;
                if((!c->mode&&now-c->progress>250)||(c->length>c->sent&&now-c->progress>2000)){drop(c);continue;}
                /* A one-second heartbeat keeps an idle stream distinguishable
                 * from a broken connection. Changed screens run at <=25 fps. */
                if(c->mode==2&&c->length==c->sent&&now-c->last_frame>=40&&frame_valid&&
                   (now-c->last_frame>=1000||c->generation!=frame_generation)){
                    memcpy(c->output,frame,sizeof(frame));c->length=sizeof(frame);c->sent=0;c->last_frame=now;c->progress=now;c->generation=frame_generation;
                }
            }
            struct pollfd ready[5]={{.fd=listener,.events=POLLIN}};
            for(unsigned i=0;i<4;++i){struct client*c=clients+i;ready[i+1]=(struct pollfd){.fd=c->fd,.events=c->mode?(c->length>c->sent?POLLOUT:0):POLLIN};}
            if(poll(ready,5,20)<0)continue;
            for(unsigned i=0;i<4;++i){struct client*c=clients+i;short r=ready[i+1].revents;if(c->fd<0)continue;
                if(r&(POLLERR|POLLHUP|POLLNVAL)){drop(c);continue;}
                if(r&POLLIN)request(c,argv[2]);
                if(c->fd>=0&&(r&POLLOUT)){
                    ssize_t n=send(c->fd,c->output+c->sent,c->length-c->sent,MSG_NOSIGNAL|MSG_DONTWAIT);
                    if(n>0){c->sent+=(size_t)n;c->progress=milliseconds();if(c->sent==c->length&&c->mode==1)drop(c);}
                    else if(!n||(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR))drop(c);
                }
            }
            if(ready[0].revents&POLLIN){int fd=accept4(listener,NULL,NULL,SOCK_CLOEXEC|SOCK_NONBLOCK);
                if(fd>=0){unsigned i;for(i=0;i<4&&clients[i].fd>=0;++i){}if(i==4)close(fd);else{clients[i].fd=fd;clients[i].progress=milliseconds();}}
            }
        }
        for(unsigned i=0;i<4;++i)drop(clients+i);
        close(listener);
    }
}
