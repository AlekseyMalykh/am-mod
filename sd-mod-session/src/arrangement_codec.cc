#include "arrangement.h"
#include <stdexcept>
namespace am_pattern {
namespace {
std::uint32_t sum(const std::string&s){std::uint32_t h=2166136261u;for(unsigned char c:s)h=(h^c)*16777619u;return h;}
void number(std::string&s,std::uint64_t value,unsigned count){for(unsigned i=0;i<count;++i){s+=char(value&255);value>>=8;}}
struct Reader {
    const std::string&data;std::size_t at=0;explicit Reader(const std::string&s):data(s){}
    std::uint64_t number(unsigned n){if(n>data.size()-at)throw std::runtime_error("Truncated arrangement");std::uint64_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint64_t((unsigned char)data[at++])<<(8*i);return v;}
};
}
std::string arrangement_encode(const Arrangement&song,const Library&library){
    if(song.validate(library)!=Result::ok)throw std::runtime_error("Invalid arrangement references");
    std::string data;number(data,song.next,8);number(data,song.revision,8);number(data,song.length,8);number(data,song.tracks.size(),1);number(data,song.clips.size(),2);
    for(const auto&t:song.tracks){number(data,t.id,8);number(data,t.muted,1);number(data,t.solo,1);number(data,t.name.size(),1);data+=t.name;}
    for(const auto&c:song.clips){number(data,c.id,8);number(data,c.track,8);number(data,c.pattern,8);number(data,c.start,8);number(data,c.length,8);number(data,c.offset,8);number(data,c.muted,1);}
    number(data,sum(data),4);const char hex[]="0123456789abcdef";std::string out="AAR1 ";out.reserve(data.size()*2+6);
    for(unsigned char c:data){out+=hex[c>>4];out+=hex[c&15];}out+='\n';return out;
}
Result arrangement_decode(const std::string&text,const Library&library,Arrangement&out){
    if(text.compare(0,5,"AAR1 "))return Result::unsupported;
    if(text.size()<6||text.size()>65536||(text.size()-6)%2||text.back()!='\n')return Result::invalid;
    auto hex=[](unsigned char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;};
    try{
        std::string data;data.reserve((text.size()-6)/2);
        for(std::size_t i=5;i+1<text.size()-1;i+=2){int a=hex(text[i]),b=hex(text[i+1]);if(a<0||b<0)return Result::invalid;data+=char(a*16+b);}
        if(data.size()<4)return Result::invalid;
        Reader tail(data);tail.at=data.size()-4;auto checksum=tail.number(4);data.resize(data.size()-4);if(sum(data)!=checksum)return Result::invalid;
        Reader r(data);Arrangement song;song.next=r.number(8);song.revision=r.number(8);auto length=r.number(8),tracks=r.number(1),clips=r.number(2);
        if(length>INT64_MAX||tracks>arrangement_track_limit||clips>arrangement_clip_limit)return Result::invalid;
        song.length=std::int64_t(length);
        for(unsigned i=0;i<tracks;++i){ArrangementTrack t;t.id=r.number(8);auto muted=r.number(1),solo=r.number(1),n=r.number(1);if(muted>1||solo>1||n>31)return Result::invalid;t.muted=muted;t.solo=solo;for(unsigned j=0;j<n;++j)t.name+=char(r.number(1));song.tracks.push_back(t);}
        for(unsigned i=0;i<clips;++i){ArrangementClip c;c.id=r.number(8);c.track=r.number(8);c.pattern=r.number(8);auto start=r.number(8),n=r.number(8),offset=r.number(8),muted=r.number(1);
            if(start>INT64_MAX||n>INT64_MAX||offset>INT64_MAX||muted>1)return Result::invalid;
            c.start=std::int64_t(start);c.length=std::int64_t(n);c.offset=std::int64_t(offset);c.muted=muted;song.clips.push_back(c);}
        if(r.at!=data.size())return Result::invalid;
        Result result=song.validate(library);if(result!=Result::ok)return result;out=std::move(song);return Result::ok;
    }catch(const std::runtime_error&){return Result::invalid;}
}
}
