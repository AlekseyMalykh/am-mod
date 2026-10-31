.global ui_shift_pairs
.type ui_shift_pairs,%object
.balign 8
ui_shift_pairs:
 .quad ui_pair_0_delta
 .word 0x2bb8,0x2f08,0,0 // Sample Trim K1
 .quad ui_pair_1_delta
 .word 0x3258,0x35a8,1,0 // Sample Trim K2
 .quad ui_pair_2_delta
 .word 0x38f8,0x3c48,2,0 // Sample Trim K3
 .quad ui_pair_3_delta
 .word 0x4638,0x4cd8,1,0 // Sample Mix empty K2
 .quad ui_pair_4_delta
 .word 0x6758,0x6aa8,2,0 // Sample Warp/Beats
 .quad ui_pair_5_delta
 .word 0x7148,0x7498,1,0 // Sample MuteGrp/PadLink
 .quad ui_pair_6_delta
 .word 0x1eb8,0x2208,2,0 // Compressor Amount/Boost
 .quad ui_pair_7_delta
 .word 0x2788,0x2a90,1,0 // Sequence Q/TimeSig
 .quad ui_pair_8_delta
 .word 0x2d98,0x30e8,2,0 // Sequence Swing/Metro
 .quad ui_pair_9_delta
 .word 0x15b0,0x18b8,1,0 // StepEdit Q/TimeSig
 .quad ui_pair_10_delta
 .word 0x7d8,0x11c8,0,0 // FX Select K1
 .quad ui_pair_11_delta
 .word 0xb28,0x1518,1,0 // FX Select K2
 .quad ui_pair_12_delta
 .word 0xe78,0x1868,2,0 // FX Select K3
 .quad ui_pair_13_delta
 .word 0xae0,0x14d0,0,0 // Input Config K1
 .quad ui_pair_14_delta
 .word 0xe30,0x1820,1,0 // Input Config K2
 .quad ui_pair_15_delta
 .word 0x1180,0x1b70,2,0 // Input Config K3
 .quad ui_pair_16_delta
 .word 0x298,0xc88,0,0 // Knob FX K1
 .quad ui_pair_17_delta
 .word 0x5e8,0xfd8,1,0 // Knob FX K2
 .quad ui_pair_18_delta
 .word 0x938,0x1328,2,0 // Knob FX K3
.size ui_shift_pairs,.-ui_shift_pairs
