.text
/* Owned native read-stream factory uses C++ x8 result ABI. */
.global pad_source_result
.hidden pad_source_result
.type pad_source_result,%function
pad_source_result:
 bti c
 mov x8,x1
 br x2
.size pad_source_result,.-pad_source_result
.global pad_source_read_block
.hidden pad_source_read_block
.type pad_source_read_block,%function
pad_source_read_block:
 bti c
 mov x8,x2
 br x3
.size pad_source_read_block,.-pad_source_read_block
.global pad_source_layer_sample
.hidden pad_source_layer_sample
.type pad_source_layer_sample,%function
pad_source_layer_sample:
 bti c
 mov x8,x2
 br x3
.size pad_source_layer_sample,.-pad_source_layer_sample
.section .note.GNU-stack,"",%progbits
