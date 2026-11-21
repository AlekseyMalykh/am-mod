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
#ifndef RAM_ROOT
#define RAM_ROOT "/run/mpc-clone-trial-v1"
#endif
#ifndef AM_STAGE_INTERNAL_PROJECTS
#define AM_STAGE_INTERNAL_PROJECTS "/data/UserContent/Projects"
#endif
#ifndef AM_STAGE_INTERNAL_SAMPLES
#define AM_STAGE_INTERNAL_SAMPLES "/data/UserContent/Samples/User"
#endif
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
static int flush_internal(void) {
    alarm(10);
    char marker[192],expected[192];metadata("internal-storage",marker,sizeof marker);
    const char *paths[]={AM_STAGE_INTERNAL_PROJECTS,AM_STAGE_INTERNAL_SAMPLES};
    struct stat st[2];int fd[2]={-1,-1};
    for(unsigned i=0;i<2;++i){
        char *resolved=realpath(paths[i],NULL);if(!resolved)die("internal canonical path");
        int same=!strcmp(resolved,paths[i]);free(resolved);if(!same)die("internal redirected path");
        fd[i]=open(paths[i],O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        struct statfs fs;
        if(fd[i]<0||fstat(fd[i],&st[i])||fstatfs(fd[i],&fs)||fs.f_type!=0xef53)die("internal filesystem changed");
    }
    snprintf(expected,sizeof expected,"AM_INTERNAL1\n%llu:%llu\n%llu:%llu",
       (unsigned long long)st[0].st_dev,(unsigned long long)st[0].st_ino,
       (unsigned long long)st[1].st_dev,(unsigned long long)st[1].st_ino);
    if(strcmp(marker,expected)||st[0].st_dev!=st[1].st_dev)die("internal identity changed");
    if(syncfs(fd[0])||close(fd[0])||close(fd[1]))die("internal syncfs");
    puts("INTERNAL_USER_STORAGE_FLUSHED");return 0;
}
int main(int argc,char **argv) {
    if(argc!=2||(strcmp(argv[1],"--flush-sd")&&strcmp(argv[1],"--flush-journal")&&strcmp(argv[1],"--flush-internal"))) die("only fixed storage flush modes accepted");
    if(geteuid()!=0) die("root required");
    if(!strcmp(argv[1],"--flush-internal"))return flush_internal();
    return flush_sd(!strcmp(argv[1],"--flush-journal"));
}
