.text
.global check_pipeline_source
check_pipeline_source:
 bti c
 mov x1,x8
 b pipeline_source
.global check_pipeline_create
check_pipeline_create:
 bti c
 mov x5,x8
 b pipeline_create
.global check_pool_insert
check_pool_insert:
 bti c
 mov x2,x8
 b pool_insert
.section .note.GNU-stack,"",%progbits
