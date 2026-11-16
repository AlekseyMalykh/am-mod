#define _GNU_SOURCE
/* Runs only as PID1 on linux,dummy-virt inside a disposable RAM initramfs. */
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/reboot.h>
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int kmsg=-1;
static const char*root="/sys/kernel/config/usb_gadget/am_names_test";
static void logline(const char*format,...){
 char text[512];va_list a;va_start(a,format);vsnprintf(text,sizeof(text),format,a);va_end(a);
 if(kmsg>=0)dprintf(kmsg,"<6>AM_VM %s\n",text);
}
static void finish(int success){
 logline(success?"PASS USB_NAMES_VM":"FAIL USB_NAMES_VM");sync();reboot(RB_POWER_OFF);for(;;)pause();
}
static void require(int ok,const char*what){if(!ok){logline("ERROR %s errno=%d",what,errno);finish(0);}}
static int load(const char*path){int f=open(path,O_RDONLY);if(f<0)return -1;int r=(int)syscall(SYS_finit_module,f,"",0);close(f);return r;}
static int unload(const char*name){return (int)syscall(SYS_delete_module,name,O_NONBLOCK);}
static void directory(const char*path){require(!mkdir(path,0755)||errno==EEXIST,path);}
static void writefile(const char*path,const char*value){
 int fd=open(path,O_WRONLY);require(fd>=0,path);size_t len=strlen(value);
 require(write(fd,value,len)==(ssize_t)len,path);close(fd);
}
static void relative(const char*name,const char*value){char p[512];snprintf(p,sizeof(p),"%s/%s",root,name);writefile(p,value);}
static unsigned number(const char*path){FILE*f=fopen(path,"r");require(f!=NULL,path);unsigned n=0;require(fscanf(f,"%u",&n)==1,path);fclose(f);return n;}
static int usb_device(char*path,size_t cap){
 for(unsigned wait=0;wait<100;++wait){
  DIR*d=opendir("/sys/bus/usb/devices");if(d){struct dirent*e;
   while((e=readdir(d))){char p[512],vendor[32]={0};snprintf(p,sizeof(p),"/sys/bus/usb/devices/%s/idVendor",e->d_name);FILE*f=fopen(p,"r");if(!f)continue;if(!fgets(vendor,sizeof(vendor),f))vendor[0]=0;fclose(f);if(strncmp(vendor,"09e8",4))continue;
    snprintf(p,sizeof(p),"/sys/bus/usb/devices/%s/busnum",e->d_name);unsigned bus=number(p);
    snprintf(p,sizeof(p),"/sys/bus/usb/devices/%s/devnum",e->d_name);unsigned dev=number(p);
    snprintf(path,cap,"/dev/bus/usb/%03u/%03u",bus,dev);closedir(d);return open(path,O_RDWR);
   }closedir(d);
  }usleep(100000);
 }return -1;
}
static int getdesc(int fd,unsigned kind,unsigned index,unsigned char*buffer,unsigned len){
 struct usbdevfs_ctrltransfer c={.bRequestType=0x80,.bRequest=6,.wValue=(uint16_t)((kind<<8)|index),.wIndex=kind==3?0x409:0,.wLength=(uint16_t)len,.timeout=3000,.data=buffer};
 return ioctl(fd,USBDEVFS_CONTROL,&c);
}
static void check_names(int named){
 unsigned char b[4096];char path[256];int fd=usb_device(path,sizeof(path));require(fd>=0,"virtual USB device");
 int n=getdesc(fd,2,0,b,sizeof(b));require(n>9,"configuration descriptor");
 unsigned cls=0,sub=0,base=0,found=0;
 for(unsigned p=0;p+2<=(unsigned)n;){
  unsigned len=b[p],type=b[p+1];require(len>=2&&p+len<=(unsigned)n,"descriptor boundary");
  if(type==4&&len>=9){cls=b[p+5];sub=b[p+6];}
  if(type==0x24&&cls==1&&sub==2&&len>=16&&b[p+2]==1&&b[p+10]==(named?18:2)){
   if(named){require(!b[p+11]&&!b[p+12]&&!b[p+13]&&!b[p+14],"non-spatial channel mask");require(b[p+15]!=0,"channel name base");base=b[p+15];}
   ++found;
  }p+=len;
 }
 require(found>=1,"audio channel count");
 if(named){
  for(unsigned i=0;i<18;++i){
   char expected[64],actual[128];if(i<2)snprintf(expected,sizeof(expected),"Master %u",i+1);else snprintf(expected,sizeof(expected),"Track %c %u",'A'+(i-2)/2,i+1);
   int size=getdesc(fd,3,base+i,b,sizeof(b));require(size>=2&&b[1]==3,"channel string descriptor");unsigned count=(b[0]-2)/2;require(count<sizeof(actual),"channel name bound");
   for(unsigned j=0;j<count;++j){require(!b[3+2*j],"ASCII label encoding");actual[j]=(char)b[2+2*j];}actual[count]=0;
   if(strcmp(actual,expected)){logline("name mismatch channel=%u actual=%s expected=%s",i,actual,expected);finish(0);}
  }
  logline("PASS 18 actual USB string descriptors and stream name base");
 }else logline("PASS stock stereo descriptors after module removal");
 close(fd);
}
static void function(int named){
 char p[512];snprintf(p,sizeof(p),"%s/functions/uac2.audio",root);directory(p);
 if(named){relative("functions/uac2.audio/p_chmask","262143");relative("functions/uac2.audio/c_chmask","3");relative("functions/uac2.audio/p_srate","44100");relative("functions/uac2.audio/c_srate","44100");relative("functions/uac2.audio/p_ssize","4");relative("functions/uac2.audio/c_ssize","4");relative("functions/uac2.audio/shared_clock","1");}
 char target[512];snprintf(target,sizeof(target),"%s/functions/uac2.audio",root);
 snprintf(p,sizeof(p),"%s/configs/c.1/audio",root);require(!symlink(target,p),"link audio function");
 relative("UDC","dummy_udc.0");check_names(named);
 if(named){errno=0;require(unload("am_usb_names")<0,"kernel must refuse unloading active driver");logline("PASS live function reference blocks module unload");}
 relative("UDC","\n");require(!unlink(p),"unlink audio function");snprintf(p,sizeof(p),"%s/functions/uac2.audio",root);require(!rmdir(p),"remove audio instance");
}
int main(void){
 if(getpid()!=1)return 2;
 mkdir("/proc",0755);mkdir("/sys",0755);mkdir("/dev",0755);
 mount("proc","/proc","proc",0,NULL);mount("sysfs","/sys","sysfs",0,NULL);mount("devtmpfs","/dev","devtmpfs",0,NULL);
 FILE*f=fopen("/proc/device-tree/compatible","r");char compatible[128]={0};if(f){size_t read_count=fread(compatible,1,sizeof(compatible)-1,f);compatible[read_count]=0;fclose(f);}
 if(strcmp(compatible,"linux,dummy-virt")){reboot(RB_POWER_OFF);return 3;}
 if(load("/am_vm_console.ko")){reboot(RB_POWER_OFF);return 4;}
 kmsg=open("/dev/kmsg",O_WRONLY);logline("INIT exact firmware kernel, RAM-only fixture");
 directory("/sys/kernel/config");require(!mount("configfs","/sys/kernel/config","configfs",0,NULL),"mount configfs");
 require(!load("/dummy_hcd.ko"),"load dummy USB controller");
 directory(root);relative("idVendor","0x09e8");relative("idProduct","0x205c");
 char p[512];snprintf(p,sizeof(p),"%s/strings/0x409",root);directory(p);relative("strings/0x409/manufacturer","AM test");relative("strings/0x409/product","USB name fixture");relative("strings/0x409/serialnumber","RAM-ONLY-TEST");
 snprintf(p,sizeof(p),"%s/configs/c.1",root);directory(p);
 for(unsigned cycle=0;cycle<2;++cycle){
  require(!load("/am_usb_names.ko"),"load named audio module");function(1);require(!unload("am_usb_names"),"restore stock registry");function(0);
 }
 finish(1);return 0;
}
