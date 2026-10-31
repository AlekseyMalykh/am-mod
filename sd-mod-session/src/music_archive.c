#define _GNU_SOURCE
#include "music_archive.h"
#include "music_settings.h"
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
static _Thread_local void *saving_data;
static _Thread_local uint64_t saving_key;
static _Thread_local int saving_override;
#define FIELD(p,off,type) (*(type *)((char *)(p)+(off)))
#define FN(off,type) ((type)(base+(off)))
static const char extension[]="mpcSampleModV1";
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
static const char *optional_payload(void *wrapper)
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
        if (name && !strcmp(name,extension))
            return json_string((void*)(p+24),MUSIC_PAYLOAD_BYTES);
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
    const char *payload=optional_payload(archive);
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
    struct music_project *pending=calloc(1,sizeof(*pending));
    loading=pending; loading_seen=0;
    int success=original(data,json);
    if (success) {
        if (pending && loading_seen) music_replace(pending);
        else music_reset();
        atomic_store(&status,0); report("PROJECT_READ_COMMITTED");
    }
    loading=previous; loading_seen=previous_seen; free(pending);
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
    FN(0xf0a290,void(*)(uint64_t*))(&saving_key);
    saving_data=previous_data; saving_key=previous_key; saving_override=previous_override;
    free(snapshot); free(payload);
    atomic_fetch_add(&saved_count,1);
    atomic_store(&status,1); report("EXTENSION_WRITTEN native_archive");
}
