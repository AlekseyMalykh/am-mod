/* Real native tree/Undo aggregation with the installed pitch-aware lookup.
 * No GUI, transport, recorder commit or disk save is claimed here. */
static void check_overdub_matrix(void){
    phase="16-pitch overdub lookup and native Undo aggregation";
    struct tree tree={0};tree.left=tree.right=&tree.colour;
    struct step_event notes[16],snapshots[16];void *nodes[16];
    unsigned char batch[0x60];init_batch(batch);
    for(unsigned i=0;i<16;++i){
        notes[i]=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))(36,0,60+i*120,100,0,.8f);
        FN(0x27e4400,void(*)(void*,int,int,int))((char*)&notes[i]+0x14,0,0,(int)i-8);
    }
    /* Deliberately scrambled insert order: matching must not depend on which
     * pitch happens to be the native lower-bound candidate. */
    for(unsigned k=0;k<16;++k){
        unsigned i=(k*7)%16;
        FN(0x1574460,void(*)(void*,const void*))(&tree,&notes[i]);
        FN(0x2137ed0,void(*)(void*,void*,const void*))(batch,&tree,&notes[i]);
        assert(tree.size==k+1&&batch_size(batch)==k+1);
    }
    for(unsigned i=0;i<16;++i){
        nodes[i]=find_recorded(&tree,&notes[i]);assert(nodes[i]!=&tree.colour);
        const struct step_event *stored=(void*)((char*)nodes[i]+0x20);
        /* Native copies omit the four bytes of event padding at +12. */
        assert(stored->time==notes[i].time&&stored->type==notes[i].type);
        assert(!memcmp(stored->data,notes[i].data,sizeof notes[i].data));
        memcpy(&snapshots[i],stored,sizeof snapshots[i]);
        for(unsigned j=0;j<i;++j)assert(nodes[i]!=nodes[j]);
    }
    unsigned checks=0;
    const int64_t lengths[]={1,60,180,1076,15540};
    for(unsigned pass=0;pass<5;++pass){
        for(unsigned k=0;k<16;++k){
            unsigned i=15-k;
            struct step_event replacement=notes[i];
            F(&replacement,0x28,int64_t)=lengths[pass];
            F(&replacement,0x10,float)=(float)(16+i)/127.f;
            assert(find_recorded(&tree,&replacement)==nodes[i]);
            /* Real added-event aggregation retains one slot per pitch even
             * when the same onset is overdubbed with velocity/duration edits. */
            FN(0x2137ed0,void(*)(void*,void*,const void*))(batch,&tree,&replacement);
            assert(tree.size==16&&batch_size(batch)==16);
            /* Lookup and Undo aggregation must not modify source sequence. */
            for(unsigned j=0;j<16;++j)
                assert(!memcmp((char*)nodes[j]+0x20,&snapshots[j],sizeof snapshots[j]));
            ++checks;
        }
    }
    for(unsigned i=0;i<16;++i){
        struct step_event other=notes[i];
        other.time=1;assert(find_recorded(&tree,&other)==&tree.colour);
        other=notes[i];F(&other,0x30,unsigned char)=37;
        assert(find_recorded(&tree,&other)==&tree.colour);
        other=notes[i];F(&other,0x31,unsigned char)=1;
        assert(find_recorded(&tree,&other)==&tree.colour);
        other=notes[i];F(&other,0x14,uint16_t)=0;
        assert(find_recorded(&tree,&other)==&tree.colour);
        checks+=4;
    }
    printf("PASS %u overdub identity checks: 16 simultaneous pitches, independent duration/velocity replacement lookup, source/channel/time/modifier isolation; native Undo aggregation retains 16 pitches.\n",checks);
}

static void check_chord_boundaries(void){
    phase="16-pitch pending releases across loop";
    unsigned char recorder[0x1c0]={0};void *header=recorder+0x1a0;
    F(header,0,void*)=F(header,8,void*)=header;
    void *program=&recorder;struct step_event notes[16];
    for(unsigned i=0;i<16;++i){
        notes[i]=FN(0x27e54d0,struct step_event(*)(unsigned,int64_t,int64_t,unsigned,unsigned,float))(36,7660,0,100,0,.8f);
        FN(0x27e4400,void(*)(void*,int,int,int))((char*)&notes[i]+0x14,0,0,(int)i-8);
        pending(recorder,program,&notes[i]);
        void *node=F(header,8,void*);
        F(node,0x58,int64_t)=F(node,0x60,int64_t)=7660+(i%2)*20;
    }
    for(unsigned k=0;k<16;++k){
        unsigned i=(k*7)%16;
        unsigned note=CHOICE_MARK|((unsigned)notes[i].data[6]<<8)|36;
        commit_count=0;
        finish_note(recorder,program,7,note,0,8640+i*6,0,0,0);
        assert(commit_count==1&&same_choice(&committed[0],&notes[i]));
        assert(F(&committed[0],0x28,int64_t)==980+i*6-(i%2)*20);
        assert(F(recorder,0x1b0,uint64_t)==15-k);
        commit_count=0;
        finish_note(recorder,program,7,note,0,9000,0,0,0);
        assert(!commit_count&&F(recorder,0x1b0,uint64_t)==15-k);
    }
    assert(F(header,0,void*)==header&&F(header,8,void*)==header);
    puts("PASS 32 chord boundary releases: 16 pitches finalized independently in scrambled order across loop; stale duplicate Off cannot consume another pitch.");
}
