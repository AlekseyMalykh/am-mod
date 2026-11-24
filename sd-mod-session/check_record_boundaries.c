/* Native finalizer preconditions, not a complete recorder/transport test. */
#define MPC_POLY_TEST_MAIN legacy_poly_checks
#include "check_poly_record.c"
#include "check_overdub_matrix.inc"

static void boundary_case(int typed,int64_t onset,int64_t release,int64_t expected){
    unsigned char recorder[0x1c0]={0};void *header=recorder+0x1a0;
    F(header,0,void*)=F(header,8,void*)=header;
    void *program=&recorder;
    struct step_event event=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))(36,7660,0,100,0,.8f);
    if(typed)FN(0x27e4400,void(*)(void*,int,int,int))((char*)&event+0x14,0,0,4);
    pending(recorder,program,&event);
    void *node=F(header,8,void*);
    F(node,0x58,int64_t)=F(node,0x60,int64_t)=onset;
    commit_count=0;
    unsigned note=typed ? CHOICE_MARK|((unsigned)event.data[6]<<8)|36 : 36;
    finish_note(recorder,program,7,note,0,release,0,0,0);
    if(expected<0){
        assert(!commit_count&&F(recorder,0x1b0,uint64_t)==1);
        /* Release later in the same clock domain to free the pending node. */
        finish_note(recorder,program,7,note,0,onset+900,0,0,0);
        assert(commit_count==1&&F(&committed[0],0x28,int64_t)==900);
    }else{
        assert(commit_count==1&&F(&committed[0],0x28,int64_t)==expected);
        assert(same_choice(&event,&committed[0]));
    }
    assert(!F(recorder,0x1b0,uint64_t));
    assert(F(header,0,void*)==header&&F(header,8,void*)==header);
}

int main(int argc,char **argv){
    legacy_poly_checks(argc,argv);
    check_overdub_matrix();
    check_chord_boundaries();
    const int64_t holds[]={0,1,36,60,96,240,600,900,15540};
    unsigned cases=0;
    for(int typed=0;typed<2;++typed){
        for(unsigned i=0;i<sizeof holds/sizeof *holds;++i){
            boundary_case(typed,7660,7660+holds[i],holds[i]?holds[i]:60);++cases;
            /* Quantized event position may wrap to zero; pending time remains
             * absolute. This fixture tests duration, not the quantizer itself. */
            boundary_case(typed,7680,7680+holds[i],holds[i]?holds[i]:60);++cases;
        }
        boundary_case(typed,7680,7670,typed?60:-1);++cases;
        /* Deliberately incompatible timestamp domains: negative control only.
         * No claim that the native transport actually supplies wrapped Off. */
        boundary_case(typed,7660,580,typed?60:-1);++cases;
    }
    printf("PASS %u native finalizer boundary cases: plain and 16 Levels; absolute multi-loop holds preserved; future/wrapped Off negative controls distinguished.\n",cases);
    return 0;
}
