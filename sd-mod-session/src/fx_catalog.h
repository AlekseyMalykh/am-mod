#ifndef MPC_MOD_FX_CATALOG_H
#define MPC_MOD_FX_CATALOG_H
#include "fx_chain.h"
#ifdef __cplusplus
extern "C" {
#endif
struct fx_catalog_entry {
    uint32_t id;
    const char *name,*category;
    unsigned count;
    const char *parameter[FX_CHAIN_PARAMETERS];
    float defaults[FX_CHAIN_PARAMETERS];
};
unsigned fx_catalog_count(void);
unsigned fx_mixer_catalog_count(void);
const struct fx_catalog_entry *fx_mixer_catalog_at(unsigned);
int fx_mixer_catalog_stage(unsigned,double,struct fx_stage*);
unsigned fx_catalog_parameter_count(const struct fx_catalog_entry*);
int fx_catalog_parameter_index(const struct fx_catalog_entry*,unsigned position);
const struct fx_catalog_entry *fx_catalog_at(unsigned);
const struct fx_catalog_entry *fx_catalog_find(uint32_t);
const char *fx_catalog_stage_name(const struct fx_stage*);
int fx_catalog_stage(unsigned,double,struct fx_stage*);
#ifdef __cplusplus
}
#endif
#endif
