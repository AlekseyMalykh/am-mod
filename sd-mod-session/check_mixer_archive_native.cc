#include "src/mixer_archive.h"
extern "C" {
#include "src/music_archive.h"
#include "src/music_settings.h"
}
#include <cassert>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
uintptr_t image;
template<class T>void set(void*p,size_t at,T value){std::memcpy(static_cast<char*>(p)+at,&value,sizeof(value));}
template<class T>T get(const void*p,size_t at){T value;std::memcpy(&value,static_cast<const char*>(p)+at,sizeof(value));return value;}
void patch(uintptr_t address,void*fn){uint32_t jump[]={0x58000050,0xd61f0200};std::memcpy(reinterpret_cast<void*>(image+address),jump,8);std::memcpy(reinterpret_cast<void*>(image+address+8),&fn,8);__builtin___clear_cache(reinterpret_cast<char*>(image+address),reinterpret_cast<char*>(image+address+16));}
struct Output {
    alignas(8) char wrapper[6][0x160]{},writer[0x250]{};
    std::ostringstream stream;std::ostream*base_stream=&stream;
    Output(){for(unsigned i=0;i<6;++i)set<void*>(wrapper[i],8,i==5?writer:wrapper[i+1]);set<void*>(writer,0x108,&base_stream);stream<<"{";}
};
Output*output;
unsigned native_calls=0;
void write_key(void*){
    const char*key=get<const char*>(output->wrapper[3],0x150);
    if(native_calls++)output->stream<<",";
    output->stream<<"\""<<key<<"\":";
}
void prefix(void*,unsigned){}
void native_field(void*archive,const char*key,const std::string&value){
    struct {const char*data;size_t size,capacity,unused;} text={value.data(),value.size(),value.size(),0};
    reinterpret_cast<void(*)(void*,const char*,const void*)>(image+0x1d27d90)(archive,key,&text);
}
void stock_save(void*,void*archive){native_field(archive,"stockSentinel","Keep native fields");}
void legacy_save(void*p,void*a){music_archive_save(p,a,stock_save);}
std::string save(){Output out;output=&out;native_calls=0;mixer_archive_save(nullptr,out.wrapper,legacy_save);out.stream<<"}";output=nullptr;return out.stream.str();}
struct Input {
    alignas(8) char wrapper[4][0x160]{},archive[0x160]{},frame[0x38]{},members[96]{};
    std::string mixer,music;unsigned n=0;
    Input(const std::string&mix,const std::string&mus):mixer(mix),music(mus){
        for(unsigned i=0;i<4;++i)set<void*>(wrapper[i],8,i==3?archive:wrapper[i+1]);
        set<uintptr_t>(archive,0x150,reinterpret_cast<uintptr_t>(frame)+0x38);set<unsigned>(frame,0x30,1);
        add("mpcSampleModV1",music);if(!mixer.empty())add("amMixerV1",mixer);
    }
    void add(const char*key,const std::string&value){char*p=members+48*n++;
        set<uint32_t>(p,0,static_cast<uint32_t>(std::strlen(key)));set<const char*>(p,8,key);set<unsigned short>(p,22,1u<<10);
        p+=24;set<uint32_t>(p,0,static_cast<uint32_t>(value.size()));set<const char*>(p,8,value.data());set<unsigned short>(p,22,1u<<10);
        set<uintptr_t>(frame,0,reinterpret_cast<uintptr_t>(members));set<uintptr_t>(frame,8,reinterpret_cast<uintptr_t>(members)+48*n);
        set<uint64_t>(frame,0x28,n);
    }
};
Input*incoming;
alignas(8) char native_data[0x100]{};
int succeed=1;
void stock_load(void*,void*){}
void legacy_load(void*p,void*a){music_archive_load(p,a,stock_load);}
int stock_read(void*p,void*){mixer_archive_load(p,incoming->wrapper,legacy_load);return succeed;}
int legacy_read(void*p,void*j){return music_archive_read(p,j,stock_read);}
void read(Input&in){incoming=&in;assert(mixer_archive_read(native_data,nullptr,legacy_read)==succeed);incoming=nullptr;}
std::string payload_of_model(){mixer_project project{};uint64_t revision;unsigned mode;
    assert(mixer_archive_snapshot(&project,&revision,&mode));std::vector<char>bytes(MIXER_PAYLOAD_BYTES);size_t n;
    assert(mixer_encode(&project,bytes.data(),bytes.size(),&n)==MIXER_OK);return std::string(bytes.data(),n);
}
}
extern "C" int mixer_archive_native_probe(uintptr_t base)
{
    image=base;mixer_archive_init(base);music_reset();music_archive_init(base);
    /* The pinned named-string function and its real character escaping run.
     * Object-key/prefix service and output stream ownership are fixtures. */
    patch(0x1ae8380,reinterpret_cast<void*>(write_key));patch(0x1ae8100,reinterpret_cast<void*>(prefix));
    static music_project musical{};music_defaults(&musical);musical.tonic=9;musical.scale=1;musical.pad_colours[127]=12;music_replace(&musical);
    std::vector<char>music_bytes(MUSIC_PAYLOAD_BYTES);int length=music_encode(&musical,music_bytes.data(),music_bytes.size());assert(length>0);
    std::string music_payload(music_bytes.data(),length);
    std::string old=save();assert(old.find("amMixerV1")==std::string::npos&&old.find("stockSentinel")!=std::string::npos);
    mixer_project project{};uint64_t revision;unsigned mode;assert(mixer_archive_snapshot(&project,&revision,&mode));
    project.output[127]=8;project.track[3].mix.level_db=-9;assert(mixer_archive_replace(&project,revision)==MIXER_EDIT_OK);
    std::string payload=payload_of_model(),known=save();assert(known.find(payload)!=std::string::npos);
    mixer_archive_reset();music_reset();Input loaded(payload,music_payload);read(loaded);
    assert(mixer_archive_snapshot(&project,&revision,&mode)&&mode==MIXER_ARCHIVE_KNOWN&&project.output[127]==8&&project.track[3].mix.level_db==-9);
    music_snapshot(&musical);assert(musical.tonic==9&&musical.scale==1&&musical.pad_colours[127]==12);
    assert(payload_of_model()==payload);
    succeed=0;Input discarded("future",music_payload);read(discarded);succeed=1;
    assert(mixer_archive_snapshot(&project,&revision,&mode)&&mode==MIXER_ARCHIVE_KNOWN&&project.output[127]==8);
    /* Exercise the actual native writer's size-based (not strlen) escaping
     * and the bridge's opaque retention, including embedded NUL. */
    std::string future("X\0\n\"\\Z",6);Input unsupported(future,music_payload);read(unsupported);
    assert(mixer_archive_snapshot(&project,&revision,&mode)&&mode==MIXER_ARCHIVE_OPAQUE);
    std::string opaque=save();assert(opaque.find("\"amMixerV1\":\"X\\u0000\\n\\\"\\\\Z\"")!=std::string::npos);
    /* Verify declared lengths in native RapidJSON long strings against the
     * firmware's own std::string reader for an ordinary V1 payload. */
    auto frame=loaded.frame;set<uint64_t>(frame,0x20,1);std::string native_read;
    reinterpret_cast<void(*)(void*,void*)>(image+0x1457dd0)(loaded.archive,&native_read);assert(native_read==payload);
    std::puts("PASS native JSON named-string writer/escaping and reader length ABI with real music_archive/model bridge; object framing/services fixtures");
    std::puts("PASS Mixer and existing music metadata coexist, failed native read preserves both owners, opaque embedded-NUL bytes survive native serialization");
    return 0;
}
