#include "pattern_source.h"
#include <condition_variable>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace am_pattern {
struct SourceState {
    std::mutex mutex;std::condition_variable idle;
    std::uint64_t epoch=1;unsigned readers=0,changes=0;bool ready=false,retired=false;
    SourceBinding binding;
};
namespace {
template<class T>T field(const void*p,std::size_t n){T v;std::memcpy(&v,static_cast<const char*>(p)+n,sizeof v);return v;}
/* Detect prohibited write/retire from inside a read lease on this thread. */
thread_local SourceState*reading=nullptr;
bool aligned(const void*p){return p&&!(reinterpret_cast<std::uintptr_t>(p)&7u);}
void invalidate(SourceState&s){
    s.ready=false;
    if(s.epoch==std::numeric_limits<std::uint64_t>::max())s.retired=true;
    else ++s.epoch;
}
class Currency:public ReadCurrency {
    std::shared_ptr<SourceState> state;std::uint64_t expected;
public:
    Currency(std::shared_ptr<SourceState>s,std::uint64_t e):state(std::move(s)),expected(e){}
    bool current()const noexcept override{
        std::lock_guard<std::mutex>lock(state->mutex);
        return state->ready&&!state->retired&&!state->changes&&state->epoch==expected;
    }
};
class Owner:public NativeReadOwner {
    std::shared_ptr<SourceState> state;std::uint64_t expected;
    std::shared_ptr<const ReadCurrency> stamp;
    std::mutex mutex;void*context=nullptr;std::thread::id thread;
public:
    Owner(std::shared_ptr<SourceState>s,std::uint64_t e):state(std::move(s)),expected(e),stamp(std::make_shared<Currency>(state,e)){}
    bool enter(void*c,NativeReadContext&out)override{
        std::lock_guard<std::mutex>own(mutex);
        if(context||reading||!aligned(c))return false;
        std::lock_guard<std::mutex>lock(state->mutex);
        if(!state->ready||state->retired||state->changes||state->epoch!=expected)return false;
        const auto&b=state->binding;
        /* The source gate is held before resolving/dereferencing native data. */
        if(field<void*>(c,0xc8)!=b.project||field<void*>(c,0x78)!=b.things
           ||field<void*>(b.things,0xb8)!=b.program)return false;
        out.image=b.image;out.project=b.project;out.things=b.things;out.generation=expected;
        out.routing_revision=b.routing_revision;out.outputs=b.outputs;out.read_safe=true;
        ++state->readers;context=c;thread=std::this_thread::get_id();reading=state.get();return true;
    }
    bool valid(void*c,const NativeReadContext&out)override{
        std::lock_guard<std::mutex>own(mutex);std::lock_guard<std::mutex>lock(state->mutex);
        return context==c&&thread==std::this_thread::get_id()&&reading==state.get()
            &&state->ready&&!state->retired&&!state->changes&&state->epoch==expected&&out.generation==expected;
    }
    void leave(void*c,const NativeReadContext&)noexcept override{
        std::lock_guard<std::mutex>own(mutex);std::lock_guard<std::mutex>lock(state->mutex);
        if(context!=c||thread!=std::this_thread::get_id()||reading!=state.get()||!state->readers)std::terminate();
        context=nullptr;thread={};reading=nullptr;--state->readers;state->idle.notify_all();
    }
    std::shared_ptr<const ReadCurrency>currency()const override{return stamp;}
};
}
SourceDomain::SourceDomain():state_(std::make_shared<SourceState>()){}
SourceDomain::~SourceDomain(){retire();}
SourceDomain::Change::Change(std::shared_ptr<SourceState>s):state_(std::move(s)){}
SourceDomain::Change::Change(Change&&other)noexcept:state_(std::move(other.state_)){}
SourceDomain::Change&SourceDomain::Change::operator=(Change&&other)noexcept{
    if(this!=&other){finish();state_=std::move(other.state_);}return *this;
}
void SourceDomain::Change::finish()noexcept{
    if(!state_)return;
    {std::lock_guard<std::mutex>lock(state_->mutex);--state_->changes;state_->idle.notify_all();}
    state_.reset();
}
SourceDomain::Change::~Change(){finish();}
SourceDomain::Change SourceDomain::change(){
    if(reading)throw std::logic_error("Source mutation from inside a read lease");
    std::unique_lock<std::mutex>lock(state_->mutex);
    if(state_->retired)throw std::logic_error("Source domain retired");
    if(state_->changes==std::numeric_limits<unsigned>::max())throw std::overflow_error("Source change limit");
    invalidate(*state_);++state_->changes;
    try{state_->idle.wait(lock,[&]{return !state_->readers;});}
    catch(...){--state_->changes;throw;}
    return Change(state_);
}
std::uint64_t SourceDomain::epoch()const noexcept{std::lock_guard<std::mutex>lock(state_->mutex);return state_->epoch;}
bool SourceDomain::publish(const SourceBinding&binding,std::uint64_t expected){
    if(!binding.image||!aligned(binding.project)||!aligned(binding.things)||!aligned(binding.program))return false;
    for(int output:binding.outputs)if(output< -1||output>8)return false;
    std::lock_guard<std::mutex>lock(state_->mutex);
    if(state_->retired||state_->ready||state_->changes||state_->readers||state_->epoch!=expected)return false;
    /* Publication must be on the lifetime owner after all writers completed. */
    if(field<std::uintptr_t>(binding.project,0)!=binding.image+0x6ecc648
       ||field<void*>(binding.things,0xb8)!=binding.program)return false;
    state_->binding=binding;state_->ready=true;return true;
}
std::shared_ptr<NativeReadOwner>SourceDomain::owner()const{
    std::lock_guard<std::mutex>lock(state_->mutex);
    if(!state_->ready||state_->retired||state_->changes)return {};
    return std::make_shared<Owner>(state_,state_->epoch);
}
void SourceDomain::retire()noexcept{
    if(reading)std::terminate();
    std::unique_lock<std::mutex>lock(state_->mutex);if(!state_->retired)invalidate(*state_);state_->retired=true;
    state_->idle.wait(lock,[&]{return !state_->readers&&!state_->changes;});state_->binding=SourceBinding{};
}
void source_load(SourceDomain&domain,void*p,void*a,void(*original)(void*,void*)){
    if(!original)throw std::invalid_argument("Missing native Load");
    auto change=domain.change();original(p,a);
}
int source_read(SourceDomain&domain,void*p,void*j,int(*original)(void*,void*)){
    if(!original)throw std::invalid_argument("Missing native Read");
    auto change=domain.change();return original(p,j);
}
void source_new(SourceDomain&domain,void*p,int*choice,void(*original)(void*,int*)){
    if(!original)throw std::invalid_argument("Missing native New");
    if(choice&&*choice==1){auto change=domain.change();original(p,choice);}
    else original(p,choice);
}
void source_program(SourceDomain&domain,void*track,void*program,int flags,void(*original)(void*,void*,int)){
    if(!original)throw std::invalid_argument("Missing native Program change");
    auto change=domain.change();original(track,program,flags);
}
}
