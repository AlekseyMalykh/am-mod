#ifndef AM_TEMP_WORKSPACE_STORE_H
#define AM_TEMP_WORKSPACE_STORE_H
/* Returns an open directory FD for the completed private TempProjects cache.
 * Existing cache contents are retained. seed may be -1 for a fresh device. */
int temp_workspace_store(int samples,int seed);
#endif
