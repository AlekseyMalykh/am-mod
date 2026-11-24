#include "pattern_lifecycle.h"
#include "pattern_source.h"
#include <memory>
namespace {
am_pattern::SourceDomain*domain()noexcept{
    /* No production publisher exists. If metadata initialization fails, there
     * cannot be any admitted readers; leave native workflows operational. */
    static std::unique_ptr<am_pattern::SourceDomain>value([]()noexcept{
        try{return new am_pattern::SourceDomain;}catch(...){return static_cast<am_pattern::SourceDomain*>(nullptr);}
    }());
    return value.get();
}
}
extern "C" void am_pattern_lifecycle_load(void*p,void*a,void(*original)(void*,void*)){
    if(auto*d=domain())am_pattern::source_load(*d,p,a,original);else original(p,a);
}
extern "C" int am_pattern_lifecycle_read(void*p,void*j,int(*original)(void*,void*)){
    if(auto*d=domain())return am_pattern::source_read(*d,p,j,original);
    return original(p,j);
}
extern "C" void am_pattern_lifecycle_new(void*p,int*choice,void(*original)(void*,int*)){
    if(auto*d=domain())am_pattern::source_new(*d,p,choice,original);else original(p,choice);
}
#ifdef AM_PATTERN_LIFECYCLE_TEST_ACCESS
namespace am_pattern {
SourceDomain&lifecycle_test_domain(){auto*p=domain();if(!p)throw std::bad_alloc();return *p;}
}
#endif
