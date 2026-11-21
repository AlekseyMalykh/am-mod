.data
.global am_record_state_resume
.balign 8
am_record_state_resume:
 .quad 0
.text
.global am_record_state_stub
.balign 4
am_record_state_stub:
 bti c
 stp x29,x30,[sp,#-32]!
 stp x0,x1,[sp,#16]
 bl am_record_state_seen
 ldp x0,x1,[sp,#16]
 ldp x29,x30,[sp],#32
 paciasp
 stp x29,x30,[sp,#-64]!
 mov x29,sp
 stp x19,x20,[sp,#16]
 mov x20,x0
 ldr x19,[x0,#7160]
 adrp x16,:got:am_record_state_resume
 ldr x16,[x16,:got_lo12:am_record_state_resume]
 ldr x16,[x16]
 br x16
.section .note.GNU-stack,"",%progbits
