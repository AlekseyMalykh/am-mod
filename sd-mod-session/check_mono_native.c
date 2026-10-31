#define BATCH_PROOF_MAIN base_batch_main
#include "check_batch_native.c"
static unsigned char mono_input[0x140],mono_output[0x140],mono_layers[128][0xb30],mono_services[0x80],mono_functions[8],mono_pool[8];
static uintptr_t mono_reference[4],assign_table[10];static int member,fail_mono,fail_assignment;static unsigned selected_channel;
static void*mono_lookup(void*p,const uintptr_t*t){static void*entry;assert(p==mono_services&&*t==base+0x6e09000);entry=mono_functions;return &entry;}
static int lock_ok(void*p){(void)p;return 0;}
void mono_name(void*p,const uint64_t*n,const uintptr_t*none,uint64_t*out){(void)p;(void)none;FN(0xf1c640,void(*)(void*,const void*))(out,n);}
void mono_layer(void*i,int layer,uintptr_t*out){assert(!layer);out[0]=FIELD(i,0x200,uintptr_t);out[1]=out[0]?(uintptr_t)mono_reference:0;if(out[1])++FIELD(mono_reference,8,unsigned);}
void mono_convert(void*f,const uintptr_t*in,unsigned channel,unsigned depth,const uint64_t*name,uintptr_t*out){assert(f==mono_functions&&in[0]==(uintptr_t)mono_input&&channel<2&&depth==24&&name&&*name);selected_channel=channel;out[0]=fail_mono?0:(uintptr_t)mono_output;out[1]=out[0]?(uintptr_t)mono_reference:0;if(out[1])++FIELD(mono_reference,8,unsigned);}
static int mono_channels(void*p){return p==mono_input?2:1;}
static int mono_frames(void*p){assert(p==mono_input||p==mono_output);return 12345;}
static uint64_t mono_contains(void*p,const uintptr_t*s){(void)p;(void)s;return (uint64_t)member<<32;}
void mono_add(void*p,const uintptr_t*s,uintptr_t*out){(void)p;assert(s[0]==(uintptr_t)mono_output);member=1;out[0]=out[1]=0;}
static void mono_remove(void*p,const uintptr_t*s){(void)p;assert(s[0]==(uintptr_t)mono_output);member=0;}
static int assign_do(void*p){unsigned n=FIELD(p,16,unsigned);FIELD(p,32,uintptr_t)=FIELD(pads[n],0x200,uintptr_t);FIELD(pads[n],0x200,uintptr_t)=FIELD(p,24,uintptr_t);return fail_assignment;}
static int assign_undo(void*p){FIELD(pads[FIELD(p,16,unsigned)],0x200,uintptr_t)=FIELD(p,32,uintptr_t);return 0;}
static void assign_delete(void*p){free(p);}
static void assign_ctor(void*p,void*props,void*program,const uintptr_t*sample,int pad,int layer,void*service){(void)service;assert(props==properties&&program==copy_program&&!layer);FIELD(p,0,void*)=assign_table;FIELD(p,16,unsigned)=pad;FIELD(p,24,uintptr_t)=sample[0];}
static void*copy_range(void*a,void*b){assert(a&&b);return a;}
extern void mono_convert_bridge(void),mono_layer_bridge(void),mono_name_bridge(void),mono_add_bridge(void);
int main(int argc,char**argv){assert(!base_batch_main(argc,argv));phase="Mono command assignment rollback";
 stub(0x143ad50,mono_lookup);stub(0xb51828,lock_ok);stub(0xb51808,lock_ok);stub(0x292f0e0,mono_name_bridge);stub(0x2803b90,mono_layer_bridge);stub(0x209e510,mono_convert_bridge);stub(0x2941f70,mono_channels);stub(0x2941f80,mono_frames);stub(0x292eb00,mono_contains);stub(0x2931440,mono_add_bridge);stub(0x2930010,mono_remove);stub(0x1995f90,assign_ctor);stub(0x294bca0,copy_range);
 __builtin___clear_cache((char*)base,(char*)(base+0x7500000));
 FIELD(properties,0x1278,void*)=mono_pool;FIELD(context,0x48,void*)=mono_services;FIELD(mono_reference,8,unsigned)=1000;
 assign_table[1]=(uintptr_t)assign_delete;assign_table[8]=(uintptr_t)assign_do;assign_table[9]=(uintptr_t)assign_undo;
 for(unsigned i=0;i<128;++i){FIELD(pads[i],0x22e8,void*)=mono_layers[i];FIELD(pads[i],0x200,uintptr_t)=(uintptr_t)mono_input;}
 for(unsigned channel=0;channel<2;++channel){unsigned to=channel?0:21;
  assert(batch_mono_submit(base,context,mono_services,0,to,channel)&&selected_channel==channel&&member&&FIELD(pads[to],0x200,void*)==mono_output);
  void*c=(void*)kept[0];assert(!((int(*)(void*))FIELD((void*)FIELD(c,0,uintptr_t),0x48,uintptr_t))(c));assert(!member&&FIELD(pads[to],0x200,void*)==mono_input);
  assert(!((int(*)(void*))FIELD((void*)FIELD(c,0,uintptr_t),0x40,uintptr_t))(c));assert(member&&FIELD(pads[to],0x200,void*)==mono_output);
  assert(!((int(*)(void*))FIELD((void*)FIELD(c,0,uintptr_t),0x48,uintptr_t))(c));
 }
 for(unsigned failure=0;failure<2;++failure){fail_mono=!failure;fail_assignment=failure;assert(!batch_mono_submit(base,context,mono_services,0,21,0));assert(!member&&FIELD(pads[21],0x200,void*)==mono_input&&FIELD(pads[0],0x200,void*)==mono_input);}
 puts("PASS Mono command with actual native CopyPad Undo/Redo: Left/Right requests, source unchanged, same-pad replacement, one command, conversion and assignment failure rollback; conversion/Assign/Pool are explicit receivers");return 0;
}
