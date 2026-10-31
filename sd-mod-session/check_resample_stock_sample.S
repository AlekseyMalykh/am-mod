.text
.global stock_test_layer
.type stock_test_layer,%function
stock_test_layer:
 bti c
 mov x2,x8
 b stock_test_layer_c
.size stock_test_layer,.-stock_test_layer
.section .note.GNU-stack,"",%progbits
