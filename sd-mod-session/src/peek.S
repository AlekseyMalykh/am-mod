/* MPC Sample 1.3.0.12 only. All addresses are relative to the main PIE.
 * The code lives in verified zero padding, newly covered by its RX segment.
 * Entry replaces the branch after the original callback's BTI instruction.
 */
.arch armv8.3-a
.text
.global peek_encoder
.type peek_encoder, %function
peek_encoder:
    bti c
    tbz w1, #0, .Lreturn
    paciasp
    stp x29, x30, [sp, #-48]!
    mov x29, sp
    stp x19, x20, [sp, #16]
    str x21, [sp, #32]
    mov x19, x0               // Callback receives SampleModeScreen* directly.
    mov x20, #0x99c0
    add x20, x19, x20

    // Clear the old peek only on an exact stock SampleModeScreen.
    ldr x2, [x19]
    adrp x3, sample_vptr
    add x3, x3, :lo12:sample_vptr
    cmp x2, x3
    b.ne .Lcycle_only
    // Encoder selection ends the whole-tab preview before selecting one value.
    mov x0, x20
    bl tab_clear_preview
    .rept 8
    nop
    .endr
.Lcycle:
    // Reproduce the stock Shift/direction lookup and focus traversal.
    ldr x1, [x19, #0x108]
    add x1, x1, #0x2000
    ldrb w1, [x1, #0x2a0]
    mov x0, x20
    bl cycle_focus
    mov x0, x20
    bl get_focus
    bl .Las_slider
    cbz x0, .Ldone
    mov x21, x0
    mov w1, #1
    strb w1, [x21, #0x230]     // Native "temporarily show value" flag.
    add x0, x21, #0x1f8       // Native Timer embedded in ui::Slider.
    mov w1, #2000
    bl start_timer
    mov x0, x21
    bl repaint
    b .Ldone
.Lcycle_only:
    ldr x1, [x19, #0x108]
    add x1, x1, #0x2000
    ldrb w1, [x1, #0x2a0]
    mov x0, x20
    bl cycle_focus
.Ldone:
    ldr x21, [sp, #32]
    ldp x19, x20, [sp, #16]
    ldp x29, x30, [sp], #48
    autiasp
.Lreturn:
    ret

// Return the ui::Slider only if the currently live focus vector contains it.
// No pointer survives this synchronous UI callback; no new timer is allocated.
.Las_slider:
    cbz x0, .Lnot_slider
    ldr x2, [x0]
    adrp x3, slider_hw_vptr
    add x3, x3, :lo12:slider_hw_vptr
    cmp x2, x3
    b.ne .Lnot_slider
    ldp x2, x3, [x20, #0x70]
    mov w5, #64
.Lfind:
    cmp x2, x3
    b.eq .Lnot_slider
    cbz w5, .Lnot_slider
    ldr x4, [x2], #8
    cmp x4, x0
    b.eq .Lslider_found
    sub w5, w5, #1
    b .Lfind
.Lslider_found:
    sub x0, x0, #0x1d8
    ret
.Lnot_slider:
    mov x0, #0
    ret
.size peek_encoder, .-peek_encoder

// Replace only the ui::Slider value-change call to startTimer. Preserve the
// original 500ms argument everywhere outside the Sample component hierarchy.
.global peek_rotation_timer
.type peek_rotation_timer, %function
peek_rotation_timer:
    bti c
    sub x2, x0, #0x1f8
    ldr x3, [x2]
    adrp x4, slider_vptr
    add x4, x4, :lo12:slider_vptr
    cmp x3, x4
    b.ne .Lnative_timer
    adrp x4, sample_vptr
    add x4, x4, :lo12:sample_vptr
    mov w5, #32
.Lparent:
    cbz x2, .Lnative_timer
    ldr x3, [x2]
    cmp x3, x4
    b.eq .Ltwo_seconds
    ldr x2, [x2, #0x18]       // Component::parentComponent, verified in addChild.
    subs w5, w5, #1
    b.ne .Lparent
.Lnative_timer:
    b start_timer
.Ltwo_seconds:
    // Native Timer::interval is also a lifetime-safe display-mode marker:
    // encoder preview=2000ms, actual value change=2001ms (both about 2s).
    // No extra object field or stored component pointer is introduced.
    mov w1, #2001
    b start_timer
.size peek_rotation_timer, .-peek_rotation_timer

.balign 256
.global peek_value_colour
.type peek_value_colour, %function
peek_value_colour:
    bti c
    // Called only from the native enabled-value drawing branch.
    ldr x2, [x0]
    adrp x3, slider_vptr
    add x3, x3, :lo12:slider_vptr
    cmp x2, x3
    b.ne .Lnative_colour
    ldrb w2, [x0, #0x230]
    tbz w2, #0, .Lnative_colour
    ldr w2, [x0, #0x208]      // Embedded Timer +0x10, written by native startTimer.
    cmp w2, #2002            // Whole-tab preview is independent of focus.
    b.eq .Lcolour_parent_start
    cmp w2, #2000            // Single-value encoder preview requires focus.
    b.ne .Lnative_colour
    adrp x2, focus_pointer
    ldr x2, [x2, :lo12:focus_pointer]
    add x3, x0, #0x1d8
    cmp x2, x3               // Only the currently focused control can be yellow.
    b.ne .Lnative_colour
.Lcolour_parent_start:
    mov x2, x0
    adrp x4, sample_vptr
    add x4, x4, :lo12:sample_vptr
    mov w5, #32
.Lcolour_parent:
    cbz x2, .Lnative_colour
    ldr x3, [x2]
    cmp x3, x4
    b.eq .Lamber
    ldr x2, [x2, #0x18]
    subs w5, w5, #1
    b.ne .Lcolour_parent
.Lnative_colour:
    mov w2, #0                // Restore the call site's inheritFromParent arg.
    b find_colour
.Lamber:
    mov w0, #0xc247
    movk w0, #0xffff, lsl #16  // JUCE ARGB 0xffffc247, warm amber.
    ret
.size peek_value_colour, .-peek_value_colour
