#include "src/midi_native.c"
#include <stddef.h>
int main(void){printf("{\"size\":%zu,\"id\":%zu,\"sound\":%zu,\"layer\":%zu,\"route_count\":%zu,\"source\":%zu,\"key_owner\":%zu,\"key\":%zu}\n",sizeof(struct live_note),offsetof(struct live_note,id),offsetof(struct live_note,sound),offsetof(struct live_note,layer),offsetof(struct live_note,route_count),offsetof(struct live_note,source),offsetof(struct live_note,key_owner),offsetof(struct live_note,key));return 0;}
