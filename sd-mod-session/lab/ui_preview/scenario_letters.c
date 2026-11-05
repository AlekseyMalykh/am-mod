#include <assert.h>
static void q(const char*want,size_t cur){char t[64];phone_utf8(&query,t,sizeof t);if(strcmp(t,want)||query.cursor!=cur){fprintf(stderr,"got '%s' %zu want '%s' %zu\n",t,query.cursor,want,cur);assert(0);}}
int main(void){
 phone_init(&query,64);phone_enable(&query,0);
 add_letter();q("a",0);            /* empty query: first letter */
 change_character(2);q("c",0);
 add_letter();q("c",1);            /* next empty position */
 change_character(1);q("ca",1);
 add_letter();q("ca",2);change_character(11);q("cak",2);
 query.cursor=0;add_letter();q("ccak",1); /* middle: copy of current letter */
 phone_delete(&query,query.cursor<query.length);q("cak",1);
 puts("PASS encoder letters: press adds, middle insert copies, Erase deletes selected");return 0;}
