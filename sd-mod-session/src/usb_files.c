#define _GNU_SOURCE
/* USB-network SD browser. Roots are directory descriptors, never request
 * paths. Only new WAVs in Samples/User/USB Inbox may be written. */
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#ifndef USB_FILES_ADDRESS
#define USB_FILES_ADDRESS "192.168.155.1"
#endif
#ifndef USB_FILES_PORT
#define USB_FILES_PORT 8082
#endif
#define UPLOAD_LIMIT (UINT64_C(1024)*1024*1024)
static int roots[2],inbox;
static const char *flag;
static const char page[]=
 "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
 "<title>Am MOD SD files</title><style>body{font:16px system-ui;background:#16191d;color:#eee;max-width:850px;margin:30px auto;padding:16px}"
 "a{color:#ffc247}button,input{font:inherit;margin:6px;padding:8px}li{padding:7px}#status{white-space:pre-wrap}</style>"
 "<h1>Am MOD SD files</h1><p>Download projects and samples. Upload new WAV samples to Samples/User/USB Inbox.</p>"
 "<p>Save the project before downloading it. Keep the card inserted until transfers finish.</p>"
 "<button id=projects>Projects</button><button id=samples>Samples</button><button id=up>Up</button>"
 "<h2 id=path></h2><ul id=files></ul><h2>Add WAV samples</h2><input id=upload type=file accept=.wav multiple><button id=send>Upload</button><p id=status></p>"
 "<script>let path='Samples';const el=id=>document.getElementById(id),enc=p=>p.split('/').map(encodeURIComponent).join('/');"
 "async function list(p){try{const r=await fetch('/list/'+enc(p));if(!r.ok)throw Error('Cannot open folder');const items=await r.json();path=p;el('path').textContent=p;el('files').replaceChildren();"
 "for(const item of items){const li=document.createElement('li'),a=document.createElement('a');a.textContent=item.name+(item.dir?'/':'');"
 "if(item.dir){a.href='#';a.onclick=e=>{e.preventDefault();list(path+'/'+item.name)}}else{a.href='/get/'+enc(path+'/'+item.name);a.download=item.name}li.append(a);el('files').append(li)}}catch(e){el('status').textContent=e.message}}"
 "el('projects').onclick=()=>list('Projects');el('samples').onclick=()=>list('Samples');el('up').onclick=()=>list(path.includes('/')?path.slice(0,path.lastIndexOf('/')):path);"
 "el('send').onclick=async()=>{el('send').disabled=true;try{for(const f of el('upload').files){el('status').textContent='Uploading '+f.name;const r=await fetch('/upload/'+encodeURIComponent(f.name),{method:'PUT',headers:{'Content-Type':'application/octet-stream'},body:f});if(!r.ok)throw Error(f.name+': '+await r.text())}el('status').textContent='Upload complete';await list('Samples/User/USB Inbox')}catch(e){el('status').textContent=e.message}finally{el('send').disabled=false}};list(path)</script>";
static int send_all(int fd,const void*p,size_t n){const char*b=p;while(n){ssize_t w=send(fd,b,n,MSG_NOSIGNAL);if(w<0&&errno==EINTR)continue;if(w<=0)return 0;b+=w;n-=(size_t)w;}return 1;}
static int header(int fd,int status,const char*type,uint64_t n){char h[512];int k=snprintf(h,sizeof(h),
 "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %llu\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nContent-Security-Policy: frame-ancestors 'none'\r\nConnection: close\r\n\r\n",
 status,status==200?"OK":status==201?"Created":"Error",type,(unsigned long long)n);return k>0&&k<(int)sizeof(h)&&send_all(fd,h,(size_t)k);}
