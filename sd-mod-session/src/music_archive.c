#define _GNU_SOURCE
#include "music_archive.h"
#include "music_settings.h"
#ifdef MPC_COMPRESSOR
#include "compressor.h"
#endif
#ifdef MPC_PROJECT_RECOVERY
#include "project_recovery.h"
#endif
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The pinned native serializer owns compression, temporary files and commit.
 * Add one optional named JSON field inside its existing data object. No raw
 * XPJ rewriting, output redirection, sidecar files or additional write scope.
 * Native readers use named-member lookup and ignore unrequested fields. */
static uintptr_t base;
static _Atomic unsigned records,status;
static _Atomic unsigned saved_count;
static _Thread_local struct music_project *loading;
static _Thread_local int loading_seen;
struct settings_snapshot { uint64_t display,compressor; int valid; };
static _Thread_local struct settings_snapshot *loading_settings;
#ifdef MPC_PROJECT_RECOVERY
static _Thread_local char *loading_recovery;
#endif
static _Thread_local void *saving_data;
static _Thread_local uint64_t saving_key;
static _Thread_local int saving_override;
#define FIELD(p,off,type) (*(type *)((char *)(p)+(off)))
#define FN(off,type) ((type)(base+(off)))
static const char extension[]="mpcSampleModV1";
static const char settings_extension[]="mpcSampleModSettingsV1";
/* Fixed-length hex accepts no whitespace, overflows, trailing data or
 * partially valid settings. Commit only with a successful native read. */
static int settings_decode(const char*s,struct settings_snapshot*out){
 if(!s||strlen(s)<5)return 0;
 int extended=!memcmp(s,"AMS3 ",5);
 unsigned digits=extended||!memcmp(s,"AMS2 ",5)?16:13;
 unsigned length=5+digits+1+13+1;
 if(strlen(s)!=length||(!extended&&memcmp(s,"AMS1 ",5)&&memcmp(s,"AMS2 ",5))||s[5+digits]!=' '||s[length-1]!='\n')return 0;
 uint64_t values[2]={0,0};
 for(unsigned k=0;k<2;++k)for(unsigned i=0;i<(k?13:digits);++i){unsigned c=(unsigned char)s[k?6+digits+i:5+i];
  if(c>='0'&&c<='9')c-='0';else if(c>='a'&&c<='f')c=c-'a'+10;else return 0;
  values[k]=(values[k]<<4)|c;}
 if((!extended&&(values[0]>>61))||!music_mod_settings_valid(values[0]))return 0;
#ifdef MPC_COMPRESSOR
 if(!compressor_project_valid(values[1]))return 0;
#else
 if(values[1]>>50)return 0;
#endif
 out->display=values[0];out->compressor=values[1];out->valid=1;return 1;
}
void music_archive_init(uintptr_t b) { base=b; }
int music_archive_status(void) { return (int)atomic_load(&status); }
unsigned music_archive_saved_count(void) { return atomic_load(&saved_count); }
static void report(const char *message)
{ if (atomic_fetch_add(&records,1)<32) fprintf(stderr,"MUSIC_ARCHIVE %s\n",message); }

/* RapidJSON values in this native build are 24 bytes. Object members are
 * name/value pairs of 48 bytes. These exact flags/frames are read by native
 * JSONInputArchive at 0x14579f0 / 0x1457dd0. Never mutate the input tree. */
static const char *json_string(void *value,size_t max)
{
    unsigned flags=FIELD(value,0x16,unsigned short);
    if (!(flags&(1u<<10))) return NULL;
    const char *s=flags&(1u<<12) ? (const char*)value : FIELD(value,8,const char*);
    if (!s || strnlen(s,max)>=max) return NULL;
    return s;
}
static const char *optional_named_payload(void *wrapper,const char *field,size_t max)
{
    void *a=wrapper;
    for (unsigned i=0;i<4;++i) { if (!a) return NULL; a=FIELD(a,8,void*); }
    if (!a) return NULL;
    uintptr_t position=FIELD(a,0x150,uintptr_t);
    if (!position) return NULL;
    void *frame=(void*)(position-0x38);
    if (FIELD(frame,0x30,unsigned)!=1) return NULL;
    uintptr_t begin=FIELD(frame,0,uintptr_t),end=FIELD(frame,8,uintptr_t);
    if (!begin || end<begin || (end-begin)%48 || end-begin>48*1024) return NULL;
    for (uintptr_t p=begin;p<end;p+=48) {
        const char *name=json_string((void*)p,256);
        if (name && !strcmp(name,field))
            return json_string((void*)(p+24),max);
    }
    return NULL;
}

void music_archive_load(void *data,void *archive,void (*original)(void*,void*))
{
    original(data,archive);
    if (!loading) return;
    /* A native project already has a key. Use it if no mod extension exists. */
    music_defaults(loading);
    const char *key=FIELD(data,0x28,const char*);
    if (key && strnlen(key,80)<80) music_key_from_text(key,&loading->tonic,&loading->scale);
    const char *payload=optional_named_payload(archive,extension,MUSIC_PAYLOAD_BYTES);
    if(loading_settings){
        const char*settings=optional_named_payload(archive,settings_extension,128);
        if(settings&&!settings_decode(settings,loading_settings))report("INVALID_SETTINGS_IGNORED");
    }
#ifdef MPC_PROJECT_RECOVERY
    const char *recovery=optional_named_payload(archive,"mpcSampleModRecoveryV1",PROJECT_RECOVERY_BYTES);
    if(loading_recovery&&recovery)memcpy(loading_recovery,recovery,strlen(recovery)+1);
#endif
    if (payload) {
        size_t size=strnlen(payload,MUSIC_PAYLOAD_BYTES);
        if (!music_decode(payload,size,loading)) {
            music_defaults(loading);
            if (key && strnlen(key,80)<80) music_key_from_text(key,&loading->tonic,&loading->scale);
            report("INVALID_EXTENSION_IGNORED native_project_preserved");
        } else report("EXTENSION_DECODED");
    } else report("NATIVE_PROJECT_WITHOUT_EXTENSION defaults");
    loading_seen=1;
}

