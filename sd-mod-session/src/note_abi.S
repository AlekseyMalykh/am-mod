/* Adapt native AAPCS64 String result in x8 to an explicit C output argument. */
.arch armv8.3-a
.text
.global note_label_string
.hidden note_label_string
.type note_label_string,%function
note_label_string:
 bti c
 mov x8,x1
 mov w1,#0
 br x2
.size note_label_string,.-note_label_string
.section .note.GNU-stack,"",%progbits
