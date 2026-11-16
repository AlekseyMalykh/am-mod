#ifndef AM_USB_DRIVE_INPUT_H
#define AM_USB_DRIVE_INPUT_H
#include <stddef.h>
#include <string.h>
/* Both native ports report id=SerialMIDI. The device name distinguishes
 * Control Surface (card0) from External MIDI (card1). Require BOTH fields. */
static inline int usb_drive_control_identity(const char*id,size_t id_size,const char*name,size_t name_size){
 const char expected_id[]="SerialMIDI",expected_name[]="Control Surface";
 return id_size>=sizeof(expected_id)&&name_size>=sizeof(expected_name)&&
        !memcmp(id,expected_id,sizeof(expected_id))&&!memcmp(name,expected_name,sizeof(expected_name));
}
static inline int usb_drive_internal_transport(const char*path){
 return path&&path[0]=='/'&&!strstr(path,"/usb");
}
struct usb_drive_input {unsigned status,first,count,sysex,held,armed,confirm;};
/* Surface buttons are channel-1 notes, not CC: physical B3 sends
 * 90 0e 7f / 80 0e 00. Handle running status and interleaved real time.
 * B1/B3 note numbers are 0x0c/0x0e. Require a fresh complete
 * click; the Connect press held across app shutdown must never exit. */
static inline int usb_drive_input_byte(struct usb_drive_input*p,unsigned byte,int enabled){
 if(byte>=0xf8)return 0;
 if(byte&0x80){
  p->count=0;
  if(byte>=0xf0){p->status=0;p->sysex=byte==0xf0;return 0;}
  p->sysex=0;p->status=byte;return 0;
 }
 if(p->sysex||!p->status)return 0;
 unsigned kind=p->status&0xf0;
 if(kind==0xc0||kind==0xd0)return 0;
 if(!p->count){p->first=byte;p->count=1;return 0;}
 p->count=0;
 if((p->status!=0x90&&p->status!=0x80)||(p->first!=0x0c&&p->first!=0x0e))return 0;
 unsigned mask=p->first==0x0c?1u:2u;
 if(p->status==0x90&&byte){
  if(!(p->held&mask)&&enabled)p->armed|=mask;
  p->held|=mask;return 0;
 }
 int fresh=(p->held&p->armed&mask)!=0;
 p->held&=~mask;p->armed&=~mask;
 if(!enabled||!fresh)return 0;
 if(mask==1){p->confirm=0;return 1;}
 if(!p->confirm){p->confirm=1;return 1;}
 p->confirm=0;return 2;
}
#endif
