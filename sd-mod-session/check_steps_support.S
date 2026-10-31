.arch armv8.3-a
.text
.global check_add_dispatch
.type check_add_dispatch,%function
check_add_dispatch:
 bti c
 ldr x9,[sp]
 cbz x9,1f
 brk #1
1:
 stp xzr,xzr,[x8]
 b add_dispatch
.size check_add_dispatch,.-check_add_dispatch
.global check_event_label_bridge
.type check_event_label_bridge,%function
check_event_label_bridge:
 bti c
 paciasp
 stp x29,x30,[sp,#-32]!
 str x24,[sp,#16]
 mov x29,sp
 mov x24,x1
 mov x8,x2
 blr x3
 ldr x24,[sp,#16]
 ldp x29,x30,[sp],#32
 autiasp
 ret
.size check_event_label_bridge,.-check_event_label_bridge
.section .note.GNU-stack,"",%progbits

.text
.global check_mute_dispatch
check_mute_dispatch:
 bti c
 mov x5,x8
 b mute_dispatch
