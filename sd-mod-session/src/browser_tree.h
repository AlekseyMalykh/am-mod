#ifndef MPC_BROWSER_TREE_H
#define MPC_BROWSER_TREE_H
#include <stddef.h>
void browser_tree_close(void);
int browser_tree_accepts(const char*name,int projects);
int browser_tree_start(const char *root,int projects);
/* Bounded read-only DFS: one file, 0=yield, -1=done. Linked directories are
 * deduplicated by device/inode; depth and visit budgets also bound loops. */
int browser_tree_next(char *path,size_t capacity,unsigned budget);
int browser_tree_limited(void);
int browser_tree_error(void);
#endif
