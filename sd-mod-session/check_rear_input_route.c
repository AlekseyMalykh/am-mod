#include "src/mixer_control.h"
#include "src/rear_audio.h"
#include <assert.h>
#include <stdio.h>
static unsigned values[2][6],source=1,monitor,available=1;
unsigned rear_setting(unsigned c,unsigned f){return values[c][f];}
int rear_set_setting(unsigned c,unsigned f,unsigned v){assert(c<2&&f<6);values[c][f]=v;return 1;}
int rear_native_source(void){return (int)source;}
int rear_native_monitor(void){return (int)monitor;}
int rear_native_monitor_set(unsigned v){if(!available)return 0;monitor=v;return 1;}
int main(void){
 assert(mixer_view_input_rows(1)==15&&mixer_view_input_rows(2)==9&&mixer_view_input_rows(0)==8);
 for(unsigned i=0;i<15;++i)assert(mixer_view_input_row(i,1)<15);
 for(unsigned c=0;c<2;++c){
  unsigned row=9+c*3;
  assert(!mixer_input_row_apply(row,0,1,64)&&values[c][REAR_LEVEL]==50);
  assert(!mixer_input_row_apply(row,1000,0,0)&&values[c][REAR_LEVEL]==100);
  assert(!mixer_input_row_apply(row,-1000,0,0)&&!values[c][REAR_LEVEL]);
  assert(!mixer_input_row_apply(row,0,2,0)&&values[c][REAR_LEVEL]==100);
  assert(!mixer_input_row_apply(row+1,0,1,64)&&values[c][REAR_PAN]==50);
  assert(!mixer_input_row_apply(row+1,0,2,0)&&values[c][REAR_PAN]==(c?100:0));
 }
 values[1][REAR_MONITOR]=0;
 assert(!mixer_input_row_apply(11,1,0,0)&&values[0][REAR_MONITOR]&&!values[1][REAR_MONITOR]);
 assert(monitor==2&&values[0][REAR_ENABLED]);
 assert(!mixer_input_row_apply(1,-2,0,0)&&!monitor);
 assert(!mixer_input_row_apply(1,2,0,0)&&monitor==2&&!values[1][REAR_MONITOR]);
 assert(!mixer_input_row_apply(14,1,0,0)&&values[1][REAR_MONITOR]);
 assert(!mixer_input_row_apply(11,-1,0,0)&&!values[0][REAR_MONITOR]&&values[1][REAR_MONITOR]);
 source=5;unsigned saved=values[0][REAR_LEVEL];assert(mixer_input_row_apply(9,-10,0,0)&&values[0][REAR_LEVEL]==saved);
 puts("PASS Rear Input: independent L/R level/pan/monitor, limits/defaults, master monitor preserves channel selection; Mic/USB row order preserved");
}
