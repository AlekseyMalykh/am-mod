/* Audit only. Native takeover code and its state are never changed. */
#include "takeover_native.h"
#include <stdio.h>
#include <string.h>
int takeover_native_check(uintptr_t base)
{
    if(!base)return 0;
    const uintptr_t offsets[]={0x15f5690,0x15e70c0};
    const uint32_t frames[]={0xa9b67bfd,0xa9b77bfd};
    for(unsigned i=0;i<2;++i){
        uint32_t words[3];memcpy(words,(const void*)(base+offsets[i]),sizeof(words));
        if(words[0]!=0xd503233f||words[1]!=frames[i]||words[2]!=0x910003fd)return 0;
    }
    fprintf(stderr,"TAKEOVER NATIVE_PROCESSORS_PRESERVED RotaryFixedConnection TrimParameter\n");
    return 1;
}
