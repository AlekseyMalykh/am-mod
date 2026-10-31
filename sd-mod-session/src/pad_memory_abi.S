.text
.global pad_memory_factory
pad_memory_factory:
 bti c
 mov x8,x3
 br x4
.global pad_memory_pool_add
pad_memory_pool_add:
 bti c
 mov x8,x2
 br x3
.section .note.GNU-stack,"",%progbits
