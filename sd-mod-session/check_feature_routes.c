#define MPC_EXPERIMENTAL_CHOP_EXPORT 1
#include "src/workflow_ui.c"
#include <assert.h>
#include <stdio.h>
int main(void){
 atomic_store(&active,1);atomic_store(&custom,0);atomic_store(&button_routes,2);
 assert(workflow_owns(2,0x0c));assert(workflow_owns(2,0x31));
 assert(!workflow_owns(1,0x11));atomic_store(&audition_owned,1);assert(workflow_owns(1,0x11)&&!workflow_owns(1,0x10));
 atomic_store(&audition_owned,0);assert(!workflow_owns(1,0x11));
 atomic_store(&custom,1);assert(workflow_owns(3,7));
 atomic_store(&audition_owned,1);assert(workflow_owns(3,7)); /* Query keyboard still owns its pads. */
 atomic_store(&audition_result_pads,1);assert(!workflow_owns(3,7)&&!workflow_owns(3,15));
 assert(workflow_replacement_active());
 atomic_store(&audition_owned,0);atomic_store(&custom,0);
 atomic_store(&button_routes,0);atomic_store(&chop_available,1);
 assert(!workflow_owns(2,0x0c));assert(!workflow_owns(2,0x31));
 assert(!workflow_blocks_sample_controls());
 atomic_store(&button_routes,2);assert(workflow_blocks_sample_controls());
 puts("PASS browser audition owns B1 and Shift edges and K2 only while audition is active; hidden Chop root does not block sample controls");return 0;
}
