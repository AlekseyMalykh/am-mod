#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
static volatile sig_atomic_t child;
static void expired(int sig){(void)sig;if(child>0)kill(child,SIGTERM);_exit(124);}
int main(int argc,char**argv){
 if(argc!=3)return 2;
 struct sigaction action={0};action.sa_handler=expired;sigemptyset(&action.sa_mask);sigaction(SIGALRM,&action,0);
 if(!strcmp(argv[1],"--run")){
  const char*p=argv[2];size_t n=strlen(p);const char*suffix="/autorun/power-audit.sh";
  if(n>=1024||strncmp(p,"/media/",7)||strstr(p,"/../")||n<strlen(suffix)||strcmp(p+n-strlen(suffix),suffix))return 2;
  child=fork();if(child<0)return 1;
  if(!child){execl("/bin/sh","sh",p,(char*)0);_exit(127);}
  alarm(20);int status;while(waitpid(child,&status,0)<0){if(errno!=EINTR)return 1;}
  alarm(0);return WIFEXITED(status)?WEXITSTATUS(status):1;
 }
 if(strcmp(argv[1],"--read"))return 2;
 const char*p=argv[2];
 int allowed=!strncmp(p,"/sys/class/power_supply/",24)||!strncmp(p,"/sys/class/typec/",17)||!strncmp(p,"/sys/class/usb_role/",20)||!strncmp(p,"/sys/class/extcon/",18)||!strncmp(p,"/sys/bus/i2c/devices/",21)||!strcmp(p,"/proc/uptime")||!strncmp(p,"/proc/device-tree/",18);
#ifdef POWER_AUDIT_TEST
 allowed|=!strncmp(p,"/tmp/mpc-power-audit-",21);
#endif
 if(!allowed||strstr(p,"/../"))return 2;
 alarm(1);int fd=open(p,O_RDONLY|O_NONBLOCK|O_CLOEXEC);if(fd<0)return 1;
 char buffer[8192];ssize_t n;do{n=read(fd,buffer,sizeof(buffer));}while(n<0&&errno==EINTR);
 close(fd);if(n<0)return 1;
 for(ssize_t at=0;at<n;){ssize_t sent=write(1,buffer+at,(size_t)(n-at));if(sent<0&&errno==EINTR)continue;if(sent<=0)return 1;at+=sent;}
 alarm(0);return 0;
}
