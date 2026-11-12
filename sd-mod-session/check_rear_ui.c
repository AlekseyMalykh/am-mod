#include "src/rear_ui.c"
#include <assert.h>
static unsigned settings[2][6],bank,occupied[8],starts,stops,busy,repeat,fx,monitor_on,last_overwrite;
int music_ui_repeat_active(void){return repeat;}
int music_ui_shift_active(void){return 0;}
int music_ui_knob_fx_active(void){return fx!=0;}
unsigned rear_setting(unsigned c,unsigned f){return settings[c][f];}
int rear_set_setting(unsigned c,unsigned f,unsigned v){settings[c][f]=v;return 1;}
int step_native_browser_bank(int n){if(n>=0)bank=n;return (int)bank;}
unsigned rear_native_bank_occupied(unsigned n){return occupied[n];}
int rear_native_busy(void){return busy;}
int rear_native_start(unsigned n){starts++;last_overwrite=n;busy=1;return 1;}
void rear_native_stop(void){stops++;busy=0;}
void rear_native_clear(void){}
void rear_native_monitor_on(void){monitor_on++;}
int rear_native_source(void){return 1;}
static void button(unsigned c){unsigned char e[16]={0};e[8]=c;e[9]=1;assert(rear_ui_button(e));e[9]=0;assert(rear_ui_button(e));}
static void cc(unsigned c,unsigned v){unsigned char e[16]={0};e[8]=c;e[9]=v;assert(rear_ui_control(e));}
int main(void){
 atomic_store(&open_panel,1);atomic_store(&visible_roots,2);panel_on_record=1;
 settings[0][REAR_MODE]=REAR_DUAL;
 rear_ui_pad(3,1);assert(waiting_right&&starts==0&&settings[0][REAR_TARGET]==3);
 button(0x0c);assert(!waiting_right&&open_panel&&!starts&&!selected_mask);
 rear_ui_pad(4,1);button(0x51);assert(!waiting_right&&open_panel&&!starts);
 rear_ui_pad(4,1);rear_ui_pad(4,1);assert(!waiting_right&&!starts);
 rear_ui_pad(4,1);bank=7;rear_ui_pad(5,1);assert(starts==1&&settings[1][REAR_TARGET]==117&&busy);
 button(0x51);assert(stops==1&&!busy);
 cc(0x10,0);assert(settings[0][REAR_MODE]==REAR_LEFT);
 occupied[7]=1u<<8;rear_ui_pad(8,1);assert(confirm&&starts==1);
 button(0x0c);assert(!confirm&&!selected_mask);
 rear_ui_pad(8,1);button(0x0e);assert(starts==2&&last_overwrite==1);button(0x51);
 cc(0x10,64);assert(settings[0][REAR_MODE]==REAR_RIGHT);
 rear_ui_pad(9,1);assert(starts==3&&settings[1][REAR_TARGET]==121);button(0x51);
 button(0x0d);button(0x0e);assert(settings[0][REAR_MONITOR]&&settings[1][REAR_MONITOR]&&monitor_on==2);
 cc(0x11,0);cc(0x12,127);assert(settings[0][REAR_PAN]==0&&settings[1][REAR_PAN]==100);
 fx=2;assert(!rear_ui_owns(1,0x10)&&!rear_ui_owns(2,0x0d)&&!rear_ui_pad_owns());
 unsigned char e[16]={0};e[8]=0x11;e[9]=127;assert(!rear_ui_control(e)&&settings[0][REAR_PAN]==0);
 fx=0;repeat=1;assert(!rear_ui_owns(1,0x10)&&!rear_ui_pad_owns());repeat=0;
 assert(rear_ui_owns(1,0x10)&&rear_ui_pad_owns());
 panel_on_record=0;rear_ui_pad(0,1);assert(starts==3&&!rear_ui_pad_owns());
 settings[0][REAR_ENABLED]=0;
 e[8]=0x11;e[9]=64;
 assert(rear_ui_control_at(e,1)&&settings[0][REAR_LEVEL]==50&&!settings[0][REAR_ENABLED]);
 assert(rear_ui_reset_knob(1,1)&&settings[0][REAR_LEVEL]==100&&!settings[0][REAR_ENABLED]);
 puts("PASS direct pad mono/dual record, cancellation B1/STOP/repeated pad, cross-bank choice, occupied-pad confirmation, monitor controls, FX/Repeat priority");
}
