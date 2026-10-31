#ifndef MPC_PAD_NATIVE_BLOCK_H
#define MPC_PAD_NATIVE_BLOCK_H
#include <stdint.h>
#include <string.h>
/* ProcessingStream::read(frames), pinned1.3.0.12: frame count + PCM pointer
 * + owned completion function. Reserve the full64-byte result, not a span.
 * The opaque callable payload is managed exclusively by the native manager.
 * Copy PCM before finish; finish invokes/releases a nonempty completion once. */
struct pad_native_block {int64_t frames;const float *pcm;uintptr_t invoke,manage,payload[4];};
static inline void pad_native_block_finish(struct pad_native_block*b){
    uintptr_t manage=b->manage,invoke=b->invoke;b->manage=0;
    if(manage){
        int (*manager)(unsigned,const void*,void*)=(int(*)(unsigned,const void*,void*))manage;
        if(manager(2,0,0)&&invoke)((void(*)(void*))invoke)(b->payload);
        manager(1,b->payload,0);
    }
    memset(b,0,sizeof(*b));
}
#endif
