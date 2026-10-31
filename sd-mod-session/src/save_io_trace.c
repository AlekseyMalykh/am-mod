#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/syscall.h>
#include <unistd.h>

/* Diagnostic only: forward original arguments/results and preserve errno.
 * No observer thread, redirection, retries, permission changes or SD log.
 * Append at most 1024 short records to the existing RAM app directory. */
static _Thread_local int inside;
static atomic_uint records;
static atomic_uint filesystem_records;
static int ready;
static int (*next_open)(const char *, int, ...);
static int (*next_open64)(const char *, int, ...);
static int (*next_openat)(int, const char *, int, ...);
static int (*next_openat64)(int, const char *, int, ...);
static FILE *(*next_fopen)(const char *, const char *);
static FILE *(*next_fopen64)(const char *, const char *);
static int (*next_mkdir)(const char *, mode_t);
static int (*next_mkdirat)(int, const char *, mode_t);
static int (*next_rename)(const char *, const char *);
static int (*next_renameat)(int, const char *, int, const char *);
static int (*next_unlink)(const char *);
static int (*next_unlinkat)(int, const char *, int);
static int (*next_access)(const char *, int);
static int (*next_euidaccess)(const char *, int);
static int (*next_faccessat)(int, const char *, int, int);
static int (*next_statfs)(const char *, struct statfs *);

