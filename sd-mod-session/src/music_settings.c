#define _GNU_SOURCE
#include "music_settings.h"
#ifdef MPC_MIDI_TOOLS
#include "midi_tools.h"
#endif
#ifdef MPC_COMPRESSOR
#include "compressor.h"
#endif
#ifdef MPC_PAD_EQ
#include "pad_eq.h"
#endif
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Card defaults are separate from musical data. A project carries its own
 * complete snapshot; loading it changes this session without rewriting the
 * card defaults. One atomic word is safe on the lighting/render paths. */
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
extern unsigned am_synth_param_get(unsigned) __attribute__((weak));
extern void am_synth_param_set(unsigned,unsigned) __attribute__((weak));
#define MOD_DEFAULT ((UINT64_C(0xdcba9876543210)<<5)|1u|(2u<<3))
#define TAIL_SHIFT 61
#define TAIL_MASK (UINT64_C(7)<<TAIL_SHIFT)
static _Atomic uint64_t mod_settings=MOD_DEFAULT,mod_generation;
static _Atomic unsigned relation_lighting=5; /* Pad Link Selected (bits 0-1) + Mute Group Selected (bits 2-3). */
int music_pad_link_lighting(void){return (int)(atomic_load(&relation_lighting)&3);}
int music_mute_group_lighting(void){return (int)((atomic_load(&relation_lighting)>>2)&3);}
static _Atomic unsigned relation_colours=(12u)|(11u<<4); /* link Cyan, group Amber. */
static _Atomic unsigned extended_roles; /* 0 legacy, 1 explicit global palette assignments */
static _Atomic unsigned service_slots[MUSIC_SERVICE_COUNT]={8,9,10,11,12,13};
static _Atomic unsigned relation_slots[MUSIC_RELATION_COUNT]={12,11};
static _Atomic unsigned reserve_system; /* Off: every colour may be reused. */
static _Atomic unsigned led_vivid=1;
unsigned music_led_vivid(void){return atomic_load(&led_vivid);}
static _Atomic unsigned text_input=1; /* 1 Pads (default), 0 Encoder; card file .am-mod-text-input */
unsigned music_text_input(void){return atomic_load(&text_input);}
static _Atomic unsigned fx_view=1; /* 1 Illustrated (default), 0 Classic; card file .am-mod-fx-view */
unsigned music_fx_view(void){return atomic_load(&fx_view);}
/* FX mode: 0 Stock (mixer off, stock FX Select/Knob FX), 1 Am MOD (default). Saved on the
 * card (.am-mod-fx-mode); the value in force is latched at start-up, so a change
 * applies after a restart. */
