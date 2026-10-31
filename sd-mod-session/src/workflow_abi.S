.arch armv8.3-a
.text
.global workflow_optional
.hidden workflow_optional
.type workflow_optional,%function
workflow_optional:
 bti c
 mov x16,x0
 mov x0,x1
 mov w1,w2
 mov x8,x3
 br x16
.size workflow_optional,.-workflow_optional
.global workflow_directory
.hidden workflow_directory
.type workflow_directory,%function
workflow_directory:
 bti c
 mov x16,x0
 mov x0,x1
 mov x8,x2
 br x16
.size workflow_directory,.-workflow_directory
.section .note.GNU-stack,"",%progbits
