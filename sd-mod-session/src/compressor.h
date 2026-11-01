#ifndef MPC_MOD_COMPRESSOR_H
#define MPC_MOD_COMPRESSOR_H
#include <stddef.h>
#include <stdint.h>
enum { COMP_DUCK, COMP_OPTO, COMP_TYPES };
/* Times are milliseconds. Release is the optical tail; the fast component
 * is Release/20, preserving the original 60ms/1200ms default response. */
struct comp_config { float threshold,ratio,gain; unsigned bypass; float attack,release,knee; };
#define COMP_CONFIG_DEFAULT {-18,3,0,0,10,1200,6}
struct comp_opto { double rate; float rms,fast,slow,linear,gr,target,desired; unsigned phase; };
struct comp_animation { double time; float level,pose; unsigned last_hit,sleeping,striking; };
struct comp_meter { float input[2],output[2]; };
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
void compressor_shift_knob(unsigned,unsigned);
void compressor_bypass(void);
float compressor_reduction(void);
float compressor_input_level(void);
void compressor_meter_levels(struct comp_meter*);
unsigned compressor_hits(void);
unsigned compressor_audio_blocks(void);
void compressor_animation_init(struct comp_animation*);
void compressor_animation_frame(struct comp_animation*,uint32_t*,double,float,const struct comp_config*,unsigned);
void compressor_meter_frame(uint32_t*,const struct comp_meter*,float);
uint32_t*compressor_image_pixels(uintptr_t);
int compressor_image_draw(void*);
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
