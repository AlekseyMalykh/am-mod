#include "src/mixer_archive.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <atomic>
#include <vector>

namespace {
template<class T>void set(void*p,size_t at,T value){std::memcpy(static_cast<char*>(p)+at,&value,sizeof(value));}
struct Input {
    alignas(8) char wrappers[4][16]{},archive[0x158]{},frame[0x38]{},members[96]{};
    unsigned count=0;
    Input(){for(unsigned i=0;i<4;++i)set<void*>(wrappers[i],8,i==3?archive:wrappers[i+1]);set<uintptr_t>(archive,0x150,reinterpret_cast<uintptr_t>(frame)+0x38);set<unsigned>(frame,0x30,1);}
    static void value(void*p,const std::string&s,bool inline_string=false){
        std::memset(p,0,24);
        if(inline_string){assert(s.size()<=21);std::memcpy(p,s.data(),s.size());set<unsigned char>(p,21,static_cast<unsigned char>(21-s.size()));set<unsigned short>(p,22,(1u<<10)|(1u<<12));}
        else {set<uint32_t>(p,0,static_cast<uint32_t>(s.size()));set<const char*>(p,8,s.data());set<unsigned short>(p,22,1u<<10);}
    }
    void append(const std::string&s,bool short_value=false){assert(count<2);static const std::string key="amMixerV1";
        value(members+count*48,key,true);value(members+count*48+24,s,short_value);++count;
        set<uintptr_t>(frame,0,reinterpret_cast<uintptr_t>(members));set<uintptr_t>(frame,8,reinterpret_cast<uintptr_t>(members)+count*48);
        set<uint64_t>(frame,0x28,count);
    }
};
std::string written,key;unsigned writes=0,original_saves=0,original_loads=0;
bool write_error=false,save_error=false,load_error=false,native_success=true,skip_load=false,edit_on_save=false,nested=false;
Input*input=nullptr;
mixer_project model{},before{};
void writer(void*,const char*name,const void*text){
    if(write_error)throw std::runtime_error("native writer failed");
    const char*ptr;size_t length;std::memcpy(&ptr,text,sizeof(ptr));std::memcpy(&length,static_cast<const char*>(text)+sizeof(ptr),sizeof(length));
    key=name;written.assign(ptr,length);++writes;
}
void original_save(void*,void*){
    ++original_saves;if(save_error)throw std::runtime_error("native save failed");
    if(edit_on_save){uint64_t revision;unsigned mode;assert(mixer_archive_snapshot(&model,&revision,&mode));
        model.track[0].mix.level_db=-30;assert(mixer_archive_replace(&model,revision)==MIXER_EDIT_OK);}
}
void original_load(void*,void*){++original_loads;if(load_error)throw std::runtime_error("native load failed");}
int original_read(void*data,void*){
    if(nested){nested=false;mixer_archive_read(data,nullptr,original_read);}
    if(!skip_load)mixer_archive_load(data,input->wrappers,original_load);
    return native_success;
}
void save(){mixer_archive_save(nullptr,nullptr,original_save);}
void read(Input&i){input=&i;assert(mixer_archive_read(nullptr,nullptr,original_read)==native_success);}
void snapshot(unsigned expected,uint64_t*ticket=nullptr){uint64_t revision;unsigned mode;assert(mixer_archive_snapshot(&model,&revision,&mode));assert(mode==expected);if(ticket)*ticket=revision;}
void legacy(){
    Input old;read(old);snapshot(MIXER_ARCHIVE_LEGACY);
    unsigned count=writes;save();assert(writes==count&&original_saves);
    for(unsigned p=0;p<MIXER_PADS;++p)assert(!model.output[p]);
    puts("PASS old projects remain default-Master and do not acquire a Mixer field until edited");
}
void roundtrip(){
    snapshot(MIXER_ARCHIVE_LEGACY);uint64_t revision;unsigned mode;
    assert(mixer_archive_snapshot(&model,&revision,&mode));model.track[0].mix.level_db=-3;model.output[113]=8;
    model.scene_count=1;model.scene[0].part_id=1003;model.scene[0].fields[0]=MIXER_SCENE_MUTE;model.scene[0].track[0].mute=1;
    assert(mixer_archive_replace(&model,revision)==MIXER_EDIT_OK);
    assert(mixer_archive_replace(&model,revision)==MIXER_EDIT_STALE);
    uint64_t serial=mixer_archive_saved_count();save();assert(mixer_archive_saved_count()==serial+1&&key=="amMixerV1");
    std::string payload=written;assert(mixer_decode(payload.data(),payload.size(),&before)==MIXER_OK);
    mixer_archive_reset();snapshot(MIXER_ARCHIVE_LEGACY);Input next;next.append(payload);read(next);snapshot(MIXER_ARCHIVE_KNOWN);
    assert(model.output[113]==8&&model.track[0].mix.level_db==-3&&model.scene[0].part_id==1003);
    save();assert(written==payload);
    edit_on_save=true;save();edit_on_save=false;
    assert(written==payload);snapshot(MIXER_ARCHIVE_KNOWN);assert(model.track[0].mix.level_db==-30);
    save();assert(written!=payload);
    puts("PASS archive round-trip of one shared project, stale edits rejected and save snapshot immutable during native callbacks");
}
void opaque(){
    const std::vector<std::string> unknown={"future-version",std::string("x\0y\n\"\\",7),std::string(100000,'X'),std::string()};
    for(const auto&s:unknown){Input future;future.append(s,s.size()<=21);read(future);
        uint64_t revision;snapshot(MIXER_ARCHIVE_OPAQUE,&revision);assert(mixer_project_valid(&model));
        model.output[4]=3;assert(mixer_archive_replace(&model,revision)==MIXER_EDIT_READONLY);
        save();assert(written==s&&key=="amMixerV1");
    }
    puts("PASS unknown/corrupt/large/inline/embedded-NUL payloads preserved verbatim and Mixer edits blocked");
}
void errors(){
    snapshot(MIXER_ARCHIVE_OPAQUE);before=model;uint64_t revision;unsigned mode;
    assert(mixer_archive_snapshot(&model,&revision,&mode));
    Input invalid;std::string payload="future";invalid.append(payload);set<unsigned short>(invalid.members+24,22,0);
    input=&invalid;bool threw=false;try{mixer_archive_read(nullptr,nullptr,original_read);}catch(const std::runtime_error&){threw=true;}
    assert(threw);uint64_t after;snapshot(MIXER_ARCHIVE_OPAQUE,&after);assert(after==revision&&!std::memcmp(&model,&before,sizeof(model)));
    Input duplicate;duplicate.append(payload);duplicate.append(payload);input=&duplicate;threw=false;
    try{mixer_archive_read(nullptr,nullptr,original_read);}catch(const std::runtime_error&){threw=true;}assert(threw);
    Input old;input=&old;native_success=false;assert(!mixer_archive_read(nullptr,nullptr,original_read));native_success=true;
    snapshot(MIXER_ARCHIVE_OPAQUE,&after);assert(after==revision);
    load_error=true;threw=false;try{mixer_archive_read(nullptr,nullptr,original_read);}catch(const std::runtime_error&){threw=true;}
    load_error=false;assert(threw);snapshot(MIXER_ARCHIVE_OPAQUE,&after);assert(after==revision);
    nested=true;threw=false;try{mixer_archive_read(nullptr,nullptr,original_read);}catch(const std::runtime_error&){threw=true;}assert(threw);
    uint64_t serial=mixer_archive_saved_count();unsigned count=writes;save_error=true;threw=false;
    try{save();}catch(const std::runtime_error&){threw=true;}save_error=false;assert(threw&&writes==count&&mixer_archive_saved_count()==serial);
    write_error=true;threw=false;try{save();}catch(const std::runtime_error&){threw=true;}write_error=false;assert(threw&&mixer_archive_saved_count()==serial);
    read(old);snapshot(MIXER_ARCHIVE_LEGACY);assert(original_loads);
    puts("PASS failed loads/saves and malformed archive structure preserve owner; exception scopes recover and no false save acknowledgment");
}
void concurrent(){
    mixer_archive_reset();std::atomic<bool>done{false};std::atomic<unsigned>reads{0};
    std::thread observer([&](){mixer_project local{};uint64_t previous=0,revision;unsigned mode;
        do {assert(mixer_archive_snapshot(&local,&revision,&mode));assert(revision>=previous&&mixer_project_valid(&local));previous=revision;++reads;}while(!done.load());
    });
    for(unsigned i=0;i<200;++i){uint64_t revision;unsigned mode;assert(mixer_archive_snapshot(&model,&revision,&mode));
        model.output[i%128]=static_cast<unsigned char>(i%9);assert(mixer_archive_replace(&model,revision)==MIXER_EDIT_OK);}
    done=true;observer.join();assert(reads>0);
    puts("PASS concurrent metadata snapshots remain coherent; no project mutex held across native serialization");
}
}
int main(){
    mixer_archive_init(reinterpret_cast<uintptr_t>(writer)-0x1d27d90);legacy();roundtrip();opaque();errors();concurrent();
    puts("PASS Mixer archive owner and native callback bridge (archive frames/writer fixtures; not physical Save/Load)");return 0;
}
