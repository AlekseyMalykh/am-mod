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
