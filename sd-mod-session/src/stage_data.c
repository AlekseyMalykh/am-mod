#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>

/* Fixed-path SD flush helper. No working-data copy and no mount capability. */
#define RAM_ROOT "/run/mpc-clone-trial-v1"
static void die(const char *s) {
    fprintf(stderr, "DATA_STAGE_FAILED: %s errno=%d\n", s, errno);
    exit(1);
}
static void metadata(const char *name,char *buf,size_t size) {
    char path[256];
    if(snprintf(path,sizeof(path),RAM_ROOT "/storage-meta/%s",name)>=(int)sizeof(path)) die("metadata name");
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    struct stat st; struct statfs fs;
    if(fd<0||fstat(fd,&st)||fstatfs(fd,&fs)||!S_ISREG(st.st_mode)||st.st_uid!=0||
       fs.f_type!=0x01021994||st.st_size<=0||(uint64_t)st.st_size>=size) die("RAM metadata");
    ssize_t n=read(fd,buf,size-1);
    if(n!=st.st_size||close(fd)) die("metadata read");
    if(n>0&&buf[n-1]=='\n') --n;
    buf[n]=0;
}
static int flush_sd(int journal) {
    alarm(10);
    char path[512],identity[128],filesystem[32];
    metadata(journal?"journal-dir":"save-path",path,sizeof(path));
    metadata(journal?"journal-identity":"save-identity",identity,sizeof(identity));
    metadata("sd-filesystem",filesystem,sizeof(filesystem));
    long expected_magic=!strcmp(filesystem,"exfat")?0x2011BAB0:
        !strcmp(filesystem,"vfat")?0x4d44:0;
    if(!expected_magic) die("unsupported SD filesystem");
    if(strncmp(path,"/media/",7)) die("SD prefix");
    const char *end=strchr(path+7,'/');
    if(!end||end==path+7||strcmp(end,journal?"/autorun":"/MPC-Sample")) die("SD fixed suffix");
    for(const char *s=path+7;s<end;++s)
        if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||
             *s=='-'||*s=='_'||*s=='.')) die("SD path characters");
    /* A fixed small output buffer makes fortified realpath abort even for a
       short path. Let libc allocate the required buffer (POSIX.1-2008). */
    char *resolved=realpath(path,NULL);
    if(!resolved) die("SD canonical path");
    int canonical=!strcmp(path,resolved);
    free(resolved);
    if(!canonical) die("SD canonical path");
    int fd=open(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    struct stat st; struct statfs fs;
    if(fd<0||fstat(fd,&st)||fstatfs(fd,&fs)||fs.f_type!=expected_magic) die("SD filesystem changed");
    char actual[128];
    snprintf(actual,sizeof(actual),"%llu:%llu",(unsigned long long)st.st_dev,(unsigned long long)st.st_ino);
    if(strcmp(actual,identity)) die("SD identity changed");
    if(syncfs(fd)||close(fd)) die("SD syncfs");
    puts(journal?"SD_JOURNAL_FLUSHED":"SD_SAVE_FLUSHED");
    return 0;
}
int main(int argc,char **argv) {
    if(argc!=2||(strcmp(argv[1],"--flush-sd")&&strcmp(argv[1],"--flush-journal"))) die("only fixed SD flush modes accepted");
    if(geteuid()!=0) die("root required");
    return flush_sd(!strcmp(argv[1],"--flush-journal"));
}
