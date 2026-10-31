#ifndef MPC_POWER_CONTROL_H
#define MPC_POWER_CONTROL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int power_control_install(uintptr_t base);
int power_control_busy(void);
int power_control_take_stop(void);
void power_control_tick(void);
int power_control_button(unsigned code,int down);
void power_control_paint(void *screen,void *graphics);
#ifdef __cplusplus
}
#endif
#endif
