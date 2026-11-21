#include "src/program_pgm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char bytes[AM_PGM_BYTES];
static void fixture(void){
    memset(bytes,0,sizeof(bytes));bytes[0]=4;bytes[1]=42;
    memcpy(bytes+4,"MPC1000 PGM 1.00",16);memset(bytes+0x2958,64,128);
    memcpy(bytes+24,"Kick",4);bytes[24+17]=85;bytes[24+19]=127;
    bytes[24+0x8f]=100;bytes[24+0x90]=50;
}
static void rejected(size_t size,enum am_pgm_status status,size_t off){
    struct am_pgm_program p,previous;memset(&p,0xa5,sizeof(p));previous=p;
    struct am_pgm_error e;assert(am_pgm_read(bytes,size,&p,&e)==status);
    assert(e.status==status&&e.offset==off&&!memcmp(&p,&previous,sizeof(p)));
}
int main(void){
    struct am_pgm_program p;struct am_pgm_error e;
    fixture();assert(am_pgm_read(bytes,sizeof(bytes),&p,&e)==AM_PGM_OK);
    assert(!strcmp(p.pads[0].layers[0].name,"Kick")&&p.pads[0].layers[0].level==85);
    /* Empty name with stale bytes and invalid stale parameters is still empty. */
    memcpy(bytes+24+24+1,"ick 2nd Wind",12);bytes[24+24+17]=255;
    assert(am_pgm_read(bytes,sizeof(bytes),&p,&e)==AM_PGM_OK&&!p.pads[0].layers[1].name[0]);
    for(unsigned s=0;s<4;++s){size_t o=24+s*24;memset(bytes+o,0,24);memcpy(bytes+o,"ABCDEFGHIJKLMNOP",16);bytes[o+17]=100;bytes[o+18]=(unsigned char)(s*32);bytes[o+19]=(unsigned char)(s*32+31);bytes[o+20]=0x9c;bytes[o+21]=0xff;bytes[o+22]=1;}
    assert(am_pgm_read(bytes,sizeof(bytes),&p,&e)==AM_PGM_OK);
    assert(strlen(p.pads[0].layers[3].name)==16&&p.pads[0].layers[3].tune_cents==-100&&p.pads[0].layers[3].velocity_high==127);
    /* Both MIDI tables survive independently, including multiple notes per pad. */
    bytes[0x2918]=37;bytes[0x2958+37]=0;bytes[0x2958+50]=0;
    assert(am_pgm_read(bytes,sizeof(bytes),&p,NULL)==AM_PGM_OK&&p.note_for_pad[0]==37&&p.pad_for_note[50]==0);
    uint8_t busy[128]={0},map[64],old[64];memset(map,0x55,sizeof(map));memcpy(old,map,sizeof(map));
    assert(!am_pgm_plan(&p,1,busy,map)&&!memcmp(map,old,sizeof(map)));
    busy[112]=1;assert(!am_pgm_plan(&p,112,busy,map)&&!memcmp(map,old,sizeof(map)));
    busy[112]=0;assert(am_pgm_plan(&p,112,busy,map)&&map[0]==112&&map[1]==255);
    p.pads[16].layers[0].name[0]='x';memcpy(old,map,sizeof(map));
    assert(!am_pgm_plan(&p,112,busy,map)&&!memcmp(map,old,sizeof(map)));
    assert(am_pgm_plan(&p,96,busy,map)&&map[0]==96&&map[16]==112);
    assert(!am_pgm_plan(NULL,0,busy,map)&&!am_pgm_plan(&p,0,NULL,map)&&!am_pgm_plan(&p,0,busy,NULL));
    fixture();rejected(0,AM_PGM_SIZE,0);rejected(AM_PGM_BYTES-1,AM_PGM_SIZE,0);rejected(AM_PGM_BYTES+1,AM_PGM_SIZE,0);
    bytes[0]=0;rejected(sizeof(bytes),AM_PGM_SIZE,0);
    fixture();bytes[4]='X';rejected(sizeof(bytes),AM_PGM_FORMAT,4);
    fixture();bytes[24]='/';rejected(sizeof(bytes),AM_PGM_NAME,24);
    fixture();bytes[24+17]=101;rejected(sizeof(bytes),AM_PGM_RANGE,24+17);
    fixture();bytes[24+18]=127;bytes[24+19]=126;rejected(sizeof(bytes),AM_PGM_RANGE,24+18);
    fixture();bytes[24+20]=0x11;bytes[24+21]=0x0e;rejected(sizeof(bytes),AM_PGM_RANGE,24+20);
    fixture();bytes[24+0x90]=101;rejected(sizeof(bytes),AM_PGM_RANGE,24+0x90);
    fixture();bytes[0x2918]=128;rejected(sizeof(bytes),AM_PGM_RANGE,0x2918);
    fixture();bytes[0x2958]=65;rejected(sizeof(bytes),AM_PGM_RANGE,0x2958);
    fixture();bytes[0x29d8]=129;rejected(sizeof(bytes),AM_PGM_RANGE,0x29d8);
    assert(am_pgm_read(NULL,0,&p,&e)==AM_PGM_ARGUMENT&&am_pgm_read(bytes,sizeof(bytes),NULL,NULL)==AM_PGM_ARGUMENT);
    /* Every byte is mutated independently, including unaligned input. All
     * accepted programs must remain bounded; failures cannot partially commit. */
    fixture();unsigned char unaligned[AM_PGM_BYTES+1];
    for(size_t i=0;i<sizeof(bytes);++i){
        unsigned char saved=bytes[i];bytes[i]^=255;memcpy(unaligned+1,bytes,sizeof(bytes));
        struct am_pgm_program previous;memset(&p,0xa5,sizeof(p));previous=p;
        enum am_pgm_status s=am_pgm_read(unaligned+1,sizeof(bytes),&p,&e);
        if(s==AM_PGM_OK){for(unsigned a=0;a<64;++a)for(unsigned b=0;b<4;++b)assert(p.pads[a].layers[b].name[16]==0);}
        else assert(!memcmp(&p,&previous,sizeof(p)));
        bytes[i]=saved;
    }
    puts("PASS PGM strict detection, stale empty layers, four velocity layers, signed tune, MIDI aliases, destination conflicts/bounds, transactional failure and 10756 unaligned mutations");
    return 0;
}
