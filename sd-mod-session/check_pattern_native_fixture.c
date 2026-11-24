/* Pinned native ELF services with explicit Project/Sequence/Clip lifetimes.
 * No app initialization, XPJ, GUI or production executor is involved. */
#define main legacy_fx_main
#include "check_fx_native.c"
#undef main
#include "src/step_native.h"
#define F(p,o,T) (*(T*)((char*)(p)+(o)))
static _Alignas(8) unsigned char project[0x1300],things[0x400],program[0x2900],track[0xc00];
static _Alignas(8) unsigned char sequences[3][0x800],clips[2][0x900],maps[2][0x40];
static void*sequence_array[3];
static void quiet_notification(void*p){(void)p;}
static void redirect(uintptr_t offset,void*function){
    uint32_t code[]={0x58000050,0xd61f0200};memcpy((void*)(base+offset),code,8);
    memcpy((void*)(base+offset+8),&function,8);
    __builtin___clear_cache((char*)base+offset,(char*)base+offset+16);
}
void*pattern_fixture_project(void){return project;}
void*pattern_fixture_things(void){return things;}
void*pattern_fixture_program(void){return program;}
void*pattern_fixture_sequence(unsigned i){assert(i<3);return sequences[i];}
void*pattern_fixture_list(unsigned i){assert(i<2);return clips[i]+0x688;}
void*pattern_fixture_node(unsigned sequence,unsigned ordinal){
    void*list=pattern_fixture_list(sequence),*end=(char*)list+0x68,*node=F(list,0x78,void*);
    while(ordinal--){assert(node!=end);node=FN(0xb51738,void*(*)(void*))(node);}
    assert(node!=end);return node;
}
extern int pattern_native_proof(uintptr_t);
int main(int argc,char**argv){
    assert(argc==2);load(argv[1]);
    struct sigaction sa={.sa_sigaction=fault,.sa_flags=SA_SIGINFO};
    sigaction(SIGSEGV,&sa,NULL);sigaction(SIGABRT,&sa,NULL);sigaction(SIGILL,&sa,NULL);
    phase="native pattern snapshot fixture";
    redirect(0x27ee100,quiet_notification);redirect(0x14c43c0,quiet_notification);
    F(project,0,uintptr_t)=base+0x6ecc648;F(project,0x1250,void*)=sequence_array;F(project,0x125c,int)=3;
    F(things,0x368,void*)=track;F(things,0xb8,void*)=program;F(track,0xbb8,void*)=program;
    FN(0xf535c0,void(*)(void*,const char*))(program+0xdc8,"native-pattern-program");
    for(unsigned i=0;i<128;++i){F(program,0x12a8+i*4,int)=(int)i;F(program,0x14a8+i*4,int)=(int)i;}
    const unsigned swapped[][2]={{0,36},{16,52}};
    for(unsigned i=0;i<2;++i){unsigned a=swapped[i][0],b=swapped[i][1];
        F(program,0x12a8+a*4,int)=(int)b;F(program,0x12a8+b*4,int)=(int)a;
        F(program,0x14a8+a*4,int)=(int)b;F(program,0x14a8+b*4,int)=(int)a;
    }
    for(unsigned i=0;i<3;++i){
        sequence_array[i]=sequences[i];F(sequences[i],0,uintptr_t)=base+0x6ecdff0;
        F(sequences[i],0x348,int64_t)=i?5760:7680;
        if(i<2){F(sequences[i],0x638,void*)=maps[i];F(maps[i],0x20,void*)=track;F(maps[i],0x28,void*)=clips[i];
            FN(0x27eb120,void(*)(void*))(clips[i]+0x688);}
    }
    struct step_event a=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))(36,3840,240,73,6,.8f);
    FN(0x27e4400,void(*)(void*,int,int,int))((char*)&a+0x14,0,0,7);
    FN(0x27e4400,void(*)(void*,int,int,int))((char*)&a+0x14,15,0,3);
    a.type|=0x100u|0x200u;a.data[36]=(a.data[36]&~7u)|2;
    struct step_event b=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))(52,100,10000,100,0,.5f);
    FN(0x27ee5d0,void*(*)(void*,const void*))(clips[0]+0x688,&a);
    FN(0x27ee5d0,void*(*)(void*,const void*))(clips[0]+0x688,&a);
    FN(0x27ee5d0,void*(*)(void*,const void*))(clips[0]+0x688,&b);
    return pattern_native_proof(base);
}
