.text
.align 2
.global project_exit_submit
.type project_exit_submit,%function
project_exit_submit:
    bti c
    paciasp
    stp x29,x30,[sp,#-32]!
    mov x29,sp
    stp x19,x20,[sp,#16]
    mov x19,x8
    adrp x16,:got:project_exit_submit_original
    ldr x16,[x16,:got_lo12:project_exit_submit_original]
    ldr x16,[x16]
    blr x16
    mov x20,x0
    mov x0,x19
    bl project_exit_capture_future
    mov x0,x20
    ldp x19,x20,[sp,#16]
    ldp x29,x30,[sp],#32
    autiasp
    ret
.size project_exit_submit,.-project_exit_submit
.section .note.GNU-stack,"",%progbits
