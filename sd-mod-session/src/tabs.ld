SECTIONS { . = 0x06d3b600; .text : { *(.text) } /DISCARD/ : { *(.note*) *(.comment*) *(.eh_frame*) } }
sample_vptr = 0x06e212a0;
slider_hw_vptr = 0x0706e800;
slider_vptr = 0x0706e5d0;
start_timer = 0x00ff9370;
stop_timer = 0x00ff1bc0;
repaint = 0x01149d70;
rebuild_focus = 0x015c1d10;
