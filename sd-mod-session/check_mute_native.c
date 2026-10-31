#define main original_steps_main
#include "check_steps.c"
#undef main
static uintptr_t kept[2];
static int native_command_result;
static int run_real_command(void*p,uintptr_t*shared){
 (void)p;void*c=(void*)shared[0];
 if(kept[1])FN(0x123bca0,void(*)(void*))((void*)kept[1]);
 kept[0]=shared[0];kept[1]=shared[1];++FIELD((void*)kept[1],8,unsigned);
 native_command_result=((int(*)(void*))FIELD((void*)FIELD(c,0,uintptr_t),0x40,uintptr_t))(c);return native_command_result;
}
static void no_notify(void*p){(void)p;}
static int contains_native_event(void*list,const struct step_event*e){
 for(void*n=FIELD(list,0x78,void*);n!=(char*)list+0x68;n=FN(0xb51738,void*(*)(void*))(n))
  if(FN(0x19722e0,int(*)(const void*,const void*))((char*)n+0x20,e)&255)return 1;
 return 0;
}
int main(int argc,char**argv){assert(argc==2);struct sigaction a={0};a.sa_sigaction=fault;a.sa_flags=SA_SIGINFO;sigaction(SIGSEGV,&a,NULL);sigaction(SIGBUS,&a,NULL);sigaction(SIGILL,&a,NULL);
load_elf(argv[1],0x1800000000ULL);phase="native list mute replacement";
stub(0x27ee100,no_notify);stub(0x14c43c0,no_notify);__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
unsigned char storage[0x300]={0};FN(0x27eb120,void(*)(void*))(storage);
FILE*f=fopen(argv[1],"rb");assert(f);fseek(f,0x194fd60,SEEK_SET);assert(fread((void*)(base+0x194fd60),1,16,f)==16);fseek(f,0x123bca0,SEEK_SET);assert(fread((void*)(base+0x123bca0),1,16,f)==16);fclose(f);stub(0x19d0a00,run_real_command);__builtin___clear_cache((char*)base,(char*)(base+0x7500000));
struct step_event old=make_note(4,240,.8f),other=old;other.time=260;other.type|=0x200;
FIELD(old.data,4,uint16_t)=0x8001;FIELD(old.data,6,unsigned char)=122;
FN(0x27ee5d0,void*(*)(void*,const void*))(storage,&old);
FN(0x27ee5d0,void*(*)(void*,const void*))(storage,&old); /* exact duplicates */
FN(0x27ee5d0,void*(*)(void*,const void*))(storage,&other);
setup();event_list=storage;FIELD(control,0xc0,void*)=storage;phase="real native group Mute, not fixture dispatch";
struct add_context c={0};c.page=0;c.sound=4;c.grid=2;c.slot=1;c.expected=3;c.mute_change=1;c.mute_value=1;
/* Factory preconditions: its native project/settings references. */
unsigned char context[0xc0]={0};FIELD(factory,0x28,void*)=context;FIELD(factory,0x50,void*)=context;
assert(run_set_command(&c));
struct slot out[16];assert(scan_page(0,4,2,out)&&out[1].count==3&&out[1].muted==3);
void*cmd=(void*)kept[0];assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x48,uintptr_t))(cmd)==0);
assert(scan_page(0,4,2,out)&&out[1].count==3&&out[1].muted==1); /* Undo */
assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x40,uintptr_t))(cmd)==0);
assert(scan_page(0,4,2,out)&&out[1].count==3&&out[1].muted==3); /* Redo */
struct add_context d={0};d.page=0;d.sound=4;d.grid=2;d.slot=1;d.expected=3;d.mute_change=1;d.mute_value=0;
assert(run_set_command(&d));assert(scan_page(0,4,2,out)&&out[1].count==3&&!out[1].muted);
struct step_event far=old;far.time=7440;far.type|=0x200;
struct step_event near=far;near.time+=30;
struct step_event unrelated=far;unrelated.data[0x20]=3;
FN(0x27ee5d0,void*(*)(void*,const void*))(storage,&far);
FN(0x27ee5d0,void*(*)(void*,const void*))(storage,&near);
FN(0x27ee5d0,void*(*)(void*,const void*))(storage,&unrelated);
struct add_context move={0};move.page=1;move.sound=4;move.grid=2;move.slot=15;move.expected=2;move.move_delta=240;
assert(run_set_command(&move));
struct step_event moved_far=far,moved_near=near;moved_far.time=0;moved_near.time=30;
assert(contains_native_event(storage,&moved_far)&&contains_native_event(storage,&moved_near)&&contains_native_event(storage,&unrelated));
assert(!memcmp(moved_far.data,far.data,40)&&moved_far.type==far.type);
cmd=(void*)kept[0];assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x48,uintptr_t))(cmd)==0);
assert(contains_native_event(storage,&far)&&contains_native_event(storage,&near));
struct add_context all={0};all.sound=4;all.mute_change=all.unmute_sound=1;
assert(run_set_command(&all));
assert(scan_page(1,4,2,out)&&out[15].count==2&&!out[15].muted);
assert(contains_native_event(storage,&unrelated)&&event_muted(&unrelated));
cmd=(void*)kept[0];assert(((int(*)(void*))FIELD((void*)FIELD(cmd,0,uintptr_t),0x48,uintptr_t))(cmd)==0);
assert(scan_page(1,4,2,out)&&out[15].count==2&&out[15].muted==2);
puts("PASS actual native group move across sequence wrap preserves offsets/payload/mute and Undo; whole-sound Unmute clears only selected sound across all pages and Undo restores flags");
puts("PASS actual native mute/unmute, exact duplicate notes, mixed group, Undo/Redo, note count preserved");return 0;}
