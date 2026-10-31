#ifndef MPC_MOD_SPLASH_H
#define MPC_MOD_SPLASH_H
#include <stdint.h>
struct mod_splash_painter {
    void *context;
    void (*rectangle)(void*,int,int,int,int,uint32_t);
};
void mod_splash_frame(uint64_t milliseconds, int width, int height,
                      const struct mod_splash_painter *painter);
#endif