static _Atomic unsigned rec_route[3]={0,0,1};
static _Atomic unsigned pad_colour_source,piano_roll_colour,hide_play_cursor;
unsigned music_hide_play_cursor(void){return atomic_load(&hide_play_cursor);}
unsigned music_piano_roll_colour(void){return atomic_load(&piano_roll_colour);}
static _Atomic unsigned fx_mode_saved=1,fx_mode_active=1;static _Atomic int fx_mode_latched;
static _Atomic unsigned usb_audio_saved;
unsigned music_usb_audio(void){return atomic_load(&usb_audio_saved);}
unsigned music_fx_mode(void){return atomic_load(&fx_mode_saved);}
unsigned music_fx_mode_active(void){return atomic_load(&fx_mode_active);}
unsigned music_relation_colour(unsigned role){return role<MUSIC_RELATION_COUNT?(atomic_load(&extended_roles)?atomic_load(&relation_slots[role]):(atomic_load(&relation_colours)>>(4*role))&15u):0;}
uint32_t music_relation_rgb(unsigned role){
 unsigned slot=music_relation_colour(role);
 /* A deleted custom colour falls back to the role's recommended slot. */
 if(!music_palette_present(slot))slot=role==MUSIC_RELATION_LINK?12:11;
 return music_palette_rgb(slot);
}
uint32_t music_led_correct(uint32_t rgb){
 unsigned level=atomic_load(&led_vivid);if(!level||!music_pad_lighting_mode())return rgb;
 unsigned c[3]={(rgb>>16)&255,(rgb>>8)&255,rgb&255},hi=c[0],lo=c[0];
 for(int i=1;i<3;++i){if(c[i]>hi)hi=c[i];if(c[i]<lo)lo=c[i];}
 if(!hi||hi==lo)return rgb;
 /* Saturation-weighted: neutral white and grey stay as they are. */
 unsigned strength=level==1?50:85;
 unsigned remove=lo*strength*(hi-lo)/(100*hi);
 for(int i=0;i<3;++i)c[i]=c[i]>remove?c[i]-remove:0;
 return (rgb&0xff000000u)|(c[0]<<16)|(c[1]<<8)|c[2];
}
static int lighting_directory=-1;
static unsigned lighting_write_serial;
static const uint32_t mod_palette_rgb[MUSIC_PALETTE_SIZE]={0x0040ff,0x8800ff,0xff00cc,0xff6600,0xff2050,0x99ff00,0x00ff99,0xffff00,0xffffff,0x00ff00,0xff0000,0xffb000,0x00ffff,0x707070};
/* Bit 24 distinguishes a saved colour, including black, from an empty slot. */
static _Atomic uint32_t custom_palette[MUSIC_PALETTE_SLOTS];
static char custom_names[MUSIC_PALETTE_SLOTS][MUSIC_COLOUR_NAME_BYTES];
static const char*const palette_names[MUSIC_PALETTE_SIZE]={"Blue","Violet","Magenta","Orange","Coral","Lime","Teal","Yellow","White","Green","Red","Amber","Cyan","Grey"};
uint64_t music_mod_settings_snapshot(void){return atomic_load(&mod_settings);}
static int palette_valid(uint64_t v,unsigned count){
 if(v>>(5+4*count))return 0;
 unsigned seen=0;
 for(unsigned i=0;i<count;++i){unsigned c=(unsigned)((v>>(5+4*i))&15);if(c>=count||(seen&(1u<<c)))return 0;seen|=1u<<c;}
 return 1;
}
int music_mod_settings_valid(uint64_t v){return palette_valid(v&~TAIL_MASK,MUSIC_PALETTE_SIZE)||palette_valid(v,11);}
static uint64_t palette_upgrade(uint64_t v){
 if(palette_valid(v&~TAIL_MASK,MUSIC_PALETTE_SIZE))return v;
 /* Original IDs retain their owners and role swaps. The three new colours
  * occupy the extra user slots; no older service colour becomes user-owned. */
 static const unsigned colour[11]={0,1,2,3,4,8,9,10,11,12,13};
 uint64_t out=v&31;
 for(unsigned slot=0;slot<MUSIC_PALETTE_SIZE;++slot){unsigned c;
  if(slot<5)c=colour[(v>>(5+4*slot))&15];
  else if(slot<MUSIC_USER_COLOURS)c=slot;
  else c=colour[(v>>(5+4*(slot-3)))&15];
  out|=(uint64_t)c<<(5+4*slot);
 }
 return out;
}
int music_mod_settings_restore(uint64_t v){
 if(!music_mod_settings_valid(v))return 0;
 v=palette_upgrade(v);
 atomic_store(&mod_settings,v);atomic_fetch_add(&mod_generation,1);return 1;
}
int music_pad_lighting_mode(void){return (int)(atomic_load(&mod_settings)&1);}
unsigned music_tail_ms(unsigned choice){
 /* Last choice (formerly 4000 ms) = Off: Stop+pad then stays stock. */
 static const unsigned ms[MUSIC_TAIL_CHOICES]={100,250,500,750,1000,1500,2000,0};
 return ms[choice<MUSIC_TAIL_CHOICES?choice:4];
}
int music_mod_setting(unsigned field){uint64_t v=atomic_load(&mod_settings);return field<3?(int)((v>>field)&1):field==MUSIC_SETTING_BLINK?(int)((v>>3)&3):field==MUSIC_SETTING_TAIL?(int)(v>>TAIL_SHIFT)^4:0;}
static unsigned palette_slot(uint64_t v,unsigned slot){return (unsigned)((v>>(5+4*slot))&15);}
uint32_t music_palette_default_rgb(unsigned c){return c<MUSIC_PALETTE_SIZE?mod_palette_rgb[c]:0;}
int music_palette_present(unsigned c){return c<MUSIC_PALETTE_SIZE||(c<MUSIC_PALETTE_SLOTS&&atomic_load(&custom_palette[c]));}
uint32_t music_palette_rgb(unsigned c){
 if(c>=MUSIC_PALETTE_SLOTS)return 0;
 uint32_t custom=atomic_load(&custom_palette[c]);
 return custom?custom&0xffffffu:music_palette_default_rgb(c);
}
const char*music_palette_name(unsigned c){
 if(c<MUSIC_PALETTE_SLOTS&&custom_names[c][0])return custom_names[c];
 if(c<MUSIC_PALETTE_SIZE)return palette_names[c];
 static const char *const extra[]={"Custom 1","Custom 2","Custom 3","Custom 4","Custom 5","Custom 6","Custom 7","Custom 8","Custom 9","Custom 10","Custom 11","Custom 12","Custom 13","Custom 14","Custom 15","Custom 16","Custom 17","Custom 18"};
 return c<MUSIC_PALETTE_SLOTS?extra[c-MUSIC_PALETTE_SIZE]:"Default";
}
unsigned music_service_colour(unsigned role){role=role<MUSIC_SERVICE_COUNT?role:0;return atomic_load(&extended_roles)?atomic_load(&service_slots[role]):palette_slot(atomic_load(&mod_settings),MUSIC_USER_COLOURS+role);}
static unsigned service_display_slot(unsigned role){
 unsigned slot=music_service_colour(role);
 return music_palette_present(slot)?slot:8+role;
}
int music_service_colours_fallback(void){
 if(!music_pad_lighting_mode())return 0;
 for(unsigned role=0;role<MUSIC_SERVICE_COUNT;++role)
  if(!music_palette_present(music_service_colour(role)))return 1;
 return 0;
}
uint32_t music_service_rgb(unsigned role){
 role=role<MUSIC_SERVICE_COUNT?role:0;
 return music_pad_lighting_mode()?music_palette_rgb(service_display_slot(role)):mod_palette_rgb[8+role];
}
unsigned music_palette_reserved(void){return atomic_load(&reserve_system);}
int music_palette_available(unsigned slot){
 if(!music_palette_present(slot))return 0;
 if(music_palette_reserved())for(unsigned r=0;r<MUSIC_SERVICE_COUNT;++r)
  if(slot==service_display_slot(r))return 0;
 return 1;
}
unsigned music_palette_choice_count(void){unsigned n=0;for(unsigned s=0;s<MUSIC_PALETTE_SLOTS;++s)n+=music_palette_available(s);return n;}
unsigned music_palette_choice(unsigned index){for(unsigned s=0;s<MUSIC_PALETTE_SLOTS;++s)if(music_palette_available(s)&&!index--)return s;return 0;}
unsigned music_palette_choice_index(unsigned slot){unsigned n=0;for(unsigned s=0;s<MUSIC_PALETTE_SLOTS;++s)if(music_palette_available(s)){if(s==slot)return n;++n;}return 0;}
unsigned music_identity_count(void){return 1+music_palette_choice_count();}
unsigned music_identity_choice(unsigned index){return index?16+music_palette_choice(index-1):0;}
static unsigned identity_slot(unsigned raw){
 if(raw>=16&&raw<16+MUSIC_PALETTE_SLOTS)return raw-16;
 if(raw>=14&&raw<16)return raw;
 unsigned index=music_colour_choice_index(raw);
 return palette_slot(atomic_load(&mod_settings),index?index-1:0);
}
unsigned music_identity_index(unsigned raw){return raw?1+music_palette_choice_index(identity_slot(raw)):0;}
uint32_t music_user_rgb(unsigned raw){
 unsigned slot=identity_slot(raw);
 if(!music_palette_available(slot))slot=music_palette_choice(0);
 return music_palette_rgb(slot);
}
const char*music_identity_name(unsigned raw){return raw?music_palette_name(identity_slot(raw)):"Default";}
unsigned music_light_colour_count(void){return 1+music_palette_choice_count();}
unsigned music_light_colour_choice(unsigned index){return index?12+music_palette_choice(index-1):11;}
static unsigned legacy_light_slot(unsigned raw){static const unsigned slots[12]={9,12,0,1,2,10,3,11,7,8,13,0};return slots[raw<12?raw:0];}
unsigned music_light_colour_index(unsigned raw){return raw==11?0:1+music_palette_choice_index(raw>=12?raw-12:legacy_light_slot(raw));}
const char *music_light_colour_name(unsigned raw){return raw==11?"Off":music_palette_name(raw<12?legacy_light_slot(raw):raw-12);}
uint32_t music_light_colour_rgb(unsigned raw){
 if(raw==11)return 0;
 unsigned slot=raw<12?legacy_light_slot(raw):raw-12;if(!music_palette_available(slot))slot=music_palette_choice(0);
 return music_palette_rgb(slot);
}
int music_cursor_on(uint64_t phase,unsigned duty){
 unsigned mode=(unsigned)music_mod_setting(MUSIC_SETTING_BLINK);if(!mode)return 1;
 unsigned period=mode==1?1200:mode==2?800:400;
 return phase%period<(uint64_t)period*(duty>1000?1000:duty)/1000;
}
int music_lighting_preferences_durable(void){return lighting_directory>=0;}
#define PALETTE_FILE ".am-mod-palette"
#define PALETTE_V1_BYTES (4+4*MUSIC_PALETTE_SIZE)
#define PALETTE_RECORD_BYTES (4+MUSIC_COLOUR_NAME_BYTES)
#define PALETTE_BYTES (4+PALETTE_RECORD_BYTES*MUSIC_PALETTE_SLOTS)
static void load_palette(int dir){
 unsigned char data[PALETTE_BYTES+1];
 uint32_t colours[MUSIC_PALETTE_SLOTS]={0};
 memset(custom_names,0,sizeof(custom_names));
 int fd=openat(dir,PALETTE_FILE,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
 if(fd>=0){
  struct stat st;
  ssize_t n=fstat(fd,&st)||!S_ISREG(st.st_mode)?-1:read(fd,data,sizeof(data));
  close(fd);
  int legacy=n==PALETTE_V1_BYTES&&!memcmp(data,"AMP1",4);
  int v2=n==4+PALETTE_RECORD_BYTES*16&&!memcmp(data,"AMP2",4);
  if(legacy||v2||(n==PALETTE_BYTES&&!memcmp(data,"AMP3",4))){
   unsigned count=legacy?MUSIC_PALETTE_SIZE:v2?16:MUSIC_PALETTE_SLOTS;
   unsigned record=legacy?4:PALETTE_RECORD_BYTES;
   for(unsigned i=0;i<count;++i){
    const unsigned char*p=data+4+record*i;
    if(p[0]>1||(!p[0]&&(p[1]||p[2]||p[3]))||
       (!legacy&&!memchr(p+4,0,MUSIC_COLOUR_NAME_BYTES))){
     memset(colours,0,sizeof(colours));memset(custom_names,0,sizeof(custom_names));break;
    }
    colours[i]=(uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];
    if(!legacy&&p[0])memcpy(custom_names[i],p+4,MUSIC_COLOUR_NAME_BYTES);
   }
  }
 }
 for(unsigned i=0;i<MUSIC_PALETTE_SLOTS;++i)atomic_store(&custom_palette[i],colours[i]);
}
int music_lighting_preferences_open(const char *directory){
 int dir=open(directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(dir<0)return 0;
 uint64_t value=MOD_DEFAULT;unsigned relations=5,rel_colours=12u|(11u<<4),vivid=1;
 int fd=openat(dir,".am-mod-settings",O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
 int legacy=fd<0&&errno==ENOENT;
 if(legacy)fd=openat(dir,".am-mod-pad-lighting",O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
 if(fd>=0){char text[65]={0};struct stat st;ssize_t n=read(fd,text,sizeof(text)-1);
  if(!fstat(fd,&st)&&S_ISREG(st.st_mode)){
   if(legacy&&n==18&&!memcmp(text,"AM_PAD_LIGHTING=",16)&&text[17]=='\n'&&(text[16]=='0'||text[16]=='1'))value=(value&~UINT64_C(1))|(unsigned)(text[16]-'0');
   if(!legacy&&((n==27&&!memcmp(text,"AM_SETTINGS1 ",13)&&text[26]=='\n')||
                 (n==30&&(!memcmp(text,"AM_SETTINGS2 ",13)||!memcmp(text,"AM_SETTINGS3 ",13))&&text[29]=='\n')||
                 (n==32&&!memcmp(text,"AM_SETTINGS4 ",13)&&text[29]==' '&&text[30]>='0'&&text[30]<='7'&&text[31]=='\n')||
                 /* AM_SETTINGS5 <16 hex> <relations> <link><group> <vivid> */
                 (n==37&&!memcmp(text,"AM_SETTINGS5 ",13)&&text[29]==' '&&isxdigit((unsigned char)text[30])&&text[31]==' '&&
                  isxdigit((unsigned char)text[32])&&isxdigit((unsigned char)text[33])&&text[34]==' '&&text[35]>='0'&&text[35]<='2'&&text[36]=='\n'))){
    uint64_t decoded=0;int valid=1;
    for(int i=13;i<(n>=32?29:n-1);++i){unsigned c=(unsigned char)text[i];if(c>='0'&&c<='9')c-='0';else if(c>='a'&&c<='f')c=c-'a'+10;else {valid=0;break;}decoded=(decoded<<4)|c;}
    if(valid&&(!(decoded&TAIL_MASK)||text[11]=='3'||text[11]=='4'||text[11]=='5')&&music_mod_settings_valid(decoded)){
     value=decoded;if(n>=32){unsigned r=(unsigned char)text[30];relations=r<='9'?r-'0':(r|32)-'a'+10;}
     if(n==37){
      unsigned a=(unsigned char)text[32],b=(unsigned char)text[33];
      a=a<='9'?a-'0':(a|32)-'a'+10;b=b<='9'?b-'0':(b|32)-'a'+10;
      rel_colours=b|(a<<4);vivid=(unsigned)(text[35]-'0');
     }
    }
   }
  }
  close(fd);
 }
 unsigned view=1;
 {int vf=openat(dir,".am-mod-fx-view",O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
  if(vf>=0){char t[16]={0};struct stat vs;ssize_t vn=read(vf,t,sizeof(t)-1);
   if(!fstat(vf,&vs)&&S_ISREG(vs.st_mode)&&vn==12&&!memcmp(t,"AM_FXVIEW ",10)&&(t[10]=='0'||t[10]=='1')&&t[11]=='\n')view=(unsigned)(t[10]-'0');
   close(vf);}}
 atomic_store(&fx_view,view);
 {unsigned mode=0;int uf=openat(dir,".am-mod-usb-audio",O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
  if(uf>=0){char t[20]={0};struct stat us;ssize_t un=read(uf,t,sizeof(t)-1);
   if(!fstat(uf,&us)&&S_ISREG(us.st_mode)&&un==14&&!memcmp(t,"AM_USBAUDIO ",12)&&(t[12]=='0'||t[12]=='1')&&t[13]=='\n')mode=(unsigned)(t[12]-'0');
   close(uf);}
  atomic_store(&usb_audio_saved,mode);}
 {unsigned mode=1;int mf=openat(dir,".am-mod-fx-mode",O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
  if(mf>=0){char t[16]={0};struct stat ms;ssize_t mn=read(mf,t,sizeof(t)-1);
   if(!fstat(mf,&ms)&&S_ISREG(ms.st_mode)&&mn==12&&!memcmp(t,"AM_FXMODE ",10)&&(t[10]=='0'||t[10]=='1')&&t[11]=='\n')mode=(unsigned)(t[10]-'0');
   close(mf);}
  atomic_store(&fx_mode_saved,mode);
  if(!atomic_exchange(&fx_mode_latched,1))atomic_store(&fx_mode_active,mode);}
 {unsigned rp=0,rt=0,rw=1;int rf=openat(dir,".am-mod-rec-route",O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
  if(rf>=0){char t[24]={0};struct stat rs;ssize_t rn=read(rf,t,sizeof(t)-1);
   if(!fstat(rf,&rs)&&S_ISREG(rs.st_mode)&&rn==18&&!memcmp(t,"AM_RECROUTE ",12)&&t[12]>='0'&&t[12]<='2'&&t[13]==' '&&
      t[14]>='0'&&t[14]<='7'&&t[15]==' '&&(t[16]=='0'||t[16]=='1')&&t[17]=='\n'){rp=(unsigned)(t[12]-'0');rt=(unsigned)(t[14]-'0');rw=(unsigned)(t[16]-'0');}
   close(rf);}
  atomic_store(&rec_route[0],rp);atomic_store(&rec_route[1],rt);atomic_store(&rec_route[2],rw);}
 {unsigned src=0;int pf=openat(dir,".am-mod-pad-colour",O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
  if(pf>=0){char t[16]={0};struct stat ps;ssize_t pn=read(pf,t,sizeof(t)-1);
   if(!fstat(pf,&ps)&&S_ISREG(ps.st_mode)&&pn==12&&!memcmp(t,"AM_PADSRC ",10)&&(t[10]=='0'||t[10]=='1')&&t[11]=='\n')src=(unsigned)(t[10]-'0');
   close(pf);}
  atomic_store(&pad_colour_source,src);}
 {unsigned pads=1;int tf=openat(dir,".am-mod-text-input",O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
  if(tf>=0){char t[16]={0};struct stat ts;ssize_t tn=read(tf,t,sizeof(t)-1);
   if(!fstat(tf,&ts)&&S_ISREG(ts.st_mode)&&tn==12&&!memcmp(t,"AM_TEXTIN ",10)&&(t[10]=='0'||t[10]=='1')&&t[11]=='\n')pads=(unsigned)(t[10]-'0');
   close(tf);}
  atomic_store(&text_input,pads);}
 if(lighting_directory>=0)close(lighting_directory);
 lighting_directory=dir;
 {unsigned value=0;char text[13]={0};int pf=openat(dir,".am-mod-piano-roll",O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
  if(pf>=0){ssize_t n=read(pf,text,sizeof(text));close(pf);if(n==10&&!memcmp(text,"AM_ROLL ",8)&&text[8]>='0'&&text[8]<='1'&&text[9]=='\n')value=(unsigned)(text[8]-'0');}
  atomic_store(&piano_roll_colour,value);}
 {unsigned value=0;char text[17]={0};int pf=openat(dir,".am-mod-play-cursor",O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
  if(pf>=0){struct stat st;ssize_t n=read(pf,text,sizeof(text));
   if(!fstat(pf,&st)&&S_ISREG(st.st_mode)&&n==16&&!memcmp(text,"AM_PLAYCURSOR ",14)&&text[14]>='0'&&text[14]<='1'&&text[15]=='\n')value=(unsigned)(text[14]-'0');
   close(pf);}
  atomic_store(&hide_play_cursor,value);}
 load_palette(dir);
 atomic_store(&extended_roles,0);atomic_store(&reserve_system,0);
 unsigned char roles[14]={0};int rf=openat(dir,".am-mod-colour-roles",O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
 if(rf>=0){ssize_t rn=read(rf,roles,sizeof(roles));close(rf);
  int valid=rn==13&&!memcmp(roles,"AMR1",4)&&roles[4]<=1;
  for(unsigned i=5;i<13;++i)if(roles[i]>=MUSIC_PALETTE_SLOTS)valid=0;
  if(valid){for(unsigned i=0;i<MUSIC_SERVICE_COUNT;++i)atomic_store(&service_slots[i],roles[5+i]);
   for(unsigned i=0;i<MUSIC_RELATION_COUNT;++i)atomic_store(&relation_slots[i],roles[11+i]);
   atomic_store(&reserve_system,roles[4]);atomic_store(&extended_roles,1);}
 }
 atomic_store(&relation_lighting,relations);
 atomic_store(&relation_colours,rel_colours);atomic_store(&led_vivid,vivid);
 music_mod_settings_restore(value);return 1;
}
static void global_load(void);
void music_lighting_preferences_init(void){
 int fd=open("/run/mpc-trial-app/storage-meta/save-path",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return;
 char path[4096]={0};ssize_t n=read(fd,path,sizeof(path)-16);close(fd);
 if(n<=0)return;
 while(n&&path[n-1]=='\n')path[--n]=0;
 if(strncmp(path,"/media/",7)||strstr(path,"/../")||n+10>=(ssize_t)sizeof(path))return;
 memcpy(path+n,"/Projects",10);music_lighting_preferences_open(path);
 global_load();
}
static int save_preference(const char*name,const void*data,size_t size){
 if(lighting_directory<0)return 1;
 char temp[80];int fd=-1;
 for(unsigned attempt=0;attempt<16&&fd<0;++attempt){
  snprintf(temp,sizeof(temp),".am-mod-settings.tmp-%ld-%u",(long)getpid(),++lighting_write_serial);
  fd=openat(lighting_directory,temp,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
  if(fd<0&&errno!=EEXIST)return 0;
 }
 if(fd<0)return 0;
 int ok=write(fd,data,size)==(ssize_t)size&&!fsync(fd);
 if(close(fd))ok=0;
 if(ok)ok=!renameat(lighting_directory,temp,lighting_directory,name);
 if(!ok){unlinkat(lighting_directory,temp,0);return 0;}
 fsync(lighting_directory);
 return 1;
}
/* Card-wide defaults kept next to .am-mod-settings in the shared Projects folder: the MIDI map (maps and
 * buttons) and the Synth parameters. A project that carries its own MIDI map keeps it; every other project
 * starts from these, and editing either one updates them. */
#ifndef AM_MIDI_PAYLOAD
#define AM_MIDI_PAYLOAD 4736u
#endif
#define SYNTH_GLOBAL_PARAMS 22u
#define SYNTH_GLOBAL_TEXT 200u
static char synth_last[SYNTH_GLOBAL_TEXT];
#ifdef MPC_MIDI_TOOLS
static char midi_last[AM_MIDI_PAYLOAD],midi_global[AM_MIDI_PAYLOAD];
#endif
static int global_read(const char*name,char*buf,size_t cap){
 if(lighting_directory<0)return 0;
 int fd=openat(lighting_directory,name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return 0;
 struct stat st;ssize_t n=-1;
 if(!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_size>0&&(size_t)st.st_size<cap)n=read(fd,buf,cap-1);
 close(fd);if(n<=0)return 0;
 buf[n]=0;return 1;
}
static void synth_text(char*out,size_t cap){
 size_t at=(size_t)snprintf(out,cap,"AMSY1");
 for(unsigned i=0;i<SYNTH_GLOBAL_PARAMS&&at<cap;++i)at+=(size_t)snprintf(out+at,cap-at," %u",am_synth_param_get(i));
}
static void global_load(void){
 char text[AM_MIDI_PAYLOAD>SYNTH_GLOBAL_TEXT?AM_MIDI_PAYLOAD:SYNTH_GLOBAL_TEXT];
#ifdef MPC_MIDI_TOOLS
 if(global_read(".am-mod-midi",text,AM_MIDI_PAYLOAD)){
  if(am_midi_restore(text)){memcpy(midi_global,text,strlen(text)+1);memcpy(midi_last,text,strlen(text)+1);}
  else am_midi_restore(NULL);
 }else{am_midi_encode(midi_last,sizeof midi_last);}
#endif
 if(am_synth_param_set&&am_synth_param_get){
  if(global_read(".am-mod-synth",text,SYNTH_GLOBAL_TEXT)&&!strncmp(text,"AMSY1",5)){
   const char*p=text+5;
   for(unsigned i=0;i<SYNTH_GLOBAL_PARAMS;++i){char*end;unsigned long v=strtoul(p,&end,10);if(end==p)break;am_synth_param_set(i,(unsigned)v);p=end;}
  }
  synth_text(synth_last,sizeof synth_last);
 }
}
const char*music_global_midi_text(void){
#ifdef MPC_MIDI_TOOLS
 return midi_global[0]?midi_global:NULL;
#else
 return NULL;
#endif
}
void music_global_midi_baseline(void){
#ifdef MPC_MIDI_TOOLS
 am_midi_encode(midi_last,sizeof midi_last);
#endif
}
/* UI thread: write the card defaults when they changed (at most every 1.5 s unless forced). */
void music_global_sync(int force){
 static unsigned long long last_ms;struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
 unsigned long long now=(unsigned long long)t.tv_sec*1000ull+(unsigned long long)t.tv_nsec/1000000ull;
 if(!force&&now-last_ms<1500ull)return;
 last_ms=now;
#ifdef MPC_MIDI_TOOLS
 {static char text[AM_MIDI_PAYLOAD];
  if(am_midi_encode(text,sizeof text)>0&&strcmp(text,midi_last)&&music_lighting_preferences_durable()
     &&save_preference(".am-mod-midi",text,strlen(text))){memcpy(midi_last,text,strlen(text)+1);memcpy(midi_global,text,strlen(text)+1);}}
#endif
 if(am_synth_param_get&&music_lighting_preferences_durable()){
  char text[SYNTH_GLOBAL_TEXT];synth_text(text,sizeof text);
  if(strcmp(text,synth_last)&&save_preference(".am-mod-synth",text,strlen(text)))memcpy(synth_last,text,strlen(text)+1);
 }
}
int music_palette_apply(const struct music_palette_edit edits[MUSIC_PALETTE_SLOTS],unsigned mask){
 if(!mask)return 1;
 if(!music_lighting_preferences_durable())return 0;
 unsigned char data[PALETTE_BYTES]={'A','M','P','3'};
 uint32_t values[MUSIC_PALETTE_SLOTS];
 for(unsigned i=0;i<MUSIC_PALETTE_SLOTS;++i){
  const char *name=custom_names[i];
  values[i]=atomic_load(&custom_palette[i]);
  if(mask&(1u<<i)){
   if(edits[i].rgb>0xffffffu||!memchr(edits[i].name,0,sizeof(edits[i].name)))return 0;
   values[i]=edits[i].reset?0:0x1000000u|edits[i].rgb;
   name=edits[i].reset?"":edits[i].name;
  }
  unsigned char *record=data+4+PALETTE_RECORD_BYTES*i;
  for(unsigned b=0;b<4;++b)record[b]=(unsigned char)(values[i]>>(24-8*b));
  memcpy(record+4,name,strlen(name)+1);
 }
 if(!save_preference(PALETTE_FILE,data,sizeof(data)))return 0;
 for(unsigned i=0;i<MUSIC_PALETTE_SLOTS;++i)if(mask&(1u<<i)){
  strcpy(custom_names[i],edits[i].reset?"":edits[i].name);
  atomic_store(&custom_palette[i],values[i]);
 }
 atomic_fetch_add(&mod_generation,1);
 return 1;
}
static int save_palette_entry(unsigned colour,uint32_t rgb,const char *name,int reset){
 if(colour>=MUSIC_PALETTE_SLOTS||strlen(name)>=MUSIC_COLOUR_NAME_BYTES||rgb>0xffffffu)return 0;
 uint32_t value=reset?0:0x1000000u|rgb;
 if(atomic_load(&custom_palette[colour])==value&&!strcmp(custom_names[colour],name))return 1;
 struct music_palette_edit edits[MUSIC_PALETTE_SLOTS]={0};
 edits[colour].rgb=rgb;edits[colour].reset=reset;
 strcpy(edits[colour].name,name);
 return music_palette_apply(edits,1u<<colour);
}
int music_palette_save(unsigned colour,uint32_t rgb,const char *name){
 return save_palette_entry(colour,rgb,name,0);
}
int music_palette_reset(unsigned colour){return save_palette_entry(colour,0,"",1);}
int music_palette_set_rgb(unsigned colour,uint32_t rgb){
 if(colour>=MUSIC_PALETTE_SLOTS)return 0;
 if(rgb==music_palette_default_rgb(colour))return music_palette_reset(colour);
 return music_palette_save(colour,rgb,custom_names[colour]);
}
static int save_card_settings(uint64_t value,unsigned relations,unsigned rel_colours,unsigned vivid){
 if(!music_mod_settings_valid(value)||relations>15||rel_colours>255||vivid>2)return 0;
 char text[38];snprintf(text,sizeof(text),"AM_SETTINGS5 %016llx %x %02x %u\n",(unsigned long long)value,relations,rel_colours,vivid);
 if(!save_preference(".am-mod-settings",text,sizeof(text)-1))return 0;
 atomic_store(&relation_lighting,relations);atomic_store(&relation_colours,rel_colours);atomic_store(&led_vivid,vivid);
 return music_mod_settings_restore(value);
}
static int save_all_settings(uint64_t value,unsigned relations){
 return save_card_settings(value,relations,atomic_load(&relation_colours),atomic_load(&led_vivid));
}
static int save_global_roles(unsigned role,unsigned slot,int reserved){
 unsigned char data[13]={'A','M','R','1'};
 data[4]=(unsigned char)reserved;
 for(unsigned i=0;i<MUSIC_SERVICE_COUNT;++i){
  unsigned value=music_service_colour(i);
  if(role==100)value=8+i;
  else if(role<MUSIC_SERVICE_COUNT){if(i==role)value=slot;else if(value==slot)value=music_service_colour(role);}
  data[5+i]=(unsigned char)value;
 }
 for(unsigned i=0;i<MUSIC_RELATION_COUNT;++i)data[11+i]=(unsigned char)(role==100?(i?11:12):role==6+i?slot:music_relation_colour(i));
 if(!save_preference(".am-mod-colour-roles",data,sizeof(data)))return 0;
 for(unsigned i=0;i<MUSIC_SERVICE_COUNT;++i)atomic_store(&service_slots[i],data[5+i]);
 for(unsigned i=0;i<MUSIC_RELATION_COUNT;++i)atomic_store(&relation_slots[i],data[11+i]);
 atomic_store(&reserve_system,reserved);atomic_store(&extended_roles,1);atomic_fetch_add(&mod_generation,1);return 1;
}
int music_set_palette_reserved(unsigned enabled){return enabled<=1&&save_global_roles(99,0,(int)enabled);}
int music_set_relation_colour(unsigned role,unsigned slot){
 if(role>=MUSIC_RELATION_COUNT||slot>=MUSIC_PALETTE_SLOTS||!music_palette_available(slot))return 0;
 return save_global_roles(6+role,slot,(int)music_palette_reserved());
}
int music_set_text_input(unsigned pads){
 if(pads>1)return 0;
 char text[13];snprintf(text,sizeof(text),"AM_TEXTIN %u\n",pads);
 if(!save_preference(".am-mod-text-input",text,12))return 0;
 atomic_store(&text_input,pads);return 1;
}
int music_set_fx_view(unsigned view){
 if(view>1)return 0;
 char text[13];snprintf(text,sizeof(text),"AM_FXVIEW %u\n",view);
 if(!save_preference(".am-mod-fx-view",text,12))return 0;
 atomic_store(&fx_view,view);return 1;
}
int music_pad_colour_locked(void){return atomic_load(&pad_colour_source)&&music_pad_lighting_mode()&&music_fx_mode_active()==1;}
unsigned music_pad_colour_source(void){return atomic_load(&pad_colour_source);}
int music_set_hide_play_cursor(unsigned value){
 if(value>1)return 0;
 char text[17];snprintf(text,sizeof(text),"AM_PLAYCURSOR %u\n",value);
 if(!save_preference(".am-mod-play-cursor",text,16))return 0;
 atomic_store(&hide_play_cursor,value);return 1;
}
int music_set_piano_roll_colour(unsigned value){
 if(value>1)return 0;
 char text[11];snprintf(text,sizeof(text),"AM_ROLL %u\n",value);
 if(!save_preference(".am-mod-piano-roll",text,10))return 0;
 atomic_store(&piano_roll_colour,value);return 1;
}
int music_set_pad_colour_source(unsigned value){
 if(value>1)return 0;
 char text[13];snprintf(text,sizeof(text),"AM_PADSRC %u\n",value);
 if(!save_preference(".am-mod-pad-colour",text,12))return 0;
 atomic_store(&pad_colour_source,value);return 1;
}
unsigned music_rec_get(unsigned field){return field<3?atomic_load(&rec_route[field]):0;}
int music_rec_set(unsigned field,unsigned value){
 if(field>2||value>(field==0?2u:field==1?7u:1u))return 0;
 unsigned v[3]={atomic_load(&rec_route[0]),atomic_load(&rec_route[1]),atomic_load(&rec_route[2])};
 v[field]=value;
 char text[24];snprintf(text,sizeof(text),"AM_RECROUTE %u %u %u\n",v[0],v[1],v[2]);
 if(!save_preference(".am-mod-rec-route",text,18))return 0;
 atomic_store(&rec_route[field],value);return 1;
}
int music_set_fx_mode(unsigned mode){
 if(mode>1)return 0;
 char text[13];snprintf(text,sizeof(text),"AM_FXMODE %u\n",mode);
 if(!save_preference(".am-mod-fx-mode",text,12))return 0;
 atomic_store(&fx_mode_saved,mode);return 1;
}
int music_set_usb_audio(unsigned mode){
 if(mode>1)return 0;
 char text[15];snprintf(text,sizeof(text),"AM_USBAUDIO %u\n",mode);
 if(!save_preference(".am-mod-usb-audio",text,14))return 0;
 atomic_store(&usb_audio_saved,mode);return 1;
}
int music_set_led_vivid(unsigned level){
 return level<=2&&save_card_settings(atomic_load(&mod_settings),atomic_load(&relation_lighting),atomic_load(&relation_colours),level);
}
static int save_mod_settings(uint64_t value){
 return save_all_settings(value,atomic_load(&relation_lighting));
}
int music_set_pad_link_lighting(unsigned mode){
 return mode<=MUSIC_LINK_BOTH&&save_all_settings(atomic_load(&mod_settings),
     (atomic_load(&relation_lighting)&12u)|mode);
}
int music_set_mute_group_lighting(unsigned mode){
 /* Same four modes as Pad Link; legacy 0/1 read as Off/Selected. */
 return mode<=MUSIC_LINK_BOTH&&save_all_settings(atomic_load(&mod_settings),
     (atomic_load(&relation_lighting)&3u)|(mode<<2));
}
int music_set_mod_setting(unsigned field,unsigned value){
 if(field==MUSIC_SETTING_TAIL)return value<MUSIC_TAIL_CHOICES&&save_mod_settings((atomic_load(&mod_settings)&~TAIL_MASK)|((uint64_t)(value^4u)<<TAIL_SHIFT));
 if(field>MUSIC_SETTING_BLINK||value>(field==MUSIC_SETTING_BLINK?3u:1u))return 0;
 uint64_t mask=field==MUSIC_SETTING_BLINK?3:1,v=atomic_load(&mod_settings);
 return save_mod_settings((v&~(mask<<field))|((uint64_t)value<<field));
}
int music_set_pad_lighting_mode(unsigned mode){return music_set_mod_setting(MUSIC_SETTING_LIGHTING,mode);}
int music_set_service_colour(unsigned role,unsigned colour){
 if(role>=MUSIC_SERVICE_COUNT||!music_palette_present(colour))return 0;
 return save_global_roles(role,colour,(int)music_palette_reserved());
}
int music_mod_restore_recommended(unsigned section){
 uint64_t mask=section==0?1:section==1?6|TAIL_MASK:section==2?UINT64_C(0x1fffffffffffffe0):section==3?24:0;
 if(section>3)return 0;
 if(section==1&&!music_set_piano_roll_colour(0))return 0;
 if(section==3&&!music_set_hide_play_cursor(0))return 0;
 if(section==2&&!save_global_roles(100,0,0))return 0;
 return save_card_settings((atomic_load(&mod_settings)&~mask)|(MOD_DEFAULT&mask),
     section==0?5:atomic_load(&relation_lighting),
     section==2?(12u|(11u<<4)):atomic_load(&relation_colours),
     section==2?1:atomic_load(&led_vivid));
}

const char *const music_note_names[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
const char *const music_scale_names[8]={"Major","Natural Minor","Dorian","Phrygian","Lydian","Mixolydian","Locrian","Chromatic"};
const char *const music_colour_names[12]={"Green","Cyan","Blue","Violet","Magenta","Red","Orange","Amber","Yellow","White","Grey","Off"};
const uint32_t music_colours[12]={0x00e060,0x00c8ff,0x2030ff,0x8830ff,0xff20b0,0xff0000,0xff6000,0xffb830,0xffe000,0xffffff,0x505050,0};
static const uint16_t scale_masks[8]={0xab5,0x5ad,0x6ad,0x5ab,0xad5,0x6b5,0x56b,0xfff};
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static struct music_project project;
static _Atomic uint64_t generation=1, published;
static _Atomic int key_edited;
/* 0 is the stock colour (the native pad colour passes through); the old
 * Cyan moved to the end. */
const char *const music_sequence_colour_names[MUSIC_SEQUENCE_COLOURS]={"Default","Green","Amber","Violet","Magenta","Red","Orange","Yellow","Blue","Lime","Mint","White","Cyan","Pink"};
const uint32_t music_sequence_colours[MUSIC_SEQUENCE_COLOURS]={0x40ff40,0x00c700,0xffc247,0x8830ff,0xff20b0,0xff0000,0xff6000,0xffe000,0x2030ff,0x98ff00,0x00ffb0,0xffffff,0x00c8ff,0xff70b8};
int music_sequence_colour(unsigned sequence){
    if(sequence>=128)return 0;
    pthread_mutex_lock(&mutex);int v=project.sequence_colours[sequence];pthread_mutex_unlock(&mutex);return v;
}
int music_set_sequence_colour(unsigned sequence,unsigned colour){
    if(sequence>=128||colour>=MUSIC_SEQUENCE_COLOURS)return 0;
    pthread_mutex_lock(&mutex);project.sequence_colours[sequence]=(unsigned char)colour;pthread_mutex_unlock(&mutex);
    atomic_fetch_add(&generation,1);return 1;
}

int music_sequence_pad_muted(unsigned sequence,unsigned pad){
    if(sequence>=128||pad>=128)return 0;
    pthread_mutex_lock(&mutex);int v=(int)((project.sequence_mutes[sequence][pad/64]>>(pad%64))&1u);pthread_mutex_unlock(&mutex);return v;
}
int music_sequence_pad_toggle(unsigned sequence,unsigned pad){
    if(sequence>=128||pad>=128)return -1;
    pthread_mutex_lock(&mutex);project.sequence_mutes[sequence][pad/64]^=(uint64_t)1<<(pad%64);
    int v=(int)((project.sequence_mutes[sequence][pad/64]>>(pad%64))&1u);pthread_mutex_unlock(&mutex);
    atomic_fetch_add(&generation,1);return v;
}
int music_sequence_mutes_get(unsigned sequence,uint64_t out[2]){
    out[0]=out[1]=0;if(sequence>=128)return 0;
    pthread_mutex_lock(&mutex);out[0]=project.sequence_mutes[sequence][0];out[1]=project.sequence_mutes[sequence][1];pthread_mutex_unlock(&mutex);
    return (out[0]|out[1])!=0;
}
int music_sequence_mutes_clear(unsigned sequence){
    if(sequence>=128)return 0;
    pthread_mutex_lock(&mutex);project.sequence_mutes[sequence][0]=project.sequence_mutes[sequence][1]=0;pthread_mutex_unlock(&mutex);
    atomic_fetch_add(&generation,1);return 1;
}
int music_sequence_mutes_copy(unsigned from,unsigned to){
    if(from>=128||to>=128)return 0;
    pthread_mutex_lock(&mutex);project.sequence_mutes[to][0]=project.sequence_mutes[from][0];project.sequence_mutes[to][1]=project.sequence_mutes[from][1];pthread_mutex_unlock(&mutex);
    atomic_fetch_add(&generation,1);return 1;
}
int music_seqmutes_encode(const struct music_project *p,char *out,size_t size){
    size_t used=0;int n=snprintf(out,size,"AMSM1\n");if(n!=6||(size_t)n>=size)return 0;used=(size_t)n;unsigned any=0;
    for(unsigned i=0;i<128;++i){
        if(!(p->sequence_mutes[i][0]|p->sequence_mutes[i][1]))continue;
        n=snprintf(out+used,size-used,"%u %016llx%016llx\n",i,(unsigned long long)p->sequence_mutes[i][0],(unsigned long long)p->sequence_mutes[i][1]);
        if(n<0||(size_t)n>=size-used)return 0;
        used+=(size_t)n;any=1;
    }
    return any?(int)used:0;
}
int music_seqmutes_decode(const char *in,size_t size,struct music_project *out){
    if(size<7||memcmp(in,"AMSM1\n",6))return 0;
    uint64_t masks[128][2];memset(masks,0,sizeof(masks));
    const char *s=in+6,*end=in+size;int last=-1;
    while(s<end){
        int q=0,digits=0;while(s<end&&*s>='0'&&*s<='9'){if(++digits>3)return 0;q=q*10+(*s++-'0');}
        if(!digits||s==end||*s++!=' '||q<=last||q>=128||end-s<33)return 0;
        for(unsigned w=0;w<2;++w){uint64_t v=0;for(unsigned j=0;j<16;++j){char c=*s++;int d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;if(d<0)return 0;v=(v<<4)|(unsigned)d;}masks[q][w]=v;}
        if(*s++!='\n'||!(masks[q][0]|masks[q][1]))return 0;
        last=q;
    }
    memcpy(out->sequence_mutes,masks,sizeof(masks));return 1;
}

int music_pad_colour(unsigned pad){
    if(pad>=128)return 0;
    pthread_mutex_lock(&mutex);int v=project.pad_colours[pad];pthread_mutex_unlock(&mutex);return v;
}
int music_set_pad_colour(unsigned pad,unsigned colour){
    if(pad>=128||colour>=MUSIC_SEQUENCE_COLOURS)return 0;
    pthread_mutex_lock(&mutex);project.pad_colours[pad]=(unsigned char)colour;pthread_mutex_unlock(&mutex);
    atomic_fetch_add(&generation,1);return 1;
}
int music_pad_colours_used(void){
    int used=0;pthread_mutex_lock(&mutex);for(unsigned i=0;i<128&&!used;++i)used=project.pad_colours[i]!=0;pthread_mutex_unlock(&mutex);return used;
}

static void sample_default(struct music_sample *p,const char *name)
{
    memset(p,0,sizeof(*p)); p->root=-1; p->tonic=-1; p->scale=-1;
    p->colour[0]=MUSIC_DEFAULT_TONIC_COLOUR; p->colour[1]=1; p->colour[2]=10;
    p->original_colour=4; /* historical Lights ID, migrated only at selection time */
    p->fifth_colour=MUSIC_DEFAULT_FIFTH_COLOUR;p->fourth_colour=MUSIC_DEFAULT_FOURTH_COLOUR;
    if (name) memcpy(p->name,name,strlen(name)+1);
}
void music_defaults(struct music_project *p) { memset(p,0,sizeof(*p)); }
void music_reset(void)
{
#ifdef MPC_MIDI_TOOLS
    am_midi_reset();
#endif
    pthread_mutex_lock(&mutex); music_defaults(&project); pthread_mutex_unlock(&mutex);
#ifdef MPC_PAD_EQ
    uint64_t empty[128]={0};pad_eq_publish_all(empty);
#endif
    atomic_store(&published,0); atomic_store(&key_edited,0); atomic_fetch_add(&generation,1);
#ifdef MPC_COMPRESSOR
    compressor_project_reset();
#endif
}
uint64_t music_generation(void) { return atomic_load(&generation)+atomic_load(&mod_generation); }
void music_get_key(int *tonic,int *scale)
{
    pthread_mutex_lock(&mutex); *tonic=project.tonic; *scale=project.scale; pthread_mutex_unlock(&mutex);
}
void music_set_key(int tonic,int scale)
{
    if (tonic<0 || tonic>11 || scale<0 || scale>7) return;
    pthread_mutex_lock(&mutex); project.tonic=tonic; project.scale=scale; pthread_mutex_unlock(&mutex);
    atomic_store(&key_edited,1);
    atomic_fetch_add(&generation,1);
}
int music_get_sample(const char *name,struct music_sample *out)
{
    if (!name || !*name || strnlen(name,MUSIC_NAME_BYTES)>=MUSIC_NAME_BYTES) return 0;
    sample_default(out,name);
    int found=0;
    pthread_mutex_lock(&mutex);
    for (unsigned i=0;i<project.count;++i) if (!strcmp(project.samples[i].name,name)) {
        *out=project.samples[i]; found=1; break;
    }
    pthread_mutex_unlock(&mutex); return found;
}
int music_set_sample(const char *name,int field,int value)
{
    if (!name || !*name || strnlen(name,MUSIC_NAME_BYTES)>=MUSIC_NAME_BYTES) return 0;
    if ((field==MUSIC_ROOT && (value< -1 || value>11))
        || (field==MUSIC_MODE && (value<0 || value>4))
        || (field==MUSIC_TRANSPOSE && (value< -24 || value>24))
        || (field>=MUSIC_COLOUR0 && field<=MUSIC_COLOUR2 && (value<0 || value>=12+MUSIC_PALETTE_SLOTS))
        || (field==MUSIC_LOCAL_TONIC && (value< -1 || value>11))
        || (field==MUSIC_LOCAL_SCALE && (value< -1 || value>7))
        || (field==MUSIC_ORIGINAL_COLOUR && (value<0||value>=12+MUSIC_PALETTE_SLOTS))
        || ((field==MUSIC_FIFTH_COLOUR||field==MUSIC_FOURTH_COLOUR) && (value<0||value>=12+MUSIC_PALETTE_SLOTS))
        || field<0 || field>MUSIC_FOURTH_COLOUR) return 0;
    pthread_mutex_lock(&mutex);
    unsigned i;
    for (i=0;i<project.count;++i) if (!strcmp(project.samples[i].name,name)) break;
    if (i==project.count) {
        if (project.count==MUSIC_MAX_SAMPLES) { pthread_mutex_unlock(&mutex); return 0; }
        sample_default(&project.samples[project.count++],name);
    }
    struct music_sample *p=&project.samples[i];
    if (field==MUSIC_ROOT) {
        p->root=value;
        if (value>=0) p->mode=MUSIC_SCALE;
    }
    else if (field==MUSIC_MODE) p->mode=value;
    else if (field==MUSIC_TRANSPOSE) p->transpose=value;
    else if (field==MUSIC_LOCAL_TONIC) p->tonic=value;
    else if (field==MUSIC_LOCAL_SCALE) p->scale=value;
    else if (field==MUSIC_ORIGINAL_COLOUR) p->original_colour=value;
    else if (field==MUSIC_FIFTH_COLOUR) p->fifth_colour=value;
    else if (field==MUSIC_FOURTH_COLOUR) p->fourth_colour=value;
    else p->colour[field-MUSIC_COLOUR0]=value;
    pthread_mutex_unlock(&mutex); atomic_fetch_add(&generation,1); return 1;
}
void music_snapshot(struct music_project *out)
{ pthread_mutex_lock(&mutex); *out=project; pthread_mutex_unlock(&mutex); }
void music_replace(const struct music_project *p)
{
    pthread_mutex_lock(&mutex); project=*p; pthread_mutex_unlock(&mutex);
#ifdef MPC_PAD_EQ
    pad_eq_publish_all(p->pad_eq);
#endif
    atomic_store(&published,0); atomic_store(&key_edited,0); atomic_fetch_add(&generation,1);
}
uint64_t music_get_pad_eq(unsigned pad){
    if(pad>=128)return 0;
    pthread_mutex_lock(&mutex);uint64_t v=project.pad_eq[pad];pthread_mutex_unlock(&mutex);return v;
}
int music_set_pad_eq(unsigned pad,uint64_t v){
    if(pad>=128)return 0;
#ifdef MPC_PAD_EQ
    if(!pad_eq_valid(v))return 0;
#else
    if(v)return 0;
#endif
    pthread_mutex_lock(&mutex);project.pad_eq[pad]=v;pthread_mutex_unlock(&mutex);
#ifdef MPC_PAD_EQ
    pad_eq_publish(pad,v);
#endif
    atomic_fetch_add(&generation,1);return 1;
}
void music_key_text(int tonic,int scale,char *out,size_t size)
{ snprintf(out,size,"%s %s",music_note_names[tonic],music_scale_names[scale]); }
int music_key_edited(void) { return atomic_load(&key_edited); }
int music_key_from_text(const char *text,int *tonic,int *scale)
{
    *tonic=0; *scale=0; if (!text) return 0;
    for (int i=0;i<12;++i) for (int j=0;j<8;++j) {
        char key[48]; music_key_text(i,j,key,sizeof(key));
        if (!strcmp(key,text)) { *tonic=i; *scale=j; return 1; }
    }
    return 0;
}

/* Versioned ASCII payload carried in a named native JSON string field. Names
 * are hex UTF-8, so delimiters/newlines cannot alter the record structure. */
int music_encode(const struct music_project *p,char *out,size_t size)
{
    if (p->count>MUSIC_MAX_SAMPLES) return 0;
    unsigned extended=0;
    unsigned pads=0,fifth=0,wide=0;
    for(unsigned i=0;i<p->count;++i){int c=p->samples[i].fifth_colour,d=p->samples[i].fourth_colour;if(c<0||c>=12+MUSIC_PALETTE_SLOTS||d<0||d>=12+MUSIC_PALETTE_SLOTS)return 0;if(c!=MUSIC_DEFAULT_FIFTH_COLOUR||d!=MUSIC_DEFAULT_FOURTH_COLOUR)fifth=1;}
    for(unsigned i=0;i<128;++i){if(p->sequence_colours[i]>=MUSIC_SEQUENCE_COLOURS)return 0;if(p->sequence_colours[i]>=5)extended=1;}
    for(unsigned i=0;i<128;++i){if(p->pad_colours[i]>=MUSIC_SEQUENCE_COLOURS)return 0;if(p->pad_colours[i])pads=1;}
    /* Version 7 only when a pad colour is set and version 9 only when a
     * Degrees IV or V colour differs from its default (8 carried V only), so older builds keep reading
     * projects that do not use these features. Version 8 implies PAD128. */
    for(unsigned i=0;i<p->count;++i){const struct music_sample *q=p->samples+i;
     if(q->original_colour>=12||q->fifth_colour>=12||q->fourth_colour>=12)wide=1;
     for(unsigned j=0;j<3;++j)if(q->colour[j]>=12)wide=1;
    }
    for(unsigned i=0;i<128;++i)if(p->sequence_colours[i]>=16||p->pad_colours[i]>=16)wide=1;
    if(wide)fifth=1;
    if(fifth)pads=1;
    int n=wide?snprintf(out,size,"MPCS20 %d %d %u\n",p->tonic,p->scale,p->count):snprintf(out,size,"MPCS1%c %d %d %u\n",fifth?'9':pads?'7':extended?'6':'5',p->tonic,p->scale,p->count);
    if (n<0 || (size_t)n>=size) return 0;
    size_t used=(size_t)n; const char hex[]="0123456789abcdef";
    for (unsigned i=0;i<p->count;++i) {
        const struct music_sample *s=&p->samples[i];
        if(fifth)n=snprintf(out+used,size-used,"%d %d %d %d %d %d %d %d %d %d %d\n",s->root,s->mode,s->transpose,s->colour[0],s->colour[1],s->colour[2],s->tonic,s->scale,s->original_colour,s->fifth_colour,s->fourth_colour);
        else n=snprintf(out+used,size-used,"%d %d %d %d %d %d %d %d %d\n",s->root,s->mode,s->transpose,s->colour[0],s->colour[1],s->colour[2],s->tonic,s->scale,s->original_colour);
        if (n<0 || (size_t)n>=size-used) return 0;
        used+=(size_t)n;
        size_t len=strnlen(s->name,MUSIC_NAME_BYTES);
        if (!len || len>=MUSIC_NAME_BYTES || size-used<=len*2+1) return 0;
        for (size_t j=0;j<len;++j) { unsigned char c=(unsigned char)s->name[j]; out[used++]=hex[c>>4]; out[used++]=hex[c&15]; }
        out[used++]='\n'; out[used]=0;
    }
    n=snprintf(out+used,size-used,"EQ128\n");if(n<0||(size_t)n>=size-used)return 0;used+=(size_t)n;
    for(unsigned pad=0;pad<128;++pad){
        uint64_t v=p->pad_eq[pad];
#ifdef MPC_PAD_EQ
        if(!pad_eq_valid(v))return 0;
#endif
        n=snprintf(out+used,size-used,"%08llx\n",(unsigned long long)v);if(n!=9||(size_t)n>=size-used)return 0;used+=(size_t)n;
    }
    n=snprintf(out+used,size-used,"SEQ128\n");if(n!=7||(size_t)n>=size-used)return 0;used+=(size_t)n;
    if(size-used<=(wide?257u:129u))return 0;
    for(unsigned i=0;i<128;++i){if(wide)out[used++]=hex[p->sequence_colours[i]>>4];out[used++]=hex[p->sequence_colours[i]&15];}
    out[used++]='\n';out[used]=0;
    if(pads){
        n=snprintf(out+used,size-used,"PAD128\n");if(n!=7||(size_t)n>=size-used)return 0;used+=(size_t)n;
        if(size-used<=(wide?257u:129u))return 0;
        for(unsigned i=0;i<128;++i){if(wide)out[used++]=hex[p->pad_colours[i]>>4];out[used++]=hex[p->pad_colours[i]&15];}
        out[used++]='\n';out[used]=0;
    }
    return (int)used;
}
static int number(const char **cursor,const char *end,int *out,char delimiter)
{
    const char *s=*cursor; int sign=1,value=0,digits=0;
    if (s<end && *s=='-') { sign=-1; ++s; }
    while (s<end && *s>='0' && *s<='9') {
        if (++digits>6) return 0;
        value=value*10+*s++-'0';
    }
    if (!digits || s==end || *s++!=delimiter) return 0;
    *out=sign*value; *cursor=s; return 1;
}
static int unhex(char c) { return c>='0' && c<='9' ? c-'0' : c>='a' && c<='f' ? c-'a'+10 : -1; }
int music_decode(const char *in,size_t size,struct music_project *out)
{
    int wide=size>=7&&!memcmp(in,"MPCS20 ",7);
    if (size<12 || size>=MUSIC_PAYLOAD_BYTES || (!wide&&memcmp(in,"MPCS12 ",7)&&memcmp(in,"MPCS13 ",7)&&memcmp(in,"MPCS14 ",7)&&memcmp(in,"MPCS15 ",7)&&memcmp(in,"MPCS16 ",7)&&memcmp(in,"MPCS17 ",7)&&memcmp(in,"MPCS18 ",7)&&memcmp(in,"MPCS19 ",7))) return 0;
    int version13=(wide? '9':in[5])>='3',version14=(wide? '9':in[5])>='4',version15=(wide? '9':in[5])>='5',version16=(wide? '9':in[5])>='6',version17=(wide? '9':in[5])>='7',version18=(wide? '9':in[5])>='8',version19=wide||in[5]=='9';
    const char *s=in+7,*end=in+size; int count;
    music_defaults(out);
    if (!number(&s,end,&out->tonic,' ') || !number(&s,end,&out->scale,' ')
        || !number(&s,end,&count,'\n') || out->tonic<0 || out->tonic>11
        || out->scale<0 || out->scale>7 || count<0 || count>MUSIC_MAX_SAMPLES) return 0;
    out->count=(unsigned)count;
    for (unsigned i=0;i<out->count;++i) {
        struct music_sample *p=&out->samples[i];
        if (!number(&s,end,&p->root,' ') || !number(&s,end,&p->mode,' ')
            || !number(&s,end,&p->transpose,' ') || !number(&s,end,&p->colour[0],' ')
            || !number(&s,end,&p->colour[1],' ') || !number(&s,end,&p->colour[2],' ')
            || !number(&s,end,&p->tonic,' ') || !number(&s,end,&p->scale,version13?' ':'\n')) return 0;
        p->original_colour=4;p->fifth_colour=MUSIC_DEFAULT_FIFTH_COLOUR;p->fourth_colour=MUSIC_DEFAULT_FOURTH_COLOUR;
        if(version13&&!number(&s,end,&p->original_colour,version18?' ':'\n'))return 0;
        if(version18&&!number(&s,end,&p->fifth_colour,version19?' ':'\n'))return 0;
        if(version19&&!number(&s,end,&p->fourth_colour,'\n'))return 0;
        if (p->root< -1 || p->root>11 || p->mode<0 || p->mode>(version13?4:3) || p->transpose< -24 || p->transpose>24 || p->original_colour<0 || p->original_colour>=(wide?12+MUSIC_PALETTE_SLOTS:12) || p->fifth_colour<0 || p->fifth_colour>=(wide?12+MUSIC_PALETTE_SLOTS:12) || p->fourth_colour<0 || p->fourth_colour>=(wide?12+MUSIC_PALETTE_SLOTS:12)) return 0;
        for (unsigned j=0;j<3;++j) if (p->colour[j]<0 || p->colour[j]>=(wide?12+MUSIC_PALETTE_SLOTS:12)) return 0;
        if (p->tonic< -1 || p->tonic>11 || p->scale< -1 || p->scale>7) return 0;
        size_t n=0;
        while (s<end && *s!='\n') {
            if (n>=MUSIC_NAME_BYTES-1 || end-s<2) return 0;
            int a=unhex(*s++),b=unhex(*s++);
            if (a<0 || b<0 || !(a|b)) return 0;
            p->name[n++]=(char)((a<<4)|b);
        }
        if (!n || s==end || *s++!='\n') return 0;
        p->name[n]=0;
        for (unsigned j=0;j<i;++j) if (!strcmp(p->name,out->samples[j].name)) return 0;
    }
    if(version14){
        if(end-s<6||memcmp(s,"EQ128\n",6))return 0;
        s+=6;
        for(unsigned pad=0;pad<128;++pad){uint64_t v=0;if(end-s<9)return 0;
            for(unsigned j=0;j<8;++j){int d=unhex(*s++);if(d<0)return 0;v=(v<<4)|(unsigned)d;}
            if(*s++!='\n')return 0;
#ifdef MPC_PAD_EQ
            if(!pad_eq_valid(v))return 0;
#endif
            out->pad_eq[pad]=v;
        }
    }
    if(version15){
        if(end-s!=(wide?528:version17?272:136)||memcmp(s,"SEQ128\n",7))return 0;
        s+=7;
        for(unsigned i=0;i<128;++i){int v=unhex(*s++);if(wide){int low=unhex(*s++);if(v<0||low<0)return 0;v=v*16+low;}if(v<0||v>=(version16?MUSIC_SEQUENCE_COLOURS:5))return 0;out->sequence_colours[i]=(unsigned char)v;}
        if(*s++!='\n')return 0;
        if(version17){
            if(memcmp(s,"PAD128\n",7))return 0;
            s+=7;
            for(unsigned i=0;i<128;++i){int v=unhex(*s++);if(wide){int low=unhex(*s++);if(v<0||low<0)return 0;v=v*16+low;}if(v<0||v>=MUSIC_SEQUENCE_COLOURS)return 0;out->pad_colours[i]=(unsigned char)v;}
            if(*s++!='\n')return 0;
        }
    }
    return s==end;
}

static uint64_t light_pack(unsigned colour,unsigned role){
 unsigned low=role<3?28+role*4:role==3?41:role==4?45:49;
 unsigned high=53+role*2;
 uint64_t v=(uint64_t)(colour&15)<<low;
 if(role==5){v|=(uint64_t)((colour>>4)&1)<<63;v|=(uint64_t)((colour>>5)&1)<<27;}
 else v|=(uint64_t)(colour>>4)<<high;
 return v;
}
static unsigned light_unpack(uint64_t v,unsigned role){
 unsigned low=role<3?28+role*4:role==3?41:role==4?45:49;
 unsigned upper=role==5?(unsigned)((v>>63)&1)|((unsigned)((v>>27)&1)<<1):(unsigned)(v>>(53+role*2))&3;
 return ((unsigned)(v>>low)&15)|(upper<<4);
}
uint64_t music_light_snapshot(const struct music_sample *p,int semitones)
{
    int tonic,scale; music_get_key(&tonic,&scale);
    if (!p || p->root< -1 || p->root>11 || p->mode<0 || p->mode>MUSIC_DEGREES
        || p->transpose< -24 || p->transpose>24 || p->tonic< -1 || p->tonic>11
        || p->scale< -1 || p->scale>7 || p->original_colour<0 || p->original_colour>=12+MUSIC_PALETTE_SLOTS || p->fifth_colour<0 || p->fifth_colour>=12+MUSIC_PALETTE_SLOTS || p->fourth_colour<0 || p->fourth_colour>=12+MUSIC_PALETTE_SLOTS
        || semitones< -96 || semitones>96) return 0;
    for(unsigned i=0;i<3;++i)if(p->colour[i]<0||p->colour[i]>=12+MUSIC_PALETTE_SLOTS)return 0;
    if (p->tonic>=0) tonic=p->tonic;
    if (p->scale>=0) scale=p->scale;
    /* Tonic was dropped in r25f and lights as Scale. ID 4 is reused by the
     * r27q Degrees layout: key notes only, with I and IV/V marked. Older mod
     * builds still decode it and light it as Scale. */
    int mode=p->mode==MUSIC_TONIC?MUSIC_SCALE:p->mode;
    uint64_t v=(uint64_t)(p->root+1) | ((uint64_t)(mode&3)<<4) | ((uint64_t)(p->transpose+24)<<6)
        | ((uint64_t)(semitones+96)<<12) | ((uint64_t)tonic<<20) | ((uint64_t)scale<<24);
    for (unsigned i=0;i<3;++i) v|=light_pack((unsigned)p->colour[i],i);
    v|=(uint64_t)(mode>>2)<<40;
    v|=light_pack((unsigned)p->original_colour,3);
    v|=light_pack((unsigned)p->fifth_colour,4);
    v|=light_pack((unsigned)p->fourth_colour,5);
    return v;
}
void music_publish(const struct music_sample *p,int semitones)
{ atomic_store(&published,music_light_snapshot(p,semitones)); }
uint64_t music_published(void) { return atomic_load(&published); }
/* Layout choices: Monochrome, Piano, Scale and Degrees. Tonic was dropped
 * in r25f and shows as Scale. */
int music_mode_ui_index(int mode){return mode==MUSIC_MONO?0:mode==MUSIC_PIANO?1:mode==MUSIC_DEGREES?3:2;}
int music_mode_from_ui_index(int index){return index<=0?MUSIC_MONO:index==1?MUSIC_PIANO:index==2?MUSIC_SCALE:MUSIC_DEGREES;}
/* Scale degree ordinal (0 = I) of a pitch class relative to the tonic, or -1
 * outside the scale. Chromatic has no ordinal degrees: use the perfect fourth
 * and fifth so IV/V still mark the same pitches as in Major. */
int music_degree_role(unsigned mask,int degree)
{
    if(degree<0||degree>11||!(mask&(1u<<degree)))return -1;
    if(degree==0)return 0;
    if(mask==0xfffu)return degree==5?1:degree==7?3:2;
    unsigned ordinal=0;for(int i=0;i<degree;++i)ordinal+=(mask>>i)&1u;
    return ordinal==3?1:ordinal==4?3:2;
}
int music_transposition(void) { uint64_t v=music_published(); return v ? (int)((v>>6)&63)-24 : 0; }
uint64_t music_pitch_light(uint64_t v,int semitones,int *replace)
{
    *replace=0;
    /* Musical 16 Levels layouts are explicitly available in both lighting
     * modes; Standard only disables Sample/Seq identity-colour overlays. */
    int root=(int)(v&15)-1,mode=(int)((v>>4)&3)|((int)((v>>40)&1)<<2);
    if (!v || root<0 || mode==MUSIC_MONO || semitones< -120 || semitones>120) return 0;
    int note=(root+(int)((v>>12)&255)-96+semitones)%12;
    if (note<0) note+=12;
    uint32_t colour; float brightness;
    if (mode==MUSIC_PIANO) {
        int natural=(0xab5u>>note)&1u;
        colour=natural ? 0xffffff : 0x2030ff; brightness=natural ? 0.35f : 0.10f;
    } else {
        int degree=(note-(int)((v>>20)&15)+12)%12;
        unsigned mask=scale_masks[(v>>24)&7];
        if(mode==MUSIC_DEGREES){
            /* Key notes: I = Tonic colour, IV and V = their own colours,
             * other degrees = the Scale (key notes) colour, dimmer. The Degrees pad
             * layout only produces key notes; recorded out-of-key pitches are
             * off. The original pitch is marked only when it is a key note. */
            int role=music_degree_role(mask,degree);
            if(role<0){colour=0;brightness=0.f;}
            else{unsigned slot=light_unpack(v,role==0?0:role==1?5:role==3?4:1);colour=music_light_colour_rgb(slot);brightness=role==2?0.32f:0.60f;}
        } else {
            int role=degree==0 ? 0 : (mask&(1u<<degree)) ? 1 : 2;
            colour=music_light_colour_rgb(light_unpack(v,(unsigned)role));
            brightness=role==0 ? 0.60f : role==1 ? 0.28f : 0.08f;
        }
    }
    /* Mark the reference pitch, not every octave of its pitch class. */
    if(semitones==0){colour=music_light_colour_rgb(light_unpack(v,3));brightness=.75f;}
    uint32_t bits; memcpy(&bits,&brightness,sizeof(bits)); *replace=1;
    return colour|((uint64_t)bits<<32);
}
/* Degrees layout: consecutive pads play consecutive notes of the key. Index 0
 * is the sample's sounding note if it belongs to the key, otherwise the next
 * key note above it. Other layouts stay chromatic (index = semitones). */
static int in_key(unsigned mask,int tonic,int pitch_class){return (mask>>((((pitch_class-tonic)%12)+12)%12))&1u;}
int music_layout_semitones(uint64_t v,int index)
{
    int root=(int)(v&15)-1,mode=(int)((v>>4)&3)|((int)((v>>40)&1)<<2);
    if(!v||root<0||mode!=MUSIC_DEGREES||index<-200||index>200)return index;
    int base=(root+(int)((v>>12)&255)-96)%12;if(base<0)base+=12;
    int tonic=(int)((v>>20)&15);unsigned mask=scale_masks[(v>>24)&7];
    int s=0;while(!in_key(mask,tonic,base+s))++s;
    for(int k=0;k<index;++k)do ++s;while(!in_key(mask,tonic,base+s));
    for(int k=0;k>index;--k)do --s;while(!in_key(mask,tonic,base+s));
    return s;
}
int music_layout_pad_pitch(uint64_t v,unsigned pad,int original_pad,int *pitch)
{
    if(!v||original_pad<0||original_pad>15)return 0;
    int mode=(int)((v>>4)&3)|((int)((v>>40)&1)<<2),transpose=(int)((v>>6)&63)-24;
    int index=(int)(pad%16)-original_pad+transpose;
    if(mode==MUSIC_DEGREES&&(int)(v&15)>0){
        int s=music_layout_semitones(v,index);
        *pitch=s<-MUSIC_LAYOUT_MAX_PITCH?-MUSIC_LAYOUT_MAX_PITCH:s>MUSIC_LAYOUT_MAX_PITCH?MUSIC_LAYOUT_MAX_PITCH:s;return 1;
    }
    if(!transpose)return 0;
    *pitch=index;return 1;
}
uint64_t music_pad_light(uint64_t v,unsigned pad,int original_pad,int *replace)
{
    if(original_pad<0||original_pad>15){*replace=0;return 0;}
    int pitch=(int)(pad&15)-original_pad+(int)((v>>6)&63)-24;
    music_layout_pad_pitch(v,pad&15,original_pad,&pitch);
    return music_pitch_light(v,pitch,replace);
}
