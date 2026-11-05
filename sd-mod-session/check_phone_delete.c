#include "src/phone_input.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){struct phone_input s;char t[64];phone_init(&s,64);phone_enable(&s,1);
 phone_sync(&s,"kick",4);assert(phone_delete(&s,0));phone_utf8(&s,t,sizeof t);assert(!strcmp(t,"kic")&&s.cursor==3);
 assert(!phone_delete(&s,1));s.cursor=1;assert(phone_delete(&s,1));phone_utf8(&s,t,sizeof t);assert(!strcmp(t,"kc")&&s.cursor==1);
 s.cursor=0;assert(!phone_delete(&s,0));
 phone_sync(&s,"ab",2);s.shift=1;phone_pad(&s,16,1,0);phone_pad(&s,16,0,0);phone_utf8(&s,t,sizeof t);assert(!strcmp(t,"ab"));
 s.cursor=0;phone_pad(&s,16,1,10);phone_pad(&s,16,0,20);phone_utf8(&s,t,sizeof t);assert(!strcmp(t,"b"));
 s.shift=0;s.cursor=1;phone_pad(&s,16,1,30);phone_pad(&s,16,0,40);phone_utf8(&s,t,sizeof t);assert(!strcmp(t,""));
 phone_enable(&s,0);phone_sync(&s,"xyz",1);assert(phone_delete(&s,s.cursor<s.length));phone_utf8(&s,t,sizeof t);assert(!strcmp(t,"xz")&&s.cursor==1);
 puts("PASS phone_delete: backspace, forward delete, Shift+16, encoder-mode delete");}
