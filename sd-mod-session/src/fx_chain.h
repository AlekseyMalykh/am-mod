#ifndef MPC_MOD_FX_CHAIN_H
#define MPC_MOD_FX_CHAIN_H
#include <stddef.h>
#include <stdint.h>
#include "fx_eq.h"
#ifdef __cplusplus
extern "C" {
#endif
#define FX_CHAIN_MAX 5
/* Actual AIR Reverb exposes eighteen parameters, beyond the six controls
 * shown by the stock AC50 page. Preserve the complete processor state. */
#define FX_CHAIN_PARAMETERS 32
enum fx_stage_kind { FX_STAGE_EQ=1,FX_STAGE_NATIVE=2,FX_STAGE_SIDECHAIN=3 };
enum fx_result { FX_OK,FX_INVALID,FX_FULL,FX_RESOURCE,FX_BUSY };
struct fx_stage { unsigned kind,bypass; uint32_t native_id; float parameter[FX_CHAIN_PARAMETERS]; struct fx_eq_config eq; };
struct fx_chain_config { unsigned count; struct fx_stage stage[FX_CHAIN_MAX]; };
/* Backend create/delete and program preparation run on the control owner.
 * process runs only on the audio owner and must not allocate or lock. */
struct fx_backend {
    void *user;
    void *(*create)(void*,uint32_t,double,const float[FX_CHAIN_PARAMETERS]);
    void (*destroy)(void*,void*);
    void (*process)(void*,void*,float*,size_t);
};
#define FX_SIDECHAIN_ID 0x5343484eu
#define FX_SIDECHAIN_KEYS 8u
float fx_sidechain_value(unsigned parameter,float normalized);
struct fx_program;
struct fx_exchange;
int fx_chain_valid(const struct fx_chain_config *,double rate);
enum fx_result fx_chain_add(struct fx_chain_config *,const struct fx_stage *,double rate);
enum fx_result fx_chain_replace(struct fx_chain_config *,unsigned,const struct fx_stage *,double rate);
enum fx_result fx_chain_remove(struct fx_chain_config *,unsigned);
enum fx_result fx_chain_move(struct fx_chain_config *,unsigned,unsigned);
struct fx_program *fx_program_prepare(const struct fx_chain_config *,double,const struct fx_backend *);
void fx_program_destroy(struct fx_program *);
void fx_program_process(struct fx_program *,float*,size_t);
/* One control owner, one audio owner. Only complete prepared programs cross
 * this boundary; previous programs are reclaimed off the audio thread. */
struct fx_exchange *fx_exchange_create(void);
/* Off RT, before first process/publication only. Takes ownership, including
 * NULL for an initially empty chain, without an initial fade from dry. */
int fx_exchange_seed(struct fx_exchange *,struct fx_program *);
/* Audio owner only: true when a complete next swap can start. Collection of
 * retired instances still belongs to the control owner. */
int fx_exchange_ready_audio(const struct fx_exchange *);
int fx_exchange_transitioning_audio(const struct fx_exchange *);
enum fx_result fx_exchange_publish(struct fx_exchange *,struct fx_program *);
void fx_exchange_collect(struct fx_exchange *);
void fx_exchange_process(struct fx_exchange *,float *,size_t);
/* Audio-only observation of one stage of the active chain, before/after DSP. */
typedef void (*fx_stage_tap)(void*,const float*,size_t,int after);
void fx_exchange_process_keyed(struct fx_exchange *,float *,size_t,unsigned,fx_stage_tap,void *,
                               const float *const keys[FX_SIDECHAIN_KEYS]);
void fx_exchange_process_tapped(struct fx_exchange *,float *,size_t,unsigned,fx_stage_tap,void *);
/* Audio callback must be stopped and joined before destroy. */
void fx_exchange_destroy(struct fx_exchange *);
#ifdef __cplusplus
}
#endif
#endif
