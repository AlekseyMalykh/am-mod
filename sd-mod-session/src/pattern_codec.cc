#include "pattern_library.h"
#include <cstring>
#include <set>
#include <stdexcept>
namespace am_pattern {
namespace {
constexpr std::size_t byte_limit=4*1024*1024;
struct Writer {
    std::string bytes;
    void number(std::uint64_t value,unsigned count){for(unsigned i=0;i<count;++i){bytes.push_back(char(value&255));value>>=8;}}
    void text(const std::string&s,unsigned limit){if(s.size()>limit||s.find('\0')!=std::string::npos)throw std::runtime_error("Pattern string limit");number(s.size(),1);bytes+=s;}
};
struct Reader {
    const std::string&bytes;std::size_t at=0;
    explicit Reader(const std::string&value):bytes(value){}
    std::uint64_t number(unsigned count){if(count>bytes.size()-at)throw std::runtime_error("Truncated patterns");std::uint64_t v=0;for(unsigned i=0;i<count;++i)v|=std::uint64_t((unsigned char)bytes[at++])<<(8*i);return v;}
    std::string text(unsigned limit){auto size=number(1);if(size>limit||size>bytes.size()-at)throw std::runtime_error("Invalid pattern string");std::string s=bytes.substr(at,size);at+=size;if(s.find('\0')!=std::string::npos)throw std::runtime_error("Pattern NUL");return s;}
};
std::uint32_t checksum(const std::string&s){std::uint32_t h=2166136261u;for(unsigned char c:s)h=(h^c)*16777619u;return h;}
}
std::string library_encode(const Library&library){
    Writer w;w.number(library.next,8);w.number(library.revision,8);w.number(library.entries.size(),1);
    for(const auto&entry:library.entries){const auto&p=entry.pattern;
        w.number(p.id(),8);w.number(p.parent(),8);w.number(p.revision(),8);w.number(std::uint64_t(p.length()),8);w.number(entry.part,1);w.text(entry.name,63);
        w.number(p.routes().size(),2);for(const auto&r:p.routes()){w.text(r.program_key,127);w.number(r.pad,1);w.number(r.source_note,1);w.number(r.mixer_bus+2,1);}
        w.number(p.events().size(),4);for(const auto&e:p.events()){
            w.number(e.id,8);w.number(e.source_snapshot,8);w.number(e.source_event,8);w.number(e.route,2);
            w.number(std::uint64_t(e.native.time),8);w.number(e.native.type&~0x100u,4);
            w.bytes.append(reinterpret_cast<const char*>(e.native.data),37);
        }
    }
    if(w.bytes.size()>byte_limit)throw std::runtime_error("Pattern archive limit");
    w.number(checksum(w.bytes),4);std::string result="AMP1 ";result.reserve(6+w.bytes.size()*2);
    const char hex[]="0123456789abcdef";for(unsigned char c:w.bytes){result+=hex[c>>4];result+=hex[c&15];}result+='\n';return result;
}
Result library_decode(const std::string&text,am_midi_choice_get get,Library&out){
    if(text.compare(0,5,"AMP1 "))return Result::unsupported;
    if(text.size()<6||(text.size()-6)%2||text.size()>6+2*(byte_limit+4)||text.back()!='\n')return Result::invalid;
    try{
        auto digit=[](unsigned char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;};
        std::string bytes;bytes.reserve((text.size()-6)/2);
        for(std::size_t i=5;i+1<text.size()-1;i+=2){int a=digit(text[i]),b=digit(text[i+1]);if(a<0||b<0)return Result::invalid;bytes+=char(a*16+b);}
        if(bytes.size()<4)return Result::invalid;
        Reader tail{bytes};tail.at=bytes.size()-4;auto sum=tail.number(4);bytes.resize(bytes.size()-4);
        if(checksum(bytes)!=sum)return Result::invalid;
        Reader r{bytes};Library value;value.next=r.number(8);value.revision=r.number(8);auto count=r.number(1);
        if(!value.next||!value.revision||count>library_limit)return Result::invalid;
        std::size_t total=0;std::set<Id>ids;
        for(unsigned i=0;i<count;++i){
            auto id=r.number(8),parent=r.number(8),revision=r.number(8),length=r.number(8);auto part=r.number(1);
            if(!id||id>=value.next||parent>=value.next||!ids.insert(id).second||part>1||length>INT64_MAX)return Result::invalid;
            if(length>std::uint64_t(library_tick_limit))return Result::unsupported;
            LibraryEntry entry;entry.part=part;entry.name=r.text(63);std::vector<Route>routes;auto nr=r.number(2);if(nr>max_routes)return Result::limit;
            for(unsigned j=0;j<nr;++j){Route route;route.program_key=r.text(127);route.pad=r.number(1);route.source_note=r.number(1);route.mixer_bus=int(r.number(1))-2;routes.push_back(std::move(route));}
            /* This editor's Sound rows have one Program catalog. Preserve a
             * future multi-Program entry opaquely rather than misroute edits. */
            for(const auto&route:routes)if(route.program_key!=routes.front().program_key)return Result::unsupported;
            auto ne=r.number(4);if(ne>max_events||total+ne>max_events)return Result::limit;total+=ne;
            std::vector<PatternEvent>events;events.reserve(ne);
            for(unsigned j=0;j<ne;++j){PatternEvent e;e.id=r.number(8);e.source_snapshot=r.number(8);e.source_event=r.number(8);e.route=r.number(2);
                auto time=r.number(8);if(time>INT64_MAX)return Result::invalid;e.native.time=std::int64_t(time);e.native.type=r.number(4);
                for(unsigned k=0;k<37;++k)e.native.data[k]=r.number(1);
                std::int64_t gate;std::memcpy(&gate,e.native.data+24,sizeof gate);
                if(gate>library_tick_limit)return Result::unsupported;
                events.push_back(e);
            }
            Result result=Pattern::restore(id,parent,revision,std::int64_t(length),routes,events,get,entry.pattern);if(result!=Result::ok)return result;
            value.entries.push_back(std::move(entry));
        }
        if(r.at!=bytes.size())return Result::invalid;
        out=std::move(value);return Result::ok;
    }catch(const std::runtime_error&){return Result::invalid;}
}
}
