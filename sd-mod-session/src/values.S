/* MPC Sample 1.3.0.12: permanent name/value in native 106x38 K blocks.
 * Enter after the stock PACIASP, preserving its signed LR and original BTI.
 * Other components execute the displaced SUB and the untouched stock renderer.
 */
.arch armv8.3-a
.text
.global values_paint
.type values_paint,%function
// Exact native type/size and an approved screen ancestor.
// Sliders retain the r6 size-based guard; Label has a position guard too.
.macro footer_guard component, fail, success, position=1
 ldp w9,w10,[\component,#0x28]
 cmp w9,#106
 b.ne \fail
 cmp w10,#38
 b.ne \fail
 .if \position
 ldp w9,w10,[\component,#0x20]
 cmp w10,#198
 b.ne \fail
 cbz w9,1f
 cmp w9,#106
 b.eq 1f
 cmp w9,#213
 b.ne \fail
1:
 .endif
 mov x9,\component
 mov w11,#32
2:
 cbz x9,\fail
 ldr x12,[x9]
 adr x13,values_screen_offsets
 mov w14,#26
3:
 ldr x15,[x13]
 add x15,x15,x13             // PIE: runtime entry address + link-time delta.
 add x13,x13,#8
 cmp x12,x15
 b.eq \success
 subs w14,w14,#1
 b.ne 3b
 ldr x9,[x9,#0x18]
 subs w11,w11,#1
 b.ne 2b
 b \fail
.endm
values_paint:
 bti c
 ldr x9,[x7]
 adrp x10,slider_vptr
 add x10,x10,:lo12:slider_vptr
 cmp x9,x10
 b.ne .Lstock_paint
 footer_guard x7,.Lstock_paint,.Lcustom,0
.Lstock_paint:
 sub sp,sp,#0x110
 b stock_paint_continue

.global values_label_paint
.type values_label_paint,%function
values_label_paint:
 bti c
 ldr x9,[x2]
 adrp x10,label_vptr
 add x10,x10,:lo12:label_vptr
 cmp x9,x10
 b.ne .Lstock_label
 footer_guard x2,.Lstock_label,.Llabel_custom
.Lstock_label:
 sub sp,sp,#0x140
 b stock_label_continue
.Llabel_custom:
 mov x7,x2
.Lcustom:
 stp x29,x30,[sp,#-160]!
 mov x29,sp
 stp x19,x20,[sp,#16]
 stp x21,x22,[sp,#32]
 stp x23,x24,[sp,#48]
 stp x25,x26,[sp,#64]
 stp d8,d9,[sp,#80]
 stp x27,x28,[sp,#112]
 mov x19,x1                   // Graphics&
 mov x20,x7                   // Exact supported primary component base.
#ifdef NOTE_UI
 mov x0,x20
 mov x1,x19
 bl note_footer_bridge
 cbnz w0,.Lrestore_frame      // No native Strings have been constructed yet.
#endif
 mov w27,#0
 ldr x9,[x20]
 adrp x10,label_vptr
 add x10,x10,:lo12:label_vptr
 cmp x9,x10
 b.ne .Lslider_data
 mov w27,#1
 mov x0,x20
 mov w1,#0
 add x8,sp,#104
 bl label_text                  // Same formatted text as native Label renderer.
 movi d9,#0
 b .Lfraction_ok
.Lslider_data:
 mov x0,x20
 bl slider_value
 fmov d8,d0
 ldr x9,[x20]
 ldr x9,[x9,#0x180]          // Native getTextFromValue(double), including overrides.
 mov x0,x20
 add x8,sp,#104               // Native JUCE String result, 8 bytes.
 blr x9
 mov x0,x20
 fmov d0,d8
 ldr x9,[x20]
 ldr x9,[x9,#0x190]          // Native valueToProportionOfLength (retains skew).
 blr x9
 fmov d9,d0
 fcmp d9,d9
 b.vs .Lzero_fraction
 fcmp d9,#0.0
 b.lt .Lzero_fraction
 fmov d0,#1.0
 fcmp d9,d0
 b.le .Lfraction_ok
 fmov d9,d0
 b .Lfraction_ok
.Lzero_fraction:
 movi d9,#0
.Lfraction_ok:
 mov x0,x20
 bl component_enabled
 and w23,w0,#1
 mov w1,#0x1400
 movk w1,#0x100,lsl #16
 cbz w27,.Lslider_colour
 mov w1,#0x281
 movk w1,#0x100,lsl #16
.Lslider_colour:
 cbnz w23,.Ltext_id
 add w1,w1,#1
.Ltext_id:
 mov x0,x20
 mov w2,#0
 bl find_colour
 cbz w23,.Ldisabled_colour
 b .Lbase_colour
.Ldisabled_colour:
 cbz w27,.Lbase_colour
 adrp x9,disabled_label_colour
 ldr w0,[x9,:lo12:disabled_label_colour]
.Lbase_colour:
 mov w24,w0                  // Native text/disabled colour.
 mov w25,w24
 mov w21,#0                  // Current focus marker.
 cbz w23,.Lcolour_ready
 cbnz w27,.Lcolour_ready
 adrp x9,focus_pointer
 ldr x9,[x9,:lo12:focus_pointer]
 add x10,x20,#0x1d8
 cmp x9,x10
 b.ne .Lcolour_ready
 mov w21,#1
 mov w25,#0xc247
 movk w25,#0xffff,lsl #16
.Lcolour_ready:
 mov w9,#0xff000000
 str w9,[sp,#144]
 mov x0,x20
 bl ui_root
 cbz x0,.Lfooter_theme_ready
 ldr x9,[x0]
 adrp x10,knob_fx_vptr
 add x10,x10,:lo12:knob_fx_vptr
 cmp x9,x10
 b.ne .Lfooter_theme_ready
 // Knob FX is an inverse amber overlay; native text is black by design.
 mov w9,#0xc247
 movk w9,#0xffff,lsl #16
 str w9,[sp,#144]
 mov w25,#0xff000000
 cbnz w23,.Lfooter_theme_ready
 mov w25,#0x7777
 movk w25,#0xff77,lsl #16
.Lfooter_theme_ready:
 ldr x0,[x20,#8]
 bl ui_name_alias
 cbz x0,.Lcopy_native_name
 mov x1,x0
 add x0,sp,#128
 bl string_utf8
 b .Ldisplay_name_ready
.Lcopy_native_name:
 add x0,sp,#128
 add x1,x20,#8
 bl string_copy
.Ldisplay_name_ready:
 // Opaque black, independent of the native grey slider fill colour.
 ldr w1,[sp,#144]
 mov x0,x19
 bl graphics_colour
 mov x0,x19
 mov w1,#0
 mov w2,#0
 mov w3,#106
 mov w4,#38
 bl fill_rect
 // Name row: 12px font, always visible, 98x12 logical pixels.
 add x0,sp,#96
 fmov s0,#12.0
 mov w1,#0
 bl font_create
 mov x0,x19
 add x1,sp,#96
 bl graphics_font
 add x0,sp,#96
 bl font_destroy
 mov x0,x19
 mov w1,w25
 bl graphics_colour
 mov x0,x19
 add x1,sp,#128
 mov w2,#14
 mov w3,#2
 mov w4,#78
 mov w5,#12
 mov w6,#0x24
 mov w7,#1
 fmov s0,#0.75
 bl draw_fitted_text
 // Preserve the native image used for unit/suffix symbols.
 add x0,x20,#0x2d8
 bl image_width
 mov w26,w0
 add x0,x20,#0x2d8
 bl image_height
 mov w22,w0
 cmp w26,#32
 b.hi .Lno_unit
 cmp w22,#18
 b.hi .Lno_unit
 cmp w22,#0
 b.le .Lno_unit
 b .Lunit_ready
.Lno_unit:
 mov w26,#0
 mov w22,#0
.Lunit_ready:
 str wzr,[sp,#140]            // Only Sample Trim Loop carries a right-edge lock.
 cbz w26,.Lunit_position_ready
 cbnz w27,.Lunit_position_ready
 mov x0,x20
 bl ui_root
 cbz x0,.Lunit_position_ready
 ldr x9,[x0]
 adrp x10,sample_vptr
 add x10,x10,:lo12:sample_vptr
 cmp x9,x10
 b.ne .Lunit_position_ready
 sub x9,x20,x0
 mov x10,#0x38f8
 cmp x9,x10
 b.ne .Lunit_position_ready
 mov w9,#1
 str w9,[sp,#140]
.Lunit_position_ready:
 // Centre the whole numeric text + unit bundle using native font metrics.
 add x0,sp,#96
 fmov s0,#16.0
 mov w1,#0
 bl font_create
 mov x0,x19
 add x1,sp,#96
 bl graphics_font
 add x0,sp,#96
 add x1,sp,#104
 bl font_text_width
 fcvtpu w28,s0
 mov w9,#98
 cbz w26,.Lmax_text
 sub w9,w9,w26
 ldr w10,[sp,#140]
 mov w11,#2
 mov w12,#6
 cmp w10,#0
 csel w10,w12,w11,ne
 sub w9,w9,w10
.Lmax_text:
 cmp w28,w9
 csel w28,w28,w9,lo
 cmp w28,#1
 csinc w28,w28,wzr,ge
 add x0,sp,#96
 bl font_destroy
 mov x0,x19
 mov w1,w25
 bl graphics_colour
 mov w9,w28
 cbz w26,.Lbundle_width
 ldr w10,[sp,#140]
 cbnz w10,.Lbundle_width
 add w9,w9,w26
 add w9,w9,#2
.Lbundle_width:
 mov w2,#106
 ldr w10,[sp,#140]
 cbz w10,.Lcentre_text
 sub w2,w2,w26
 sub w2,w2,#6
.Lcentre_text:
 sub w2,w2,w9
 asr w2,w2,#1
 str w2,[sp,#136]
 mov x0,x19
 add x1,sp,#104
 mov w3,#14
 mov w4,w28
 mov w5,#18
 mov w6,#0x24
 mov w7,#1
 fmov s0,#0.75
 bl draw_fitted_text
 cbz w26,.Lbar
 mov x0,x19
 add x1,x20,#0x2d8
 ldr w2,[sp,#136]
 add w2,w2,w28
 add w2,w2,#2
 ldr w9,[sp,#140]
 cbz w9,.Ldraw_unit
 mov w2,#102
 sub w2,w2,w26
.Ldraw_unit:
 mov w3,#18
 sub w3,w3,w22
 asr w3,w3,#1
 add w3,w3,#14
 mov w4,#0
 bl draw_image_at
.Lbar:
 cbnz w27,.Lfocus
 ldr w9,[x20,#0x240]          // Native ui::Slider render mode 3 = discrete choices.
 cmp w9,#3
 b.eq .Lsegments
.Lcontinuous_bar:
 mov x0,x19
 mov w1,#0x3b3b
 movk w1,#0xff3b,lsl #16
 bl graphics_colour
 mov x0,x19
 mov w1,#4
 mov w2,#35
 mov w3,#98
 mov w4,#2
 bl fill_rect
 mov x0,x19
 mov w1,w25
 bl graphics_colour
 mov w9,#98
 scvtf d0,w9
 fmul d0,d9,d0
 fcvtzs w3,d0
 cbz w3,.Lfocus
 mov x0,x19
 mov w1,#4
 mov w2,#35
 mov w4,#2
 bl fill_rect
 b .Lfocus
.Lsegments:
 mov x0,x20
 bl slider_range              // Same inclusive range source as stock mode 3.
 fsub d2,d1,d0
 fmov d3,#1.0
 fadd d2,d2,d3
 fcmp d2,d2
 b.vs .Lcontinuous_bar
 fcvtzs w22,d2
 cmp w22,#2
 b.lt .Lcontinuous_bar
 cmp w22,#25                 // At least two pixels plus a 2px gap; bounded paint work.
 b.gt .Lcontinuous_bar
 fsub d0,d8,d0
 fcvtzs w28,d0               // Current choice relative to the native minimum.
 mov w26,#0
.Lsegment_loop:
 mov w1,#0x3b3b
 movk w1,#0xff3b,lsl #16
 cmp w26,w28
 csel w1,w25,w1,eq
 mov x0,x19
 bl graphics_colour
 mov w9,#100
 mul w10,w26,w9
 udiv w10,w10,w22
 add w11,w26,#1
 mul w11,w11,w9
 udiv w11,w11,w22
 sub w3,w11,w10
 sub w3,w3,#2
 add w1,w10,#4
 mov x0,x19
 mov w2,#35
 mov w4,#2
 bl fill_rect
 add w26,w26,#1
 cmp w26,w22
 b.lt .Lsegment_loop
.Lfocus:
 cbz w21,.Lfinish
 mov x0,x19
 mov w1,w25
 bl graphics_colour
 // Native-style upper bracket, 2px thick with visible 32px side arms.
 mov x0,x19
 mov w1,#0
 mov w2,#0
 mov w3,#106
 mov w4,#2
 bl fill_rect
 mov x0,x19
 mov w1,#0
 mov w2,#0
 mov w3,#2
 mov w4,#32
 bl fill_rect
 mov x0,x19
 mov w1,#104
 mov w2,#0
 mov w3,#2
 mov w4,#32
 bl fill_rect
.Lfinish:
 add x0,sp,#128
 bl string_destroy
 add x0,sp,#104
 bl string_destroy
.Lrestore_frame:
 ldp x19,x20,[sp,#16]
 ldp x21,x22,[sp,#32]
 ldp x23,x24,[sp,#48]
 ldp x25,x26,[sp,#64]
 ldp d8,d9,[sp,#80]
 ldp x27,x28,[sp,#112]
 ldp x29,x30,[sp],#160
 autiasp                      // Authenticate the original renderer's PACIASP.
 ret
.size values_paint,.-values_paint

.balign 8
.global values_screen_offsets
.type values_screen_offsets,%object
values_screen_offsets:
 .quad ui_screen_0_delta // 16CompressorScreen
 .quad ui_screen_1_delta // 14CopyModeScreen
 .quad ui_screen_2_delta // 15EditTempoScreen
 .quad ui_screen_3_delta // 15FaderModeScreen
 .quad ui_screen_4_delta // 16FileSelectScreen
 .quad ui_screen_5_delta // 14FlexBeatScreen
 .quad ui_screen_6_delta // 14FxSelectScreen
 .quad ui_screen_7_delta // 17InputConfigScreen
 .quad ui_screen_8_delta // 19KnobFxOverlayScreen
 .quad ui_screen_9_delta // 17MassStorageScreen
 .quad ui_screen_10_delta // 16MidiConfigScreen
 .quad ui_screen_11_delta // 23NoteRepeatOverlayScreen
 .quad ui_screen_12_delta // 11PadFxScreen
 .quad ui_screen_13_delta // 21ProjectSaveLoadScreen
 .quad ui_screen_14_delta // 17ProjectSaveScreen
 .quad ui_screen_15_delta // 14QuestionScreen
 .quad ui_screen_16_delta // 17ResamplePadScreen
 .quad ui_screen_17_delta // 19ResourcesInfoScreen
 .quad ui_screen_18_delta // 16SampleModeScreen
 .quad ui_screen_19_delta // 22SampleRecordModeScreen
 .quad ui_screen_20_delta // 16SampleSaveScreen
 .quad ui_screen_21_delta // 18SequenceModeScreen
 .quad ui_screen_22_delta // 16SongExportScreen
 .quad ui_screen_23_delta // 10SongScreen
 .quad ui_screen_24_delta // 14StepEditScreen
 .quad ui_screen_25_delta // 17TimeCorrectScreen

.size values_screen_offsets,.-values_screen_offsets

#include "values_ui.S"
