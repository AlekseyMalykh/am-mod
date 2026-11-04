/* A native allocation failure must return to the C caller's cleanup path.
 * There is no global event-factory override or alternate command executor. */
#include <cstdint>
#include <memory>
/* Native SetSequenceLengthCommand (19a6240), through the same Undo executor
 * as Sequence's Bars control. The command retains its native virtual
 * destructor; shared_ptr uses the firmware's libstdc++ ABI. */
extern "C" int step_set_bars(uintptr_t base,void *executor,void *sequence,int bars) noexcept
{
    if(!executor||!sequence||bars<1||bars>128)return 0;
    try {
        void *storage=((void*(*)(unsigned long))(base+0xb51418))(0x80);
        try {((void(*)(void*,void*,int))(base+0x19a6240))(storage,sequence,bars);}
        catch (...) {((void(*)(void*,unsigned long))(base+0xb51408))(storage,0x80);throw;}
        std::shared_ptr<void> command(storage,[](void *p){
            auto table=*static_cast<uintptr_t**>(p);
            ((void(*)(void*))table[1])(p);
        });
        return ((int(*)(void*,const std::shared_ptr<void>*))(base+0x19d0a00))(executor,&command)==0;
    } catch (...) {return 0;}
}
extern "C" int step_try_call(void (*function)(void *),void *context) noexcept
{
    try {
        function(context);
        return 1;
    } catch (...) { return 0; }
}
