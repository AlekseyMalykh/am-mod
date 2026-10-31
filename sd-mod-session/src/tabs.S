/* UI-thread only. Native focus-list rebuild identifies a changed Sample tab.
 * All timers belong to the native sliders. No persistent pointers/new fields.
 */
.arch armv8.3-a
.text
.global tab_clear_preview
.type tab_clear_preview,%function
tab_clear_preview:
 mov w1,#0
 b .Lwalk
.global tab_show_preview
.type tab_show_preview,%function
tab_show_preview:
 mov w1,#1
 b .Lwalk
.Lwalk:
 bti c
 paciasp
 stp x29,x30,[sp,#-64]!
 mov x29,sp
 stp x19,x20,[sp,#16]
 stp x21,x22,[sp,#32]
 stp x23,x24,[sp,#48]
 mov w22,w1
 ldp x19,x20,[x0,#0x70]
 sub x2,x20,x19
 cmp x2,#512
 b.hi .Lwalk_done
 tst x2,#7
 b.ne .Lwalk_done
.Lnext:
 cmp x19,x20
 b.eq .Lwalk_done
 ldr x21,[x19],#8
 cbz x21,.Lnext
 ldr x2,[x21]
 adrp x3,slider_hw_vptr
 add x3,x3,:lo12:slider_hw_vptr
 cmp x2,x3
 b.ne .Lnext
 sub x21,x21,#0x1d8
 ldr x2,[x21]
 adrp x3,slider_vptr
 add x3,x3,:lo12:slider_vptr
 cmp x2,x3
 b.ne .Lnext
 cbnz w22,.Lshow
 ldr w2,[x21,#0x208]
 cmp w2,#2000
 b.eq .Lclear
 cmp w2,#2002
 b.ne .Lnext
.Lclear:
 strb wzr,[x21,#0x230]
 add x0,x21,#0x1f8
 bl stop_timer
 b .Lpaint
.Lshow:
 mov w2,#1
 strb w2,[x21,#0x230]
 add x0,x21,#0x1f8
 mov w1,#2002
 bl start_timer
.Lpaint:
 mov x0,x21
 bl repaint
 b .Lnext
.Lwalk_done:
 ldp x19,x20,[sp,#16]
 ldp x21,x22,[sp,#32]
 ldp x23,x24,[sp,#48]
 ldp x29,x30,[sp],#64
 autiasp
 ret

.global tab_after_refresh
.type tab_after_refresh,%function
tab_after_refresh:
 bti c
 // The native AsyncUpdater receives HWFocusContainer* directly after thunk.
 ldr x1,[x0,#0x68]
 cbz x1,.Lnative_only
 ldr x2,[x1]
 adrp x3,sample_vptr
 add x3,x3,:lo12:sample_vptr
 cmp x2,x3
 b.ne .Lnative_only
 mov x2,#0x99c0
 add x2,x1,x2
 cmp x0,x2
 b.ne .Lnative_only
 paciasp
 sub sp,sp,#576
 stp x29,x30,[sp]
 mov x29,sp
 stp x19,x20,[sp,#16]
 stp x21,x22,[sp,#32]
 stp x23,x24,[sp,#48]
 mov x19,x0
 ldp x20,x21,[x19,#0x70]
 sub x22,x21,x20
 cmp x22,#512
 b.hi .Linvalid_snapshot
 tst x22,#7
 b.ne .Linvalid_snapshot
 mov x23,x20
 add x24,sp,#64
.Lsnapshot:
 cmp x23,x21
 b.eq .Lrefresh
 ldr x2,[x23],#8
 str x2,[x24],#8
 b .Lsnapshot
.Lrefresh:
 mov x0,x19
 bl rebuild_focus
 ldp x20,x21,[x19,#0x70]
 sub x2,x21,x20
 cmp x2,x22
 b.ne .Lchanged
 mov x23,x20
 add x24,sp,#64
.Lcompare:
 cmp x23,x21
 b.eq .Lrefresh_done
 ldr x2,[x23],#8
 ldr x3,[x24],#8
 cmp x2,x3
 b.ne .Lchanged
 b .Lcompare
.Lchanged:
 mov x0,x19
 bl tab_show_preview
 b .Lrefresh_done
.Linvalid_snapshot:
 mov x0,x19
 bl rebuild_focus
.Lrefresh_done:
 ldp x19,x20,[sp,#16]
 ldp x21,x22,[sp,#32]
 ldp x23,x24,[sp,#48]
 ldp x29,x30,[sp]
 add sp,sp,#576
 autiasp
 ret
.Lnative_only:
 b rebuild_focus
.size tab_after_refresh,.-tab_after_refresh