static void reply(int fd,int status,const char*msg){if(header(fd,status,"text/plain; charset=utf-8",strlen(msg)))send_all(fd,msg,strlen(msg));}
static int hex(unsigned char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
static int decode(char*out,const char*in,size_t cap){size_t n=0;while(*in){unsigned c=(unsigned char)*in++;if(c=='%'){if(!in[0]||!in[1])return 0;int a=hex((unsigned char)in[0]),b=hex((unsigned char)in[1]);if(a<0||b<0)return 0;c=(unsigned)(a*16+b);in+=2;}
 if(c<32||c==127||c=='\\'||c=='?'||c=='#'||n+1>=cap)return 0;out[n++]=(char)c;}out[n]=0;return n>0;}
static int component(const char*s){return *s&&s[0]!='.'&&!strchr(s,'/')&&!strchr(s,'\\')&&strlen(s)<=255;}
static int open_path(char*path,int directory){
 char*save=NULL,*part=strtok_r(path,"/",&save);if(!part)return -1;
 int root=!strcmp(part,"Projects")?0:!strcmp(part,"Samples")?1:-1;if(root<0)return -1;
 int fd=openat(roots[root],".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(fd<0)return -1;
 for(part=strtok_r(NULL,"/",&save);part;part=strtok_r(NULL,"/",&save)){
  if(!component(part)){close(fd);return -1;}
  int is_dir=directory||(save&&*save),next=openat(fd,part,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC|(is_dir?O_DIRECTORY:0));close(fd);fd=next;if(fd<0)return -1;
 }
 struct stat st;if(fstat(fd,&st)||(directory?!S_ISDIR(st.st_mode):!S_ISREG(st.st_mode))){close(fd);return -1;}return fd;
}
static int append_json(char*out,size_t*used,size_t cap,const char*s){
 if(*used+2>=cap)return 0;out[(*used)++]='"';
 for(;*s;++s){unsigned char c=(unsigned char)*s;if(*used+7>=cap)return 0;if(c<32){int n=snprintf(out+*used,cap-*used,"\\u%04x",c);*used+=(size_t)n;}
  else{if(c=='"'||c=='\\')out[(*used)++]='\\';out[(*used)++]=(char)c;}}
 out[(*used)++]='"';return 1;
}
static void list(int client,int fd){
 DIR*d=fdopendir(fd);if(!d){close(fd);reply(client,404,"Folder unavailable");return;}
 size_t cap=2*1024*1024,used=1,count=0;char*out=malloc(cap);if(!out){closedir(d);reply(client,503,"Out of memory");return;}out[0]='[';
 struct dirent*e;int ok=1;while((e=readdir(d))){
  if(e->d_name[0]=='.')continue;struct stat st;if(fstatat(dirfd(d),e->d_name,&st,AT_SYMLINK_NOFOLLOW)||(!S_ISREG(st.st_mode)&&!S_ISDIR(st.st_mode)))continue;
  if(used+64>=cap){ok=0;break;}if(count++)out[used++]=',';memcpy(out+used,"{\"name\":",8);used+=8;
  if(!append_json(out,&used,cap,e->d_name)){ok=0;break;}
  int n=snprintf(out+used,cap-used,",\"dir\":%s}",S_ISDIR(st.st_mode)?"true":"false");used+=(size_t)n;
 }
 closedir(d);if(ok){out[used++]=']';if(header(client,200,"application/json; charset=utf-8",used))send_all(client,out,used);}else reply(client,413,"Folder too large; open a smaller folder");free(out);
}
static void download(int client,int fd){struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<0){close(fd);reply(client,404,"File unavailable");return;}
 if(header(client,200,"application/octet-stream",(uint64_t)st.st_size)){char b[65536];off_t left=st.st_size;while(left>0&&access(flag,F_OK)==0){size_t want=left>(off_t)sizeof(b)?sizeof(b):(size_t)left;ssize_t n=read(fd,b,want);if(n<0&&errno==EINTR)continue;if(n<=0||!send_all(client,b,(size_t)n))break;left-=n;}}close(fd);}
static void upload(int client,const char*name,uint64_t size){
 size_t len=strlen(name);if(!component(name)||len<5||strcasecmp(name+len-4,".wav")){reply(client,400,"Choose a WAV filename without path separators");return;}
 if(!size||size>UPLOAD_LIMIT){reply(client,413,"WAV must be between 1 byte and 1 GiB");return;}
 char temporary[64];snprintf(temporary,sizeof(temporary),".usb-upload-%ld",(long)getpid());
 int fd=openat(inbox,temporary,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);if(fd<0){reply(client,409,"Upload unavailable");return;}
 uint64_t left=size;char b[65536];int ok=1;
 while(left&&ok&&access(flag,F_OK)==0){size_t want=left>sizeof(b)?sizeof(b):(size_t)left;ssize_t n=recv(client,b,want,0);if(n<0&&errno==EINTR)continue;if(n<=0){ok=0;break;}
  size_t done=0;while(done<(size_t)n){ssize_t w=write(fd,b+done,(size_t)n-done);if(w<0&&errno==EINTR)continue;if(w<=0){ok=0;break;}done+=(size_t)w;}left-=(uint64_t)n;
 }
 if(left)ok=0;
 /* Validate WAV magic before making the file visible to the sampler. */
 if(ok){unsigned char magic[12];int check=openat(inbox,temporary,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);ssize_t n=check<0?-1:read(check,magic,12);if(check>=0)close(check);if(n!=12||memcmp(magic,"RIFF",4)||memcmp(magic+8,"WAVE",4))ok=0;}
 if(ok&&fsync(fd))ok=0;close(fd);
 /* exFAT/FAT32 have no hard links. Atomic no-replace rename is provided by
  * the pinned Linux kernel and these filesystems. Never fall back to an
  * overwriting rename or an exists-then-rename race. */
 int exists=0;if(ok&&renameat2(inbox,temporary,inbox,name,RENAME_NOREPLACE)){exists=errno==EEXIST;ok=0;}
 unlinkat(inbox,temporary,0);
 if(ok&&fsync(inbox))ok=0;
 reply(client,ok?201:exists?409:400,ok?"Saved to Samples/User/USB Inbox":exists?"File already exists; rename your WAV":"Upload incomplete, invalid WAV, or SD write failed");
}
static int length_value(const char*s,uint64_t*out){if(!*s)return 0;uint64_t v=0;for(;*s;++s){if(*s<'0'||*s>'9'||v>(UINT64_MAX-(unsigned)(*s-'0'))/10)return 0;v=v*10+(unsigned)(*s-'0');}*out=v;return 1;}
static void serve(int client){
 struct timeval timeout={.tv_sec=5};setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
 char h[16384];size_t n=0;
 /* Read only through CRLFCRLF; upload body stays in the socket. */
 while(n+1<sizeof(h)){ssize_t r=recv(client,h+n,1,0);if(r!=1)return;++n;if(n>=4&&!memcmp(h+n-4,"\r\n\r\n",4))break;}
 h[n]=0;if(n<4||memcmp(h+n-4,"\r\n\r\n",4)){reply(client,431,"Headers too large");return;}
 char method[8],url[8192],version[16],extra;char*line_end=strstr(h,"\r\n");if(!line_end)return;*line_end=0;
 if(sscanf(h,"%7s %8191s %15s %c",method,url,version,&extra)!=3||strcmp(version,"HTTP/1.1")){reply(client,400,"Bad request");return;}
 int host=0,has_length=0;uint64_t length=0;
 char expected[96];snprintf(expected,sizeof(expected),"%s:%u",USB_FILES_ADDRESS,USB_FILES_PORT);
 for(char*line=line_end+2;*line;){char*end=strstr(line,"\r\n");if(!end)break;*end=0;if(!*line)break;
  char*value=strchr(line,':');if(!value){reply(client,400,"Bad header");return;}*value++=0;while(*value==' '||*value=='\t')++value;
  if(!strcasecmp(line,"Host")){if(host||strcmp(value,expected)){reply(client,403,"Use the MPC USB address");return;}host=1;}
  if(!strcasecmp(line,"Origin")){char origin[112];snprintf(origin,sizeof(origin),"http://%s",expected);if(strcmp(value,origin)){reply(client,403,"Origin rejected");return;}}
  if(!strcasecmp(line,"Transfer-Encoding")){reply(client,400,"Use Content-Length");return;}
  if(!strcasecmp(line,"Content-Length")){if(has_length++||!length_value(value,&length)){reply(client,400,"Bad Content-Length");return;}}
  line=end+2;
 }
 if(!host){reply(client,400,"Host required");return;}
 if(access(flag,F_OK)){reply(client,403,"Enable USB files on the MPC");return;}
 if(!strcmp(method,"GET")&&!strcmp(url,"/")){if(header(client,200,"text/html; charset=utf-8",sizeof(page)-1))send_all(client,page,sizeof(page)-1);return;}
 char path[4096];
 if(!strcmp(method,"PUT")&&!strncmp(url,"/upload/",8)){
  if(!has_length||!decode(path,url+8,sizeof(path))){reply(client,400,"Invalid upload");return;}upload(client,path,length);return;}
 int listing=!strncmp(url,"/list/",6),getting=!strncmp(url,"/get/",5);
 if(strcmp(method,"GET")||(!listing&&!getting)||!decode(path,url+(listing?6:5),sizeof(path))){reply(client,404,"Not found");return;}
 int fd=open_path(path,listing);if(fd<0){reply(client,404,"Not found");return;}
 if(listing)list(client,fd);else download(client,fd);
}
int main(int argc,char**argv){
 if(argc!=3){fprintf(stderr,"usage: usb-files <MPC-Sample directory> <enabled flag>\n");return 2;}flag=argv[2];
 int root=open(argv[1],O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(root<0)return 2;
 roots[0]=openat(root,"Projects",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);roots[1]=openat(root,"Samples",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);close(root);if(roots[0]<0||roots[1]<0)return 2;
 if(mkdirat(roots[1],"User",0700)&&errno!=EEXIST)return 2;
 int user=openat(roots[1],"User",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(user<0)return 2;
 if(mkdirat(user,"USB Inbox",0700)&&errno!=EEXIST)return 2;
 inbox=openat(user,"USB Inbox",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);close(user);if(inbox<0)return 2;
 signal(SIGPIPE,SIG_IGN);
 for(;;){
  if(access(flag,F_OK)){usleep(250000);continue;}
  int server=socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC,0);if(server<0)return 2;
  int one=1;setsockopt(server,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
  struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons(USB_FILES_PORT)};
  if(inet_pton(AF_INET,USB_FILES_ADDRESS,&address.sin_addr)!=1)return 2;
  if(bind(server,(void*)&address,sizeof(address))||listen(server,4)){close(server);sleep(1);continue;}
  while(!access(flag,F_OK)){
   struct pollfd ready={.fd=server,.events=POLLIN};if(poll(&ready,1,250)<=0)continue;
   int client=accept4(server,NULL,NULL,SOCK_CLOEXEC);if(client<0)continue;serve(client);close(client);
  }
  close(server);
 }
}
