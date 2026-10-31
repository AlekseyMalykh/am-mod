#include "src/resample_output_file.h"
#include <cassert>
#include <fstream>
#include <iterator>
static void wav(const std::string&file,bool audible){
 unsigned char b[48]={};memcpy(b,"RIFF",4);b[4]=40;memcpy(b+8,"WAVEfmt ",8);b[16]=16;b[20]=1;b[22]=1;b[24]=0x44;b[25]=0xac;b[28]=0x88;b[29]=0x58;b[30]=1;b[32]=2;b[34]=16;memcpy(b+36,"data",4);b[40]=4;b[45]=audible?32:0;
 FILE*f=fopen(file.c_str(),"wb");assert(f&&fwrite(b,1,sizeof(b),f)==sizeof(b)&&!fclose(f));
}
static std::string bytes(const std::string&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
 char dir[]="/tmp/mpc-output-proof-XXXXXX";assert(mkdtemp(dir));std::string root=dir,want=root+"/Hat Resample 1.wav",stem=root+"/Hat Resample 1-Track-001-Drum 001.wav",other=root+"/Other.wav";
 ResampleOutputFile out;wav(want,false);wav(stem,true);std::string signal=bytes(stem),silent=bytes(want);
 assert(out.capture(want,stem,44100,2));unlink(stem.c_str());assert(out.commit(want)&&!out.pending()&&bytes(want)==signal); // native cleanup may unlink the stem before import
 wav(want,false);wav(stem,true);assert(out.capture(want,stem,44100,2));assert(!out.commit(other)&&out.pending());assert(bytes(want)==silent);out.reset();
 wav(stem,false);assert(!out.capture(want,stem,44100,2)&&bytes(want)==silent);wav(stem,true);
 assert(!out.capture(want,stem,48000,2)&&!out.capture(want,stem,44100,3));
 wav(other,true);assert(!out.capture(want,other,44100,2));unlink(other.c_str());assert(!symlink(stem.c_str(),other.c_str()));assert(!out.capture(other,other,44100,2));unlink(other.c_str());
 assert(out.capture(want,stem,44100,2));unlink(want.c_str());assert(!symlink(stem.c_str(),want.c_str()));assert(!out.commit(want)&&!out.pending()&&bytes(stem)==signal);unlink(want.c_str());
 wav(want,true);assert(out.capture(want,stem,44100,2));std::string before=bytes(want);assert(out.commit(want)&&bytes(want)==before);
 wav(want,false);assert(out.capture(want,stem,44100,2));FILE*f=fopen(stem.c_str(),"ab");assert(f);fputc(1,f);fclose(f);assert(!out.commit(want)&&bytes(want)==silent);
 unlink(stem.c_str());unlink(want.c_str());assert(!rmdir(root.c_str()));
 puts("PASS exact native stem -> requested WAV finalization, byte-identical signal, retained FD after cleanup/unlink, wrong job/path/rate/length, silent source, symlinks and changed source rejected; failed commit preserves destination; existing audible native output retained.");
}
