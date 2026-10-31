/* Display-only Shift badges. All pointers are PC/entry relative for PIE. */
.macro ui_shift_name_or fail
 ldr x9,[x0,#8]
 adr x10,.Lshift_name
 mov w11,#15
1:
 ldrb w12,[x9],#1
 ldrb w13,[x10],#1
 cmp w12,w13
 b.ne \fail
 subs w11,w11,#1
 b.ne 1b
.endm

/* Leaf: Component* -> known screen ancestor or null. */
ui_root:
 mov x9,x0
 mov w11,#32
1:
 cbz x9,4f
 ldr x12,[x9]
 adr x13,values_screen_offsets
 mov w14,#26
2:
 ldr x15,[x13]
 add x15,x15,x13
 add x13,x13,#8
 cmp x12,x15
 b.eq 3f
 subs w14,w14,#1
 b.ne 2b
 ldr x9,[x9,#0x18]
 subs w11,w11,#1
 b.ne 1b
4:
 mov x0,#0
 ret
3:
 mov x0,x9
 ret

ui_has_shift_rows:
 cbz x0,3f
 ldr x9,[x0]
 adr x10,ui_shift_pairs
 mov w11,#19
1:
 ldr x12,[x10]
 add x12,x12,x10
 cmp x9,x12
 b.eq 2f
 add x10,x10,#24
 subs w11,w11,#1
 b.ne 1b
3:
 mov w0,#0
 ret
2:
 mov w0,#1
 ret

/* Bounds wrapper enters after native BTI; original LR is not PAC signed. */
.global ui_indicator_bounds
.type ui_indicator_bounds,%function
ui_indicator_bounds:
 bti c
 ui_shift_name_or .Lbounds_native
 paciasp
 stp x29,x30,[sp,#-48]!
 mov x29,sp
 stp x0,x1,[sp,#16]
 str x2,[sp,#32]
 bl ui_root
 bl ui_has_shift_rows
 cbz w0,.Lbounds_restore
 mov x1,#0xc600000000
 mov x2,#320
 movk x2,#38,lsl #32
 str x1,[sp,#24]
 str x2,[sp,#32]
.Lbounds_restore:
 ldp x0,x1,[sp,#16]
 ldr x2,[sp,#32]
 ldp x29,x30,[sp],#48
 autiasp
.Lbounds_native:
 mov w3,w2                   // displaced native instruction
 b stock_bounds_continue
.size ui_indicator_bounds,.-ui_indicator_bounds

/* Root -> mask of columns whose current page offers a Shift alternative.
 * Native visibility bit1 is read only; no widget/property is changed.
 * A page anchor may be in a different column (Mix's otherwise empty K2).
 */
ui_shift_mask:
 mov x8,x0
 ldr x9,[x8]
 adr x10,ui_shift_pairs
 mov w11,#19
 mov w0,#0
1:
 ldr x12,[x10]
 add x12,x12,x10
 cmp x9,x12
 b.ne 2f
 ldp w12,w13,[x10,#8]
 add x12,x8,x12
 add x13,x8,x13
 ldr w14,[x12,#0xa8]
 ldr w15,[x13,#0xa8]
 orr w14,w14,w15
 tbz w14,#1,2f
 ldr w12,[x10,#16]
 mov w13,#1
 lsl w13,w13,w12
 orr w0,w0,w13
2:
 add x10,x10,#24
 subs w11,w11,#1
 b.ne 1b
 ret

/* Shared MultiImage paint hook; native PACIASP has already run. */
.global ui_indicator_paint
.type ui_indicator_paint,%function
ui_indicator_paint:
 bti c
 ui_shift_name_or .Lindicator_native
 ldp w9,w10,[x0,#0x28]
 cmp w9,#320
 b.ne .Lindicator_hidden
 cmp w10,#38
 b.ne .Lindicator_hidden
 stp x29,x30,[sp,#-160]!
 mov x29,sp
 stp x19,x20,[sp,#16]
 stp x21,x22,[sp,#32]
 stp x23,x24,[sp,#48]
 stp x25,x26,[sp,#64]
 stp x27,x28,[sp,#80]
 mov x19,x1
 mov x20,x0
 bl ui_root
 cbz x0,.Lindicator_done
 mov w28,#0xffffffff
 ldr x9,[x0]
 adrp x10,knob_fx_vptr
 add x10,x10,:lo12:knob_fx_vptr
 cmp x9,x10
 b.ne .Lindicator_base_ready
 mov w28,#0xff000000
.Lindicator_base_ready:
 bl ui_shift_mask
 mov w21,w0
 cbz w21,.Lindicator_done
 mov w25,w28                // black on Knob FX amber, white elsewhere
 ldr w9,[x20,#0xfc]          // native white/red image selection (0/1)
 cmp w9,#1
 b.ne .Lbadge_colour_ready
 mov w25,#0x4747
 movk w25,#0xffff,lsl #16    // active Shift, red
.Lbadge_colour_ready:
 add x0,sp,#96
 fmov s0,#10.0
 mov w1,#0
 bl font_create
 mov x0,x19
 add x1,sp,#96
 bl graphics_font
 add x0,sp,#96
 bl font_destroy
 add x0,sp,#104
 adr x1,.Lbadge_s
 bl string_utf8
 mov w22,#0
.Lbadge_loop:
 mov w9,#1
 lsl w9,w9,w22
 tst w21,w9
 b.eq .Lbadge_next
 mov w9,#106
 mul w23,w22,w9
 add w23,w23,#94
 mov x0,x19
 mov w1,w25
 bl graphics_colour
 mov x0,x19
 mov w1,w23
 mov w2,#2
 mov w3,#10
 mov w4,#1
 bl fill_rect
 mov x0,x19
 mov w1,w23
 mov w2,#13
 mov w3,#10
 mov w4,#1
 bl fill_rect
 mov x0,x19
 mov w1,w23
 mov w2,#2
 mov w3,#1
 mov w4,#12
 bl fill_rect
 mov x0,x19
 add w1,w23,#9
 mov w2,#2
 mov w3,#1
 mov w4,#12
 bl fill_rect
 mov x0,x19
 add x1,sp,#104
 add w2,w23,#1
 mov w3,#3
 mov w4,#8
 mov w5,#10
 mov w6,#0x24
 mov w7,#1
 fmov s0,#1.0
 bl draw_fitted_text
.Lbadge_next:
 add w22,w22,#1
 cmp w22,#3
 b.ne .Lbadge_loop
 add x0,sp,#104
 bl string_destroy
.Lindicator_done:
 ldp x19,x20,[sp,#16]
 ldp x21,x22,[sp,#32]
 ldp x23,x24,[sp,#48]
 ldp x25,x26,[sp,#64]
 ldp x27,x28,[sp,#80]
 ldp x29,x30,[sp],#160
.Lindicator_hidden:
 autiasp
 ret
.Lindicator_native:
 stp x29,x30,[sp,#-48]!      // displaced native frame
 b stock_indicator_continue
.size ui_indicator_paint,.-ui_indicator_paint

/* UTF-8 data -> optional display-only alias, exact match including NUL. */
.macro ui_alias key, title, next
 mov x9,x15
 adr x10,\key
1:
 ldrb w11,[x9],#1
 ldrb w12,[x10],#1
 cmp w11,w12
 b.ne \next
 cbnz w12,1b
 adr x0,\title
 ret
.endm
ui_name_alias:
 mov x15,x0
 ui_alias .Lkey_length,.Ltitle_length,1f
1:
 ui_alias .Lkey_q,.Ltitle_q,2f
2:
 ui_alias .Lkey_sig,.Ltitle_sig,3f
3:
 ui_alias .Lkey_step_q,.Ltitle_q,4f
4:
 mov x0,#0
 ret
.Lshift_name: .asciz "shiftIndicator"
.Lbadge_s: .asciz "S"
.Lkey_length: .asciz "sequence.length"
.Ltitle_length: .asciz "Length"
.Lkey_q: .asciz "quantise.division"
.Ltitle_q: .asciz "Q"
.Lkey_step_q: .asciz "step.quantise.division"
.Lkey_sig: .asciz "time.signature"
.Ltitle_sig: .asciz "Time Sig"

#include "values_ui_map.inc"
