static unsigned st[2][6]={{1,100,50,1,REAR_DUAL,7*16+13},{0,80,30,1,REAR_DUAL,7*16+15}};
static int s_busy,s_shift;static unsigned s_state;
unsigned rear_setting(unsigned c,unsigned f){return st[c&1][f];}
void rear_audio_meters(float l[2]){l[0]=.35f;l[1]=.08f;}
unsigned rear_record_state(void){return s_state;}
uint64_t rear_record_frames(void){return 70560;}
int rear_native_busy(void){return s_busy;}
unsigned rear_native_status(void){return 0;}
const char*rear_native_message(void){return "";}
int rear_native_source(void){return 1;}
int music_ui_shift_active(void){return s_shift;}
int music_ui_repeat_active(void){return 0;}
unsigned phone_native_button_light(unsigned c){(void)c;return 0;}
void rear_ui_wake(void){}
int main(void){
 static uintptr_t fake[64];image=0;void*root=fake;fake[0]=0x6e22970; /* record panel */
 atomic_store(&open_panel,1);panel_on_record=1;
 PREVIEW_BEGIN("rear-1-record.ops");paint(root,0);PREVIEW_END();
 atomic_store(&selected_mask,1);waiting_right=1;
 PREVIEW_BEGIN("rear-2-waiting.ops");paint(root,0);PREVIEW_END();
 waiting_right=0;atomic_store(&selected_mask,3);s_busy=1;s_state=REAR_RECORDING;
 PREVIEW_BEGIN("rear-3-recording.ops");paint(root,0);PREVIEW_END();
 s_busy=0;confirm=1;overwrite=3;
 PREVIEW_BEGIN("rear-4-confirm.ops");paint(root,0);PREVIEW_END();
 confirm=0;fake[0]=0;panel_on_record=0;s_shift=1;atomic_store(&selected_mask,0);
 PREVIEW_BEGIN("rear-5-config-shift.ops");paint(root,0);PREVIEW_END();
 return 0;}
