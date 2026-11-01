#ifndef MPC_MOD_COMPRESSOR_H
#define MPC_MOD_COMPRESSOR_H
#include <stddef.h>
#include <stdint.h>
enum { COMP_DUCK, COMP_OPTO, COMP_TYPES };
struct comp_config { float threshold,ratio,gain; unsigned bypass; };
struct comp_opto { double rate; float rms,fast,slow,linear,gr,target; unsigned phase; };
#ifdef __cplusplus
extern "C" {
#endif
void comp_opto_init(struct comp_opto*,double);
int comp_opto_process(struct comp_opto*,const struct comp_config*,float *const*,float *const*,unsigned,size_t);
/* One UI settings owner; one native audio callback. */
int compressor_native_bind(uintptr_t,void*);
int compressor_ready(void);
unsigned compressor_type(void);
int compressor_select(unsigned);
void compressor_config(struct comp_config*);
void compressor_knob(unsigned,unsigned);
void compressor_bypass(void);
float compressor_reduction(void);
void compressor_ui_init(uintptr_t);
void compressor_phone_wake(void);
int compressor_ui_install(void);
int compressor_ui_tick(void);
int compressor_ui_owns(unsigned,unsigned);
uint64_t compressor_ui_ticket(void);
int compressor_ui_button(void*,uint64_t);
int compressor_ui_control(void*,uint64_t);
#ifdef __cplusplus
}
#endif
#endif
