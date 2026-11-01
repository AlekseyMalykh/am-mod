.arch armv8.3-a
.text
/* C++ shared_ptr has a nontrivial destructor: its 16-byte result uses x8,
 * unlike a trivial 16-byte C struct. Keep that ABI explicit. */
.global step_command_result
.hidden step_command_result
.type step_command_result,%function
step_command_result:
 bti c
 mov x8,x4
 br x5
.size step_command_result,.-step_command_result
.global step_add_result
.hidden step_add_result
.type step_add_result,%function
step_add_result:
 bti c
 paciasp
 ldr x8,[sp]
 ldr x16,[sp,#8]
 sub sp,sp,#32
 stp x29,x30,[sp,#16]
 add x29,sp,#16
 str xzr,[sp]
 blr x16
 ldp x29,x30,[sp,#16]
 add sp,sp,#32
 autiasp
 ret
.size step_add_result,.-step_add_result
.global step_layer_sample
.hidden step_layer_sample
.type step_layer_sample,%function
step_layer_sample:
 bti c
 mov x8,x2
 br x3
.size step_layer_sample,.-step_layer_sample
.global step_pad_string
.hidden step_pad_string
.type step_pad_string,%function
step_pad_string:
 bti c
 mov x8,x1
 br x2
.size step_pad_string,.-step_pad_string
.global step_mute_result
.hidden step_mute_result
.type step_mute_result,%function
step_mute_result:
 bti c
 mov x8,x5
 mov x16,x6
 mov x5,#0
 mov w6,#0
 br x16
.size step_mute_result,.-step_mute_result
.global batch_name_result
.hidden batch_name_result
.type batch_name_result,%function
batch_name_result:
 bti c
 mov x8,x3
 br x4
.size batch_name_result,.-batch_name_result
.global step_table_cell_string
.hidden step_table_cell_string
.type step_table_cell_string,%function
step_table_cell_string:
 bti c
 mov x8,x3
 br x4
.size step_table_cell_string,.-step_table_cell_string
.section .note.GNU-stack,"",%progbits
