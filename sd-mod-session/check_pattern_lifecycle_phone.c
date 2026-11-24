#include <assert.h>
extern void pattern_lifecycle_original_probe(unsigned);
static unsigned loads,reads,news;
static int*last_choice;
static void dispatch_load(void);
static void music_load(void*p,void*a){assert(p==(void*)0x1230&&a==(void*)0x4560);++loads;pattern_lifecycle_original_probe(1);}
static int music_read(void*p,void*a){assert(p==(void*)0x1230&&a==(void*)0x4560);++reads;pattern_lifecycle_original_probe(2);dispatch_load();return 7;}
static void new_project(void*p,int*choice){assert(p==(void*)0x1230);++news;last_choice=choice;pattern_lifecycle_original_probe(3);}
#include "src/pattern_phone_lifecycle.inc"
static void dispatch_load(void){PHONE_PATTERN_LOAD((void*)0x1230,(void*)0x4560);}
void pattern_lifecycle_call_load(void){dispatch_load();}
int pattern_lifecycle_call_read(void){return PHONE_PATTERN_READ((void*)0x1230,(void*)0x4560);}
void pattern_lifecycle_call_new(int*p){PHONE_PATTERN_NEW((void*)0x1230,p);}
unsigned pattern_lifecycle_loads(void){return loads;}
unsigned pattern_lifecycle_reads(void){return reads;}
unsigned pattern_lifecycle_news(void){return news;}
int*pattern_lifecycle_last_choice(void){return last_choice;}
