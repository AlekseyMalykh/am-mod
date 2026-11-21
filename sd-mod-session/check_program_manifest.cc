#include "src/program_manifest.h"
#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <cstdio>
static std::string xpm(const std::string&layers,const std::string&type="Drum"){
 return "<?xml version='1.0'?><MPCVObject><Program type='"+type+"'><Instruments><Instrument number='1'><Layers>"+layers+"</Layers></Instrument></Instruments></Program></MPCVObject>";
}
static std::string layer(const std::string&name="Kick &amp; Snare",const std::string&number="1"){
 return "<Layer number='"+number+"'><SampleName>"+name+"</SampleName><SampleFile>Kit/Kick.wav</SampleFile></Layer>";
}
int main(){
 auto output=std::unique_ptr<program_manifest>(new program_manifest{});char error[96];
 auto read=[&](const std::string&s){return program_xpm_read(s.data(),s.size(),output.get(),error);};
 assert(read(xpm(layer())));assert(output->count==1&&output->samples[0].pad==0&&output->samples[0].layer==0);
 assert(!std::strcmp(output->samples[0].name,"Kick & Snare")&&!std::strcmp(output->samples[0].file,"Kit/Kick.wav"));
 auto previous=std::unique_ptr<program_manifest>(new program_manifest(*output));
 auto reject=[&](const std::string&s){assert(!read(s));assert(!std::memcmp(previous.get(),output.get(),sizeof(*output)));};
 reject(xpm("<AudioRoute/>"+layer()));
 reject(xpm("<AudioRoute><AudioRoute>0</AudioRoute></AudioRoute>"+layer()));
 reject(xpm(layer(),"Plugin"));reject(xpm(layer(),"Keygroup"));reject(xpm(layer("Kick","5")));
 reject(xpm(layer()+layer()));reject(xpm(""));reject(xpm(layer()).substr(0,80));
 reject("<!DOCTYPE MPCVObject [<!ENTITY a SYSTEM 'file:///etc/passwd'>]>"+xpm(layer("&a;")));
 reject("<!DOCTYPE MPCVObject [<!ENTITY a 'test'>]>"+xpm(layer("&a;")));
 reject(xpm(layer("&bad;")));reject(xpm(layer(std::string(256,'x'))));
 std::string deep;for(int i=0;i<70;++i)deep+="<a>";for(int i=0;i<70;++i)deep+="</a>";reject(xpm(deep));
 assert(read(xpm(layer("Kick","1")+layer("Snare","4"))));assert(output->count==2&&output->samples[1].layer==3);
 std::string valid=xpm(layer());
 for(size_t i=0;i<valid.size();++i){std::string bad=valid;bad[i]='\0';*previous=*output;if(!read(bad))assert(!std::memcmp(previous.get(),output.get(),sizeof(*output)));}
 puts("PASS Drum XPM metadata, entities, layers, type/bounds/duplicates, malformed XML, DTD rejection and transactional mutations");
}
