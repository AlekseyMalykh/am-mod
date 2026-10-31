.text
.global mono_convert_bridge
mono_convert_bridge:
 bti c
 mov x5,x8
 b mono_convert
.global mono_layer_bridge
mono_layer_bridge:
 bti c
 mov x2,x8
 b mono_layer
.global mono_name_bridge
mono_name_bridge:
 bti c
 mov x3,x8
 b mono_name
.global mono_add_bridge
mono_add_bridge:
 bti c
 mov x2,x8
 b mono_add
.section .note.GNU-stack,"",%progbits
