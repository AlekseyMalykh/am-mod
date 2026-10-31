#ifndef MPC_PROJECT_EXIT_H
#define MPC_PROJECT_EXIT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* 0 idle, 1 saving, 2 durable copy ready, 3 error. */
int project_exit_start(uintptr_t base, void *services);
int project_exit_install(uintptr_t base);
int project_exit_status(void);
const char *project_exit_message(void);
const char *project_exit_path(void);
#ifdef __cplusplus
}
#endif
#endif
