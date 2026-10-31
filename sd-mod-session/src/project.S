/* Project menu: append one independently owned native row; request Mod Off. */
.arch armv8.3-a
.text
.global project_append
.type project_append,%function
project_append:
 bti c
 paciasp
 stp x29,x30,[sp,#-48]!
 mov x29,sp
 stp x19,x20,[sp,#16]
 mov x19,x0
 mov x20,x1
 bl list_append                  // Original SD Card Access append.
 mov x0,x19
 mov x1,x20
 bl list_append                  // Native deep copy of the final row.
#ifdef NOTE_UI
 mov x0,x19
 mov x1,x20
 bl list_append                  // Separate Key / Scale row, preserving Mod Off.
#endif
 ldr x0,[x19,#0xc0]             // Native model, vector begin/end at +8/+16.
 ldp x1,x2,[x0,#8]
 sub x3,x2,x1
#ifdef NOTE_UI
 cmp x3,#0x300
#else
 cmp x3,#0x280                  // Exactly five MenuItem objects of 128 bytes.
#endif
 b.ne .Lappend_done
#ifdef NOTE_UI
 sub x0,x2,#0x80
 adr x1,mod_off_title
 bl .Lrename
 ldr x0,[x19,#0xc0]
 ldr x2,[x0,#16]
 sub x0,x2,#0x100
#else
 sub x0,x2,#0x80
#endif
#ifdef NOTE_UI
 adr x1,music_title
#else
 adr x1,mod_off_title
#endif
 bl .Lrename
.Lappend_done:
 ldp x19,x20,[sp,#16]
 ldp x29,x30,[sp],#48
 autiasp
 ret
.Lrename:
 stp x29,x30,[sp,#-32]!
 mov x29,sp
 str x20,[sp,#16]
 ldr x20,[x0,#0x60]
 cbz x20,.Lrename_done
 ldp x2,x3,[x20,#0x10]
 orr x2,x2,x3
 cbnz x2,.Lrename_done
 sub sp,sp,#16
 mov x0,sp
 bl string_utf8
 add x0,x20,#0x28
 mov x1,sp
 bl string_assign
 mov x0,sp
 bl string_destroy
 add sp,sp,#16
.Lrename_done:
 ldr x20,[sp,#16]
 ldp x29,x30,[sp],#32
 ret
.size project_append,.-project_append

.global project_dispatch
.type project_dispatch,%function
project_dispatch:
 bti c
#ifdef NOTE_UI
 cmp w0,#4
 b.eq .Lkey
 cmp w0,#5
 b.eq .Lconfirm_off
#else
 cmp w0,#4
 b.eq .Loff
#endif
 cmp w0,#3                     // Displaced stock comparison.
 b project_dispatch_continue
.Loff:
 // Existing callback frame is 32 bytes; x19 and the PAC-signed LR are on it.
 // No system bus or persistent filesystem operation; one-byte RAM request.
 mov x0,#-100
 adr x1,mod_off_path
 mov x2,#0x8241                 // AArch64 O_NOFOLLOW | O_WRONLY | O_CREAT | O_TRUNC
 movk x2,#0x8,lsl #16          // AArch64 O_CLOEXEC
 mov x3,#0x180                  // 0600
 mov x8,#56                    // openat
 svc #0
 tbnz x0,#63,.Loff_done
 mov x9,x0
 adr x1,mod_off_byte
 mov x2,#1
 mov x8,#64                    // write
 svc #0
 mov x0,x9
 mov x8,#57                    // close
 svc #0
.Loff_done:
 ldr x19,[sp,#16]
 ldp x29,x30,[sp],#32
 autiasp
 ret
#ifdef NOTE_UI
.Lconfirm_off:
 ldr x0,[x19]
 orr x0,x0,#1                  // Tagged UI request: confirm, never exit here.
 bl music_project_bridge
 b .Loff_done
.Lkey:
 ldr x0,[x19]
 bl music_project_bridge
 b .Loff_done
#endif
.size project_dispatch,.-project_dispatch
mod_off_title: .asciz "Mod Off"
#ifdef NOTE_UI
music_title: .asciz "Key / Scale"
#endif
mod_off_path: .asciz "/run/mpc-trial-app/mod-off.request"
mod_off_byte: .ascii "1"