static int relevant(const char *p) {
    return p && (!strncmp(p,"/media/",7) || !strncmp(p,"/data/",6) ||
        !strncmp(p,"/mnt/",5) || strstr(p,"MPC-Sample"));
}
static void note(const char *op,int dir,const char *p,const char *q,
                 long result,int error,int detail) {
    int saved=errno;
    if (!ready || inside || (!relevant(p) && !relevant(q) && result>=0)) return;
    inside=1;
    unsigned n=atomic_fetch_add_explicit(&records,1,memory_order_relaxed);
    if (n<1024) {
        char line[1600];
        int length=snprintf(line,sizeof(line),
            "SAVE_IO n=%u op=%s dirfd=%d result=%ld errno=%d detail=%d path=\"%.600s\" to=\"%.600s\"\n",
            n,op,dir,result,error,detail,p?p:"(null)",q?q:"");
        if (length>0 && length<(int)sizeof(line)) {
            int fd=(int)syscall(SYS_openat,AT_FDCWD,
                "/run/mpc-trial-app/save-io.txt",
                O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW|O_CLOEXEC,0600);
            if (fd>=0) { (void)syscall(SYS_write,fd,line,(size_t)length); (void)syscall(SYS_close,fd); }
        }
    }
    inside=0;
    errno=saved;
}
__attribute__((constructor)) static void setup(void) {
#define LOAD(name) next_##name=dlsym(RTLD_NEXT,#name)
    inside=1;
    LOAD(open); LOAD(open64); LOAD(openat); LOAD(openat64);
    LOAD(fopen); LOAD(fopen64); LOAD(mkdir); LOAD(mkdirat);
    LOAD(rename); LOAD(renameat); LOAD(unlink); LOAD(unlinkat);
    LOAD(access); LOAD(euidaccess); LOAD(faccessat); LOAD(statfs);
    inside=0;
    ready=1;
    note("loaded",AT_FDCWD,"/run/mpc-trial-app/save-io.so",NULL,-1,0,0);
}
static int writes(int flags) {
    return (flags&O_ACCMODE)!=O_RDONLY || (flags&(O_CREAT|O_TRUNC));
}
static int needs_mode(int flags) {
    return (flags&O_CREAT) || (flags&O_TMPFILE)==O_TMPFILE;
}
#define OPEN_WRAPPER(name) \
int name(const char *p,int flags,...) { \
    mode_t mode=0; if(needs_mode(flags)) {va_list a;va_start(a,flags);mode=va_arg(a,mode_t);va_end(a);} \
    int r=next_##name?next_##name(p,flags,mode):(int)syscall(SYS_openat,AT_FDCWD,p,flags,mode); \
    int e=errno;if(writes(flags)||r<0)note(#name,AT_FDCWD,p,NULL,r,r<0?e:0,flags);errno=e;return r; \
}
OPEN_WRAPPER(open)
OPEN_WRAPPER(open64)
#define OPENAT_WRAPPER(name) \
int name(int dir,const char *p,int flags,...) { \
    mode_t mode=0;if(needs_mode(flags)){va_list a;va_start(a,flags);mode=va_arg(a,mode_t);va_end(a);} \
    int r=next_##name?next_##name(dir,p,flags,mode):(int)syscall(SYS_openat,dir,p,flags,mode); \
    int e=errno;if(writes(flags)||r<0)note(#name,dir,p,NULL,r,r<0?e:0,flags);errno=e;return r; \
}
OPENAT_WRAPPER(openat)
OPENAT_WRAPPER(openat64)
#define FOPEN_WRAPPER(name) \
FILE *name(const char *p,const char *mode) { \
    if(!next_##name)next_##name=dlsym(RTLD_NEXT,#name); \
    if(!next_##name){errno=ENOSYS;return NULL;} \
    FILE *r=next_##name(p,mode);int e=errno; \
    if(!r||strpbrk(mode,"wa+")){note(#name,AT_FDCWD,p,NULL,r?0:-1,r?0:e,0);}errno=e;return r; \
}
FOPEN_WRAPPER(fopen)
FOPEN_WRAPPER(fopen64)
int mkdir(const char *p,mode_t mode) {
    int r=next_mkdir?next_mkdir(p,mode):(int)syscall(SYS_mkdirat,AT_FDCWD,p,mode);
    int e=errno;note("mkdir",AT_FDCWD,p,NULL,r,r<0?e:0,(int)mode);errno=e;return r;
}
int mkdirat(int dir,const char *p,mode_t mode) {
    int r=next_mkdirat?next_mkdirat(dir,p,mode):(int)syscall(SYS_mkdirat,dir,p,mode);
    int e=errno;note("mkdirat",dir,p,NULL,r,r<0?e:0,(int)mode);errno=e;return r;
}
int rename(const char *p,const char *q) {
    int r=next_rename?next_rename(p,q):(int)syscall(SYS_renameat,AT_FDCWD,p,AT_FDCWD,q);
    int e=errno;note("rename",AT_FDCWD,p,q,r,r<0?e:0,AT_FDCWD);errno=e;return r;
}
int renameat(int dir,const char *p,int dir2,const char *q) {
    int r=next_renameat?next_renameat(dir,p,dir2,q):(int)syscall(SYS_renameat,dir,p,dir2,q);
    int e=errno;note("renameat",dir,p,q,r,r<0?e:0,dir2);errno=e;return r;
}
int unlink(const char *p) {
    int r=next_unlink?next_unlink(p):(int)syscall(SYS_unlinkat,AT_FDCWD,p,0);
    int e=errno;note("unlink",AT_FDCWD,p,NULL,r,r<0?e:0,0);errno=e;return r;
}
int unlinkat(int dir,const char *p,int flags) {
    int r=next_unlinkat?next_unlinkat(dir,p,flags):(int)syscall(SYS_unlinkat,dir,p,flags);
    int e=errno;note("unlinkat",dir,p,NULL,r,r<0?e:0,flags);errno=e;return r;
}
int access(const char *p,int mode) {
    int r=next_access?next_access(p,mode):(int)syscall(SYS_faccessat,AT_FDCWD,p,mode);
    int e=errno;if(r<0)note("access",AT_FDCWD,p,NULL,r,e,mode);errno=e;return r;
}
int euidaccess(const char *p,int mode) {
    if(!next_euidaccess)next_euidaccess=dlsym(RTLD_NEXT,"euidaccess");
    if(!next_euidaccess){errno=ENOSYS;return -1;}
    int r=next_euidaccess(p,mode);
    int e=errno;if(r<0)note("euidaccess",AT_FDCWD,p,NULL,r,e,mode);errno=e;return r;
}
int faccessat(int dir,const char *p,int mode,int flags) {
    /* glibc handles faccessat flags/emulation; do not substitute older syscall. */
    if(!next_faccessat){errno=ENOSYS;return -1;}
    int r=next_faccessat(dir,p,mode,flags);
    int e=errno;if(r<0)note("faccessat",dir,p,NULL,r,e,mode);errno=e;return r;
}
int statfs(const char *p,struct statfs *st) {
    int r=next_statfs?next_statfs(p,st):(int)syscall(SYS_statfs,p,st);
    int e=errno;
    if(relevant(p) && atomic_fetch_add_explicit(&filesystem_records,1,memory_order_relaxed)<16)
        note("statfs_flags",AT_FDCWD,p,NULL,r,r<0?e:0,r<0?0:(int)st->f_flags);
    errno=e;return r;
}
