#include "src/synth_archive.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <atomic>
#include <thread>
namespace {
template<class T>void set(void*p,size_t at,T v){std::memcpy(static_cast<char*>(p)+at,&v,sizeof v);}
struct Input {
 alignas(8) char wrapper[4][16]{},archive[0x158]{},frame[0x38]{},members[144]{};
 std::string keys[3],values[3];unsigned count=0;
 Input(){for(unsigned i=0;i<4;++i)set<void*>(wrapper[i],8,i==3?archive:wrapper[i+1]);set<uintptr_t>(archive,0x150,reinterpret_cast<uintptr_t>(frame)+0x38);set<unsigned>(frame,0x30,1);}
 static void text(void*p,const std::string&s,bool small){std::memset(p,0,24);
  if(small){assert(s.size()<=21);std::memcpy(p,s.data(),s.size());set<unsigned char>(p,21,21-s.size());set<unsigned short>(p,22,(1u<<10)|(1u<<12));}
  else{set<uint32_t>(p,0,s.size());set<const char*>(p,8,s.data());set<unsigned short>(p,22,1u<<10);}
 }
 void append(const std::string&value,const char*key="amSynthV1",bool small=false){assert(count<3);keys[count]=key;values[count]=value;
  text(members+48*count,keys[count],true);text(members+48*count+24,values[count],small);++count;
  set<uintptr_t>(frame,0,reinterpret_cast<uintptr_t>(members));set<uintptr_t>(frame,8,reinterpret_cast<uintptr_t>(members)+48*count);
 }
};
Input*input;bool success=true,load_error=false,save_error=false,write_error=false,nested=false,edit_on_save=false;
unsigned writes,saves;std::string written,written_key;
am_synth_binding binding{},model{};uint64_t revision;unsigned mode;
void snapshot(){assert(am_synth_archive_snapshot(&model,&revision,&mode));}
std::string encoded(const am_synth_binding&b){size_t n=0;assert(am_synth_binding_encode(&b,nullptr,0,&n)==0);std::string out(n+1,0);assert(am_synth_binding_encode(&b,&out[0],out.size(),nullptr)==0);out.resize(n);return out;}
void writer(void*,const char*key,const void*value){if(write_error)throw std::runtime_error("writer");const char*s;size_t n;std::memcpy(&s,value,sizeof s);std::memcpy(&n,static_cast<const char*>(value)+sizeof s,sizeof n);written.assign(s,n);written_key=key;++writes;}
void original_save(void*,void*){++saves;if(save_error)throw std::runtime_error("save");if(edit_on_save){snapshot();model.carrier=14;assert(am_synth_archive_replace(&model,revision)==0);}}
void original_load(void*,void*){if(load_error)throw std::runtime_error("load");}
int original_read(void*p,void*){if(nested){nested=false;return am_synth_archive_read(p,nullptr,original_read);}am_synth_archive_load(p,input->wrapper,original_load);return success;}
void read(Input&i){input=&i;assert(am_synth_archive_read(nullptr,nullptr,original_read)==success);}
void save(){am_synth_archive_save(nullptr,nullptr,original_save);}
void codec(){
 assert(am_synth_binding_capture(u8"Drum Клавиши",15,&binding));std::string bytes=encoded(binding);am_synth_binding result{};
 assert(am_synth_binding_decode(bytes.data(),bytes.size(),&result)==0&&!std::memcmp(&binding,&result,sizeof result));
 assert(am_synth_binding_matches(&binding,u8"Drum Клавиши",15)&&!am_synth_binding_matches(&binding,"Other",15));
 assert(!am_synth_binding_matches(&binding,u8"Drum Клавиши",14));
 for(size_t i=0;i<bytes.size();++i){result=binding;assert(am_synth_binding_decode(bytes.data(),i,&result)!=0);assert(!std::memcmp(&result,&binding,sizeof result));}
 char small[8];std::memset(small,42,sizeof small);assert(am_synth_binding_encode(&binding,small,sizeof small,nullptr)==AM_SYNTH_BINDING_BUFFER);for(char c:small)assert(c==42);
 result=binding;result.parameters[0]=4;assert(!am_synth_binding_valid(&result));
 result=binding;result.parameters[7]=16;assert(!am_synth_binding_valid(&result));
 result=binding;result.carrier=128;assert(!am_synth_binding_valid(&result));
 result=binding;std::memset(result.program_key,'x',sizeof result.program_key);assert(!am_synth_binding_valid(&result));
 assert(!am_synth_binding_capture("",15,&result));assert(!am_synth_binding_capture("key",128,&result));
 std::string extra=bytes+"x";assert(am_synth_binding_decode(extra.data(),extra.size(),&result)!=0);
 puts("PASS Synth binding codec: exact key/carrier/36 parameters, bounded transactional decoding, invalid/truncated/future payloads");
}
void archive(){
 am_synth_archive_init(reinterpret_cast<uintptr_t>(writer)-0x1d27d90);snapshot();assert(mode==AM_SYNTH_ARCHIVE_LEGACY);save();assert(!writes&&saves==1);
 assert(am_synth_archive_replace(&binding,revision)==0);uint64_t old=revision;assert(am_synth_archive_replace(&binding,old)==AM_SYNTH_EDIT_STALE);
 save();assert(written_key=="amSynthV1"&&written==encoded(binding));
 am_synth_archive_reset();Input known;known.append(encoded(binding));known.append("keep","amMixerV1",true);read(known);snapshot();assert(mode==AM_SYNTH_ARCHIVE_KNOWN&&!std::memcmp(&model,&binding,sizeof model));
 uint64_t before=revision;Input future;future.append(std::string("AMSY9 \0future\n",14));success=false;read(future);success=true;snapshot();assert(revision==before&&mode==AM_SYNTH_ARCHIVE_KNOWN);
 edit_on_save=true;save();edit_on_save=false;assert(written==encoded(binding));snapshot();assert(model.carrier==14);
 read(future);snapshot();assert(mode==AM_SYNTH_ARCHIVE_OPAQUE);save();assert(written==future.values[0]);assert(am_synth_archive_replace(&binding,revision)==AM_SYNTH_EDIT_READONLY);
 Input corrupt;corrupt.append("AMSY1 broken", "amSynthV1",true);read(corrupt);save();assert(written=="AMSY1 broken");
 Input legacy;read(legacy);snapshot();assert(mode==AM_SYNTH_ARCHIVE_LEGACY);unsigned before_writes=writes;save();assert(writes==before_writes);
 assert(am_synth_archive_replace(&binding,revision)==0);snapshot();before=revision;
 Input duplicate;duplicate.append(encoded(binding));duplicate.append(encoded(binding));bool caught=false;try{read(duplicate);}catch(const std::runtime_error&){caught=true;}assert(caught);snapshot();assert(revision==before&&!am_synth_archive_loading());
 load_error=true;caught=false;try{read(known);}catch(const std::runtime_error&){caught=true;}load_error=false;assert(caught);snapshot();assert(revision==before&&!am_synth_archive_loading());
 nested=true;caught=false;try{read(known);}catch(const std::runtime_error&){caught=true;}assert(caught&&!am_synth_archive_loading());
 save_error=true;caught=false;try{save();}catch(const std::runtime_error&){caught=true;}save_error=false;assert(caught);
 write_error=true;caught=false;try{save();}catch(const std::runtime_error&){caught=true;}write_error=false;assert(caught);save();assert(written==encoded(binding));
 std::atomic<bool>done{false};std::atomic<unsigned>reads{0};std::thread observer([&](){uint64_t previous=0;do{am_synth_binding b{};uint64_t r;unsigned m;assert(am_synth_archive_snapshot(&b,&r,&m));assert(r>=previous&&m==AM_SYNTH_ARCHIVE_KNOWN&&am_synth_binding_valid(&b));previous=r;++reads;}while(!done.load());});
 for(unsigned i=0;i<1000;++i){snapshot();model.carrier=i%128;assert(am_synth_archive_replace(&model,revision)==0);}done=true;observer.join();assert(reads>0);
 puts("PASS Synth archive: optional native callback field, read commit/rollback, opaque bytes, stale edits, errors, concurrent snapshots; frames/writer fixtures");
}
}
extern "C" void test_synth_load_binding(const am_synth_binding*b){Input in;in.append(encoded(*b));read(in);}
extern "C" void test_synth_load_opaque(){Input in;in.append("AMSY9 future");read(in);}
extern "C" void am_synth_ui_binding_probe(void);
extern "C" void am_synth_route_probe(void);
int main(){codec();archive();am_synth_ui_binding_probe();am_synth_route_probe();}
