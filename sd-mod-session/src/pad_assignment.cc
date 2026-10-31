#include "pad_assignment.h"
#include <memory>
#include <new>
#define F(p,o,T) (*reinterpret_cast<T*>(reinterpret_cast<char*>(p)+(o)))
extern "C" void pad_assignment_pool_find(void*,const uint64_t*,uintptr_t*,uintptr_t);
extern "C" int pad_assignment_prepare(uintptr_t base,void*properties,void*program,
    void*service,void*pool,const uintptr_t sample[2],unsigned target,unsigned layer,
    uintptr_t out[2]){
    if(!out)return 0;
    out[0]=out[1]=0;
    if(!base||!properties||!program||!service||!pool||!sample||!sample[0]||!sample[1]||target>=128||layer>=8)return 0;
    void*p=reinterpret_cast<void*>(sample[0]);
    const char*name=F(p,0xa8,const char*);
    if(!name||!*name||!F(p,0x130,void*)||F(properties,0x1278,void*)!=pool)return 0;
    int frames=reinterpret_cast<int(*)(void*)>(base+0x2941f80)(p);
    int channels=reinterpret_cast<int(*)(void*)>(base+0x2941f70)(p);
    int rate=reinterpret_cast<int(*)(void*)>(base+0x2941f90)(p);
    if(frames<=0||(channels!=1&&channels!=2)||rate<8000||rate>192000)return 0;
    /* The native ctor copies only the name. An unregistered sample would
     * otherwise appear to succeed while assigning an empty layer. */
    uintptr_t found[2]={};
    try{pad_assignment_pool_find(pool,reinterpret_cast<const uint64_t*>(reinterpret_cast<char*>(p)+0xa8),found,base+0x292ed50);}
    catch(...){if(found[1])reinterpret_cast<void(*)(void*)>(base+0x123bca0)(reinterpret_cast<void*>(found[1]));return 0;}
    bool registered=found[0]==sample[0]&&found[1]!=0;
    if(found[1])reinterpret_cast<void(*)(void*)>(base+0x123bca0)(reinterpret_cast<void*>(found[1]));
    if(!registered)return 0;
    void*instrument=reinterpret_cast<void*(*)(void*,int)>(base+0x286bbe0)(program,(int)target);
    if(!instrument||F(instrument,0x2310,void*)!=program)return 0;
    void*memory=nullptr;bool constructed=false;
    try{
        memory=::operator new(0x108);
        reinterpret_cast<void(*)(void*,void*,void*,const uintptr_t*,int,int,void*)>(base+0x1995f90)
            (memory,properties,program,sample,(int)target,(int)layer,service);
        constructed=true;
        std::shared_ptr<void> command(memory,[base](void*q){reinterpret_cast<void(*)(void*)>(base+0x199c2d0)(q);::operator delete(q);});
        /* Ownership transfer without aliasing a C pair as std::shared_ptr. */
        static_assert(sizeof(command)==2*sizeof(uintptr_t),"Pinned shared_ptr ABI");
        auto*words=reinterpret_cast<const unsigned char*>(&command);
        for(unsigned i=0;i<sizeof(command);++i)reinterpret_cast<unsigned char*>(out)[i]=words[i];
        /* Caller now owns the reference represented by these two words. */
        new(&command) std::shared_ptr<void>();
        return 1;
    }catch(...){if(memory&&!constructed)::operator delete(memory);return 0;}
}
extern "C" void pad_assignment_release(uintptr_t base,uintptr_t command[2]){
    if(!command||!base)return;
    uintptr_t reference=command[1];command[0]=command[1]=0;
    if(reference)reinterpret_cast<void(*)(void*)>(base+0x123bca0)(reinterpret_cast<void*>(reference));
}
