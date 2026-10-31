#include "src/phone_input.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void tap(struct phone_input*s,unsigned pad,unsigned time){assert(phone_pad(s,pad,1,time)&&phone_pad(s,pad,0,time+1));}
static void expect(struct phone_input*s,const char*text){char out[600];phone_utf8(s,out,sizeof(out));assert(!strcmp(text,out));}
int main(void){
 struct phone_input s;char view[600];phone_init(&s,128);phone_enable(&s,1);
 tap(&s,14,10);tap(&s,14,100);expect(&s,"b");phone_view(&s,20,view,sizeof(view));assert(!strcmp(view,"b|"));
 phone_move(&s,-1);assert(s.cursor==0&&!s.pending);tap(&s,16,200);expect(&s,"b");
 s.shift=1;tap(&s,16,210);expect(&s,"");s.shift=0;
 assert(phone_sync(&s,"abcd",2));tap(&s,14,300);expect(&s,"abacd");tap(&s,14,310);expect(&s,"abbcd");
 phone_move(&s,-1);assert(s.cursor==2);tap(&s,16,320);expect(&s,"abcd");assert(s.cursor==1);
 s.shift=1;tap(&s,16,330);expect(&s,"acd");assert(s.cursor==1);
 phone_move(&s,1000);tap(&s,16,340);expect(&s,"acd");s.shift=0;tap(&s,16,350);expect(&s,"ac");
 assert(phone_sync(&s,"a\xc3\xa9\xd0\x96z",2));phone_view(&s,20,view,sizeof(view));assert(!strcmp(view,"a\xc3\xa9|\xd0\x96z"));
 tap(&s,16,400);expect(&s,"a\xd0\x96z");s.shift=1;tap(&s,16,410);expect(&s,"az");
 phone_init(&s,2);phone_enable(&s,1);tap(&s,14,500);tap(&s,15,510);expect(&s,"ad");tap(&s,15,520);expect(&s,"ae");
 phone_move(&s,-1);tap(&s,13,600);expect(&s,"ae");assert(s.length==2);phone_move(&s,-1000);assert(s.cursor==0);
     {   /* Lost release: a later down on a held pad is a new tap; a duplicate down is not. */
        struct phone_input s;phone_init(&s,32);phone_enable(&s,1);phone_sync(&s,"abc",3);
        assert(phone_pad(&s,1,1,1000)&&s.cursor==2);
        assert(phone_pad(&s,1,1,1050)&&s.cursor==2);   /* duplicate within 120 ms */
        assert(phone_pad(&s,1,1,1400)&&s.cursor==1);   /* release was lost */
        assert(phone_pad(&s,1,0,1500)&&!(s.held&1u));
        assert(phone_pad(&s,3,1,1600)&&s.cursor==2);
    }
puts("PASS pad input insertion and multi-tap, encoder cursor without text change, display-only caret, Backspace/Delete at beginning/end/middle, Unicode boundaries and capacity");return 0;
}
