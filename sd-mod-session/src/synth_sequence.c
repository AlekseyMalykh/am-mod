#include "synth_sequence.h"
#include <string.h>
#ifdef MPC_PAD_MIDI_RECORDING_TRIAL
int am_synth_sequence_stop(struct am_synth_sequence*p){
 if(!p||!p->epoch||p->transport==UINT64_MAX)return AM_SYNTH_SEQUENCE_INVALID;
 if(!am_synth_owned_reset())return AM_SYNTH_SEQUENCE_BUSY;
 ++p->transport;return AM_SYNTH_SEQUENCE_ACCEPTED;
}
int am_synth_sequence_configure(struct am_synth_sequence*p,uint64_t epoch){
 if(!p||!epoch||epoch<=p->epoch)return AM_SYNTH_SEQUENCE_INVALID;
 if(!am_synth_owned_reset())return AM_SYNTH_SEQUENCE_BUSY;
 *p=(struct am_synth_sequence){epoch,1,0};return AM_SYNTH_SEQUENCE_ACCEPTED;
}
int am_synth_sequence_on(struct am_synth_sequence*p,struct am_synth_sequence_voice*v,uint64_t epoch,uint64_t transport,uint64_t occurrence,
                         const struct am_midi_event*e,int transpose){
 if(!p||!p->epoch||!v||!e||!epoch||!occurrence)return AM_SYNTH_SEQUENCE_INVALID;
 if(epoch!=p->epoch||transport!=p->transport||occurrence<=p->last_occurrence)return AM_SYNTH_SEQUENCE_IGNORED;
 if(e->off||e->muted)return AM_SYNTH_SEQUENCE_IGNORED;
 if(e->tick<0||e->duration<=0||!e->velocity||e->velocity>127||e->slice||transpose< -127||transpose>127)return AM_SYNTH_SEQUENCE_INVALID;
 /* The recording codec uses Tune relative to MIDI60. Bound transpose before
  * arithmetic; source_note remains provenance, never the musical pitch. */
 int pitch=60+(int)e->tune+transpose;if(pitch<0||pitch>127)return AM_SYNTH_SEQUENCE_INVALID;
 if(v->occurrence&&v->epoch==p->epoch&&v->transport==p->transport)return AM_SYNTH_SEQUENCE_BUSY;
 struct am_synth_handle handle;
 if(!am_synth_owned_on((unsigned)pitch,e->velocity,&handle))return AM_SYNTH_SEQUENCE_BUSY;
 *v=(struct am_synth_sequence_voice){p->epoch,p->transport,occurrence,handle,0};p->last_occurrence=occurrence;
 return AM_SYNTH_SEQUENCE_ACCEPTED;
}
int am_synth_sequence_off(struct am_synth_sequence*p,struct am_synth_sequence_voice*v,uint64_t epoch,uint64_t transport,uint64_t occurrence){
 if(!p||!p->epoch||!v||!epoch||!occurrence)return AM_SYNTH_SEQUENCE_INVALID;
 if(epoch!=p->epoch||transport!=p->transport)return AM_SYNTH_SEQUENCE_IGNORED;
 if(v->epoch!=p->epoch||v->transport!=p->transport||v->occurrence!=occurrence)return AM_SYNTH_SEQUENCE_IGNORED;
 if(!am_synth_owned_off(v->handle)){v->releasing=1;return AM_SYNTH_SEQUENCE_BUSY;}
 memset(v,0,sizeof *v);return AM_SYNTH_SEQUENCE_ACCEPTED;
}
int am_synth_sequence_retry(struct am_synth_sequence*p,struct am_synth_sequence_voice*v){
 if(!p||!p->epoch||!v)return AM_SYNTH_SEQUENCE_INVALID;
 if(v->epoch!=p->epoch||v->transport!=p->transport){memset(v,0,sizeof *v);return AM_SYNTH_SEQUENCE_IGNORED;}
 if(!v->occurrence||!v->releasing)return AM_SYNTH_SEQUENCE_IGNORED;
 return am_synth_sequence_off(p,v,p->epoch,p->transport,v->occurrence);
}
#endif
