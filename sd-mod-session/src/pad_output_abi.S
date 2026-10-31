.text
.global pad_output_frame
.hidden pad_output_frame
.type pad_output_frame,%function
pad_output_frame:
 bti c
 mov x2,x8
 b pad_output_frame_result
.size pad_output_frame,.-pad_output_frame
.section .note.GNU-stack,"",%progbits
