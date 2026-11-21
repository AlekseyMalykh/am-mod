#include <stddef.h>
#include "src/seq_audition.c"
int main(void){
 printf("{\"size\":%zu,\"project_size\":%zu,\"mutes\":%zu",sizeof A,sizeof(struct music_project),offsetof(struct music_project,sequence_mutes));
 #define F(k) printf(",\""#k"\":%zu",offsetof(__typeof__(A),k))
 F(active);F(hold);F(swapped);F(program);F(pad);F(inst);F(orig);F(cand);F(cand_ctrl);F(swaps);F(orig_tune);F(written_tune);
 puts("}");return 0;
}
