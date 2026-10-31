#include "src/resample_file_probe.h"
#include <cassert>
#include <initializer_list>
#include <unistd.h>
static void put(FILE*f,unsigned value,unsigned width){for(unsigned i=0;i<width;++i)std::fputc((value>>(8*i))&255,f);}
static void wave(const char*path,unsigned format,unsigned bits,bool sound){
 FILE*f=std::fopen(path,"wb");assert(f);unsigned bytes=16*bits/8;
 std::fwrite("RIFF",1,4,f);put(f,36+bytes,4);std::fwrite("WAVEfmt ",1,8,f);put(f,16,4);put(f,format,2);put(f,1,2);put(f,44100,4);put(f,44100*bits/8,4);put(f,bits/8,2);put(f,bits,2);std::fwrite("data",1,4,f);put(f,bytes,4);
 for(unsigned i=0;i<16;++i){unsigned value=bits==8?128:0;if(sound&&i==7){if(format==3){float sample=-.25f;std::memcpy(&value,&sample,4);}else value=bits==8?96:3u<<(bits-4);}put(f,value,bits/8);}std::fclose(f);
}
int main(){char path[]="/tmp/mpc-resample-wave-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);
 for(unsigned bits:{8u,16u,24u,32u})for(unsigned sound=0;sound<2;++sound){wave(path,1,bits,sound);auto p=resample_file_probe(path);assert(p.valid&&p.scanned==p.bytes&&p.rate==44100&&p.channels==1&&(sound?p.peak>0:p.peak==0));}
 wave(path,3,32,true);assert(resample_file_probe(path).peak==.25);assert(!truncate(path,20));assert(!resample_file_probe(path).valid);unlink(path);assert(!resample_file_probe(path).valid);
 puts("PASS read-only render WAV inspection distinguishes PCM8/16/24/32 and float32 signal/silence; missing/truncated file never claimed valid");
}
