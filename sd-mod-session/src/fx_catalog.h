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
/* Display-only control; never an index into parameter[]. */
#define FX_CONTROL_AMP_MODEL FX_CHAIN_PARAMETERS
float fx_catalog_parameter_value(const struct fx_stage*,unsigned);
unsigned fx_catalog_count(void);
unsigned fx_amp_model_count(void);
unsigned fx_amp_model_index(const struct fx_stage*);
const char *fx_amp_model_name(unsigned);
int fx_amp_model_apply(struct fx_stage*,unsigned);
float fx_catalog_parameter_default(const struct fx_stage*,unsigned);
unsigned fx_mixer_catalog_count(void);
/* Add Effect filter: 0 = All, then each effect category in catalog order. */
unsigned fx_mixer_category_count(void);
const char *fx_mixer_category_name(unsigned filter);
void fx_mixer_search_set(const char*text); /* case-insensitive name substring; "" = off */
const char *fx_mixer_search_get(void);
unsigned fx_mixer_filtered_count(unsigned filter);
unsigned fx_mixer_filtered_index(unsigned filter,unsigned position); /* real catalog index */
const struct fx_catalog_entry *fx_mixer_catalog_at(unsigned);
int fx_mixer_catalog_stage(unsigned,double,struct fx_stage*);
unsigned fx_catalog_parameter_count(const struct fx_catalog_entry*);
/* A page has six display cells: K1-K3, then Shift+K1-K3. -1 is blank.
 * parameter_count counts named storage parameters, not display cells. */
unsigned fx_catalog_page_count(const struct fx_catalog_entry*);
const char *fx_catalog_page_name(const struct fx_catalog_entry*,unsigned page);
const char *fx_catalog_parameter_label(const struct fx_catalog_entry*,unsigned index);
int fx_catalog_parameter_index(const struct fx_catalog_entry*,unsigned position);
/* 1 when the Shift layer of this page holds at least one parameter; otherwise Shift keeps the normal layer. */
/* Parameters that are lists of named choices in the stock firmware (Amp Sim "Cab Model"): the number
 * of choices (0 = plain 0..100%) and the name for a normalized value. */
unsigned fx_catalog_choice_count(const struct fx_catalog_entry*,unsigned index);
const char *fx_catalog_choice_name(const struct fx_catalog_entry*,unsigned index,float normalized);
int fx_catalog_shift_layer(const struct fx_catalog_entry*,unsigned page);
const struct fx_catalog_entry *fx_catalog_at(unsigned);
const struct fx_catalog_entry *fx_catalog_find(uint32_t);
const char *fx_catalog_stage_name(const struct fx_stage*);
int fx_catalog_stage(unsigned,double,struct fx_stage*);
#ifdef __cplusplus
}
#endif
#endif
