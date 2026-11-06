#include "mixer_fx.h"
#include "fx_native.h"
#include <atomic>
#include <cstring>
#include <new>
struct mixer_fx {
    uintptr_t image;
    uint64_t epoch;
    void *engine,*program;
    fx_native_context native{};
    fx_backend backend{},delegate{};
    bool ready=false,captured=false;
    std::atomic<bool> fault{false};
};
static int position(void *user,void *out,uint64_t *block)
{
    auto *context=static_cast<mixer_fx*>(user);
    if(mixer_live_native_position(context->epoch,out,block))return 1;
    context->fault.store(true,std::memory_order_release);return 0;
}
static void*create(void *user,uint32_t id,double rate,const float *parameters)
{
    auto *context=static_cast<mixer_fx*>(user);
    return context->ready?context->delegate.create(context->delegate.user,id,rate,parameters):nullptr;
}
static void destroy(void *user,void *processor)
{
    auto *context=static_cast<mixer_fx*>(user);
    context->delegate.destroy(context->delegate.user,processor);
}
static void process(void *user,void *processor,float *data,size_t frames)
{
    auto *context=static_cast<mixer_fx*>(user);
    context->delegate.process(context->delegate.user,processor,data,frames);
}
extern "C" void mixer_fx_refresh(mixer_fx *context,const mixer_native_binding *source)
{
    if(!context||!source||source->epoch!=context->epoch||
       source->engine!=context->engine||source->program!=context->program)return;
    context->ready=false;if(!source->clock_valid)return;
    if(!context->captured){
        if(!fx_native_application_formats(context->image,&context->native))return;
        context->native.position_user=context;context->native.position=position;
        fx_native_backend(&context->native,&context->delegate);context->captured=true;
    }
    /* Processor creation reads these only on this same owner. Already prepared
     * processors own their initial copy and use the audio clock callback. */
    std::memcpy(&context->native.bpm,source->position,8);
    std::memcpy(&context->native.numerator,source->position+8,4);
    std::memcpy(&context->native.denominator,source->position+12,4);
    std::memcpy(context->native.initial_position,source->position,88);context->ready=true;
}
extern "C" mixer_fx*mixer_fx_create(uintptr_t image,const mixer_native_binding *source)
{
    if(!image||!source)return nullptr;
    auto *context=new(std::nothrow)mixer_fx;if(!context)return nullptr;
    context->image=image;context->epoch=source->epoch;context->engine=source->engine;context->program=source->program;
    context->backend={context,create,destroy,process};mixer_fx_refresh(context,source);return context;
}
extern "C" const fx_backend*mixer_fx_backend(mixer_fx *context){return context?&context->backend:nullptr;}
extern "C" int mixer_fx_ready(const mixer_fx *context){return context&&context->ready;}
extern "C" int mixer_fx_fault(const mixer_fx *context){return context&&context->fault.load(std::memory_order_acquire);}
extern "C" void mixer_fx_destroy(mixer_fx *context){delete context;}