int music_archive_read(void *data,void *json,int (*original)(void*,void*))
{
    struct music_project *previous=loading;
    int previous_seen=loading_seen;
    struct settings_snapshot pending_settings={0,0,0},*previous_settings=loading_settings;
    loading_settings=&pending_settings;
#ifdef MPC_PROJECT_RECOVERY
    char recovery[PROJECT_RECOVERY_BYTES]={0};char *previous_recovery=loading_recovery;loading_recovery=recovery;
#endif
    struct music_project *pending=calloc(1,sizeof(*pending));
    loading=pending; loading_seen=0;
    int success=original(data,json);
    if (success) {
        if (pending && loading_seen) music_replace(pending);
        else music_reset();
        if(pending_settings.valid)music_mod_settings_restore(pending_settings.display);
#ifdef MPC_COMPRESSOR
        if(pending_settings.valid)compressor_project_restore(pending_settings.compressor);
        else compressor_project_reset();
#endif
        atomic_store(&status,0); report("PROJECT_READ_COMMITTED");
#ifdef MPC_PROJECT_RECOVERY
        project_recovery_loaded(recovery);
#endif
    }
    loading=previous; loading_seen=previous_seen; free(pending);
    loading_settings=previous_settings;
#ifdef MPC_PROJECT_RECOVERY
    loading_recovery=previous_recovery;
#endif
    return success;
}

void music_archive_key(void *data,void *archive,const void *pair,void (*original)(void*,void*,const void*))
{
    const char *name=FIELD(pair,0,const char*);
    if (data==saving_data && saving_key && name && !strcmp(name,"key")) {
        const void *native_string=FIELD(pair,8,const void*);
        const char *native=native_string ? FIELD(native_string,0,const char*) : NULL;
        int tonic,scale;
        /* An imported project can use native modes outside this first UI.
         * Preserve its key unless the user explicitly selected our new key. */
        if (!saving_override && (!native || strnlen(native,80)>=80 || !music_key_from_text(native,&tonic,&scale))) {
            original(data,archive,pair); return;
        }
        const void *replacement[2]={name,&saving_key};
        original(data,archive,replacement);
    } else original(data,archive,pair);
}

void music_archive_save(void *data,void *archive,void (*original)(void*,void*))
{
    struct music_project *snapshot=malloc(sizeof(*snapshot));
    char *payload=malloc(MUSIC_PAYLOAD_BYTES);
    if (!snapshot || !payload) {
        free(snapshot); free(payload); original(data,archive);
        atomic_store(&status,2); report("METADATA_ALLOCATION_FAILED native_save_only"); return;
    }
    music_snapshot(snapshot);
    uint64_t display_settings=music_mod_settings_snapshot(),compressor_settings=0;
#ifdef MPC_COMPRESSOR
    compressor_settings=compressor_project_snapshot();
#endif
    int length=music_encode(snapshot,payload,MUSIC_PAYLOAD_BYTES);
    if (!length) {
        free(snapshot); free(payload); original(data,archive);
        atomic_store(&status,2); report("METADATA_ENCODING_FAILED native_save_only"); return;
    }
    void *previous_data=saving_data; uint64_t previous_key=saving_key;
    int previous_override=saving_override; saving_override=music_key_edited();
    char key[48]; music_key_text(snapshot->tonic,snapshot->scale,key,sizeof(key));
    saving_data=data; saving_key=0;
    FN(0xf535c0,void(*)(uint64_t*,const char*))(&saving_key,key);
    original(data,archive);
    /* Native std::string layout: data, size, capacity/short-string storage.
     * 0x1d27d90 only reads data/size; it neither owns nor modifies this view. */
    struct { const char *data; size_t size,capacity,unused; } text={payload,(size_t)length,(size_t)length,0};
    FN(0x1d27d90,void(*)(void*,const char*,const void*))(archive,extension,&text);
    char settings[38];
    int settings_size=snprintf(settings,sizeof(settings),"AMS%c %016llx %013llx\n",display_settings>>61?'3':'2',(unsigned long long)display_settings,(unsigned long long)compressor_settings);
    if(settings_size==36){
        struct {const char*data;size_t size,capacity,unused;} value={settings,36,36,0};
        FN(0x1d27d90,void(*)(void*,const char*,const void*))(archive,settings_extension,&value);
    }
#ifdef MPC_PROJECT_RECOVERY
    const char *recovery=project_recovery_copy_payload();
    if(recovery){
        struct { const char *data; size_t size,capacity,unused; } value={recovery,strlen(recovery),strlen(recovery),0};
        FN(0x1d27d90,void(*)(void*,const char*,const void*))(archive,"mpcSampleModRecoveryV1",&value);
    }
#endif
    FN(0xf0a290,void(*)(uint64_t*))(&saving_key);
    saving_data=previous_data; saving_key=previous_key; saving_override=previous_override;
    free(snapshot); free(payload);
    atomic_fetch_add(&saved_count,1);
    atomic_store(&status,1); report("EXTENSION_WRITTEN native_archive");
}
