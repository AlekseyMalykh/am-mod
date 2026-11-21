#ifndef AM_PROGRAM_IMPORT_H
#define AM_PROGRAM_IMPORT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* UI owner. A detached reader is retained through review and dispatch.
 * No occupied destination is replaced; commit is one native Undo command. */
int program_import_open(uintptr_t base,const char *path,void *context,char error[96]);
int program_import_plan(unsigned bank,unsigned *pads,unsigned *layers,char error[96]);
int program_import_commit(unsigned bank,char error[96]);
void program_import_close(void);
#ifdef __cplusplus
}
#endif
#endif
