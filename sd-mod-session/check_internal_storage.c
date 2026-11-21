#define _GNU_SOURCE
#include "src/internal_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
int main(int argc,char **argv){
 assert(argc==2);const char *root=argv[1];char p[4096],sample[4096],meta[4096],file[8192];
 snprintf(p,sizeof p,"%s/Projects",root);snprintf(sample,sizeof sample,"%s/Samples",root);snprintf(meta,sizeof meta,"%s/meta",root);
 assert(!mkdir(p,0700)&&!mkdir(sample,0700));assert(!internal_storage_enabled());
 struct stat a,b;assert(!stat(p,&a)&&!stat(sample,&b));
 FILE *out=fopen(meta,"w");assert(out);fprintf(out,"AM_INTERNAL1\n%llu:%llu\n%llu:%llu\n",(unsigned long long)a.st_dev,(unsigned long long)a.st_ino,(unsigned long long)b.st_dev,(unsigned long long)b.st_ino);assert(!fclose(out));
 assert(internal_storage_enabled());snprintf(file,sizeof file,"%s/Song.xpj",p);assert(internal_storage_project_path(file));
 out=fopen(file,"w");assert(out);fputs("project with native samples",out);assert(!fclose(out));assert(internal_storage_project_path(file));
 char nested[8192];snprintf(nested,sizeof nested,"%s/../escaped.xpj",p);assert(!internal_storage_project_path(nested));
 snprintf(nested,sizeof nested,"%s//bad.xpj",p);assert(!internal_storage_project_path(nested));
 snprintf(nested,sizeof nested,"%s/no-folder/Song.xpj",p);assert(!internal_storage_project_path(nested));
 snprintf(nested,sizeof nested,"%s/raw.bin",p);assert(!internal_storage_project_path(nested));
 snprintf(nested,sizeof nested,"%s/linked.xpj",p);assert(!symlink(file,nested)&&!internal_storage_project_path(nested)&&!unlink(nested));
 assert(!link(file,nested)&&!internal_storage_project_path(file)&&!unlink(nested));
 snprintf(nested,sizeof nested,"%s/voice.wav",sample);assert(internal_storage_sample_path(nested));
 char exports[8192];snprintf(exports,sizeof(exports),"%s/Export",sample);assert(!mkdir(exports,0700));
 snprintf(nested,sizeof nested,"%s/Export/mixdown.wav",sample);assert(internal_storage_sample_path(nested));
 snprintf(nested,sizeof nested,"%s/Export/../outside.wav",sample);assert(!internal_storage_sample_path(nested));
 assert(!rmdir(exports));
 assert(!internal_storage_sample_path(file)&&!internal_storage_project_path(nested));
 assert(!internal_storage_project_path("/etc/passwd")&&!internal_storage_sample_path("/data/System/config"));
 out=fopen(meta,"w");assert(out);fputs("AM_INTERNAL1\n0:0\n0:0\n",out);assert(!fclose(out));assert(!internal_storage_enabled()&&!internal_storage_project_path(file));
 assert(!unlink(meta));assert(!symlink(file,meta));assert(!internal_storage_enabled());assert(!unlink(meta));
 assert(!unlink(file)&&!rmdir(p)&&!rmdir(sample));
 puts("PASS persistent internal capability: exact directory identities, native project/sample destinations, existing files, traversal/missing folders/symlink/hardlink rejection, stale and redirected metadata; no system paths");
 return 0;
}
