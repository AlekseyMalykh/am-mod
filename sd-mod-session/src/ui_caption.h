#ifndef MPC_UI_CAPTION_H
#define MPC_UI_CAPTION_H
#include <stddef.h>
#include <string.h>
/* Presentation only: keep whole UTF-8 characters; never change file names. */
static inline size_t ui_prefix(const char*s,size_t characters){
 size_t n=0;while(s[n]&&characters--){++n;while(((unsigned char)s[n]&0xc0)==0x80)++n;}return n;
}
static inline void ui_caption(char*out,size_t capacity,const char*source,size_t characters){
 if(!capacity)return;
 size_t all=strlen(source),n=ui_prefix(source,characters);
 if(n==all&&all<capacity){memcpy(out,source,all+1);return;}
 if(capacity<4){out[0]=0;return;}
 n=ui_prefix(source,characters>3?characters-3:0);
 if(n>capacity-4){n=capacity-4;while(n&&((unsigned char)source[n]&0xc0)==0x80)--n;}
 memcpy(out,source,n);memcpy(out+n,"...",4);
}
#endif
