.text
.align 2
.global check_power_continuation
.type check_power_continuation,%function
check_power_continuation:
    bti c
    stp x29,x30,[sp,#-32]!
    mov x29,sp
    mov x0,x2
    bl check_power_question
    ldp x29,x30,[sp],#32
    add sp,sp,#0x1b0
    autiasp
    ret
.size check_power_continuation,.-check_power_continuation
.section .note.GNU-stack,"",%progbits
.text
.global check_power_dispatch
.type check_power_dispatch,%function
check_power_dispatch:
    bti c
    paciasp
    stp x19,x30,[sp,#-16]!
    adr x19,1f
    br x3
1:  bti j
    ldp x19,x30,[sp],#16
    autiasp
    ret
.size check_power_dispatch,.-check_power_dispatch
