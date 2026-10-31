
.text
.global check_pcm_frame
check_pcm_frame:
 bti c
 mov x2,x8
 b pcm_frame
.global check_read_frame
check_read_frame:
 bti c
 mov x8,x2
 br x3

.section .note.GNU-stack,"",%progbits
