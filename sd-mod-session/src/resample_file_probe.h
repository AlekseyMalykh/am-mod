#ifndef MPC_RESAMPLE_FILE_PROBE_H
#define MPC_RESAMPLE_FILE_PROBE_H
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <sys/stat.h>
/* Read only after offline export, before native import; never on audio callback.
 * Bounded work and memory. A partial scan cannot establish complete silence. */
struct ResampleFileProbe {unsigned valid=0,format=0,channels=0,rate=0,bits=0,bytes=0,scanned=0;double peak=0;};
static unsigned resample_le(const unsigned char*p,unsigned n){unsigned v=0;for(unsigned i=0;i<n;++i)v|=(unsigned)p[i]<<(i*8);return v;}
static ResampleFileProbe resample_file_probe_stream(FILE*f){
 ResampleFileProbe r;if(!f)return r;struct stat st{};
 unsigned char header[40];if(fstat(fileno(f),&st)||!S_ISREG(st.st_mode)||st.st_size<12||std::fread(header,1,12,f)!=12||std::memcmp(header,"RIFF",4)||std::memcmp(header+8,"WAVE",4))return r;
 for(unsigned chunk=0;chunk<64;++chunk){
  unsigned char name[8];if(std::fread(name,1,8,f)!=8)return r;
  unsigned size=resample_le(name+4,4);long start=std::ftell(f);if(start<0||(uint64_t)start+size>(uint64_t)st.st_size)return r;
  if(!std::memcmp(name,"fmt ",4)){
   unsigned n=size<40?size:40;if(n<16||std::fread(header,1,n,f)!=n)return r;
   r.format=resample_le(header,2);r.channels=resample_le(header+2,2);r.rate=resample_le(header+4,4);r.bits=resample_le(header+14,2);
   if(r.format==65534&&n>=40)r.format=resample_le(header+24,2);
  }else if(!std::memcmp(name,"data",4)){
   unsigned width=r.bits/8;if(!r.channels||r.channels>2||!r.rate||!size||!width||size%(width*r.channels)||!((r.format==1&&(r.bits==8||r.bits==16||r.bits==24||r.bits==32))||(r.format==3&&r.bits==32)))return r;
   r.bytes=size;unsigned remaining=size<64u*1024*1024?size:64u*1024*1024;remaining-=remaining%width;unsigned char block[4096];
   while(remaining){unsigned n=remaining<sizeof(block)?remaining:sizeof(block);n-=n%width;if(!n||std::fread(block,1,n,f)!=n)return r;
    for(unsigned i=0;i<n;i+=width){double sample;unsigned u=resample_le(block+i,width);
     if(r.format==3){float value;std::memcpy(&value,&u,4);if(!std::isfinite(value))return r;sample=value;}
     else if(r.bits==8)sample=((int)u-128)/128.;
     else{int64_t value=u;if(u&(1u<<(r.bits-1)))value-=INT64_C(1)<<r.bits;sample=value/(double)(INT64_C(1)<<(r.bits-1));}
     if(std::fabs(sample)>r.peak)r.peak=std::fabs(sample);
    }r.scanned+=n;remaining-=n;
   }r.valid=1;return r;
  }
  if(std::fseek(f,start+(long)size+(size&1u),SEEK_SET))return r;
 }return r;
}
static ResampleFileProbe resample_file_probe(const char*path){
 FILE*f=path?std::fopen(path,"rb"):nullptr;if(!f)return ResampleFileProbe{};
 struct Close{FILE*f;~Close(){std::fclose(f);}}close{f};return resample_file_probe_stream(f);
}
#endif
