#include "program_manifest.h"
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <memory>
#include <limits>
namespace {
using Parser=void*;
struct API {
 void*lib;Parser(*create)(const char*);void(*free)(Parser);void(*user)(Parser,void*);
 void(*elements)(Parser,void(*)(void*,const char*,const char**),void(*)(void*,const char*));
 void(*characters)(Parser,void(*)(void*,const char*,int));
 void(*doctype)(Parser,void(*)(void*,const char*,const char*,const char*,int));
 int(*parse)(Parser,const char*,int,int);int(*stop)(Parser,int);
 API():lib(dlopen("libexpat.so.1",RTLD_NOW|RTLD_LOCAL)) {
#define LOAD(member,name) member=reinterpret_cast<decltype(member)>(lib?dlsym(lib,name):nullptr)
  LOAD(create,"XML_ParserCreate");LOAD(free,"XML_ParserFree");LOAD(user,"XML_SetUserData");LOAD(elements,"XML_SetElementHandler");LOAD(characters,"XML_SetCharacterDataHandler");LOAD(doctype,"XML_SetStartDoctypeDeclHandler");LOAD(parse,"XML_Parse");LOAD(stop,"XML_StopParser");
#undef LOAD
 }
 bool ready()const{return create&&free&&user&&elements&&characters&&doctype&&parse&&stop;}
};
struct State {
 API&api;Parser parser;program_manifest manifest{};std::vector<std::string> stack;
 bool pads[128]{},layers[128][4]{},failed=false;unsigned programs=0;
 unsigned route_fields[64]{};
 int pad=-1,layer=-1;std::string name,file,field;const char*error="Invalid XPM XML";
 explicit State(API&a):api(a),parser(a.create(nullptr)){}
 ~State(){if(parser)api.free(parser);}
 void fail(const char*s){failed=true;error=s;api.stop(parser,0);}
 bool path(const char*a,const char*b,const char*c=nullptr,const char*d=nullptr)const{
  return stack.size()==(d?4u:c?3u:2u)&&stack[0]==a&&stack[1]==b&&(!c||stack[2]==c)&&(!d||stack[3]==d);
 }
};
static const char*attr(const char**a,const char*n){for(;a&&*a;a+=2)if(!std::strcmp(*a,n))return a[1];return nullptr;}
static int number(const char*s,int max){if(!s||!*s)return -1;unsigned n=0;for(;*s;++s){if(*s<'0'||*s>'9'||n>(unsigned)max)return -1;n=n*10+(unsigned)(*s-'0');}return n>0&&n<=(unsigned)max?(int)n-1:-1;}
static void start(void*u,const char*n,const char**a){
 auto&s=*static_cast<State*>(u);if(s.failed)return;
 try{
  if(s.stack.size()>=64||std::strlen(n)>128){s.fail("XPM nesting or tag exceeds limit");return;}
  if(s.stack.empty()&&std::strcmp(n,"MPCVObject")){s.fail("Expected MPCVObject XPM");return;}
  if(!s.stack.empty()&&s.stack.back()=="AudioRoute"){
   unsigned bit=!std::strcmp(n,"AudioRoute")?1u:!std::strcmp(n,"AudioRouteSubIndex")?2u:!std::strcmp(n,"AudioRouteChannelBitmap")?4u:!std::strcmp(n,"InsertsEnabled")?8u:0u;
   s.route_fields[s.stack.size()-1]|=bit;
  }
  s.route_fields[s.stack.size()]=0;s.stack.emplace_back(n);
  if(s.path("MPCVObject","Program")){
   const char*t=attr(a,"type");if(++s.programs!=1||!t||std::strcmp(t,"Drum"))s.fail("Only Drum XPM programs are supported");
  }else if(s.path("MPCVObject","Program","Instruments","Instrument")){
   s.pad=number(attr(a,"number"),128);if(s.pad<0||s.pads[s.pad])s.fail("Invalid or duplicate XPM pad");else s.pads[s.pad]=true;
  }else if(s.stack.size()==6&&s.stack[3]=="Instrument"&&s.stack[4]=="Layers"&&s.stack[5]=="Layer"&&s.pad>=0){
   s.layer=number(attr(a,"number"),4);if(s.layer<0||s.layers[s.pad][s.layer])s.fail("XPM supports four unique layers per pad");
   else{s.layers[s.pad][s.layer]=true;s.name.clear();s.file.clear();}
  }else if(s.stack.size()==7&&s.stack[5]=="Layer"&&(s.stack[6]=="SampleName"||s.stack[6]=="SampleFile"))s.field=n;
 }catch(...){s.fail("XPM metadata allocation failed");}
}
static void characters(void*u,const char*p,int n){
 auto&s=*static_cast<State*>(u);if(s.failed||s.field.empty())return;
 try{std::string&v=s.field=="SampleName"?s.name:s.file;if(v.size()+(unsigned)n>1023){s.fail("XPM sample path exceeds limit");return;}v.append(p,(size_t)n);}catch(...){s.fail("XPM metadata allocation failed");}
}
static void end(void*u,const char*n){
 auto&s=*static_cast<State*>(u);if(s.failed)return;
 if(!std::strcmp(n,"AudioRoute")&&s.stack.size()>1&&s.stack[s.stack.size()-2]!="AudioRoute"&&s.route_fields[s.stack.size()-1]!=15){
  s.fail("Incomplete XPM audio routing fields");return;
 }
 if(s.stack.size()==7)s.field.clear();
 if(s.stack.size()==6&&s.stack[5]=="Layer"&&s.pad>=0&&s.layer>=0){
  if(!s.name.empty()||!s.file.empty()){
   if(s.name.empty()||s.name.size()>255||s.manifest.count>=512){s.fail("Invalid XPM sample name");return;}
   auto&r=s.manifest.samples[s.manifest.count++];r.pad=(unsigned)s.pad;r.layer=(unsigned)s.layer;
   std::memcpy(r.name,s.name.c_str(),s.name.size()+1);std::memcpy(r.file,s.file.c_str(),s.file.size()+1);
  }s.layer=-1;
 }
 if(s.stack.size()==4&&!std::strcmp(n,"Instrument"))s.pad=-1;
 if(!s.stack.empty())s.stack.pop_back();
}
static void doctype(void*u,const char*,const char*,const char*,int){static_cast<State*>(u)->fail("XPM DTD and external entities are unsupported");}
}
extern "C" int program_xpm_read(const void*bytes,size_t size,program_manifest*out,char error[96]){
 if(!bytes||!out||!error||size<16||size>16*1024*1024)return 0;
 static API api;if(!api.ready()){std::snprintf(error,96,"Firmware XML parser unavailable");return 0;}
 try{
  auto s=std::unique_ptr<State>(new State(api));if(!s->parser){std::snprintf(error,96,"XML allocation failed");return 0;}
  api.user(s->parser,s.get());api.elements(s->parser,start,end);api.characters(s->parser,characters);api.doctype(s->parser,doctype);
  int ok=api.parse(s->parser,static_cast<const char*>(bytes),(int)size,1);
  if(!ok||s->failed||s->programs!=1||!s->manifest.count){std::snprintf(error,96,"%s",s->failed?s->error:"Invalid or empty Drum XPM");return 0;}
  *out=s->manifest;error[0]=0;return 1;
 }catch(...){std::snprintf(error,96,"XML metadata allocation failed");return 0;}
}
