.arch armv8.3-a
.text
/* ValueSource::getValue returns a nontrivial JUCE var through x8. */
.global check_header_value_get
.type check_header_value_get,%function
check_header_value_get:
 bti c
 mov x1,x0
 mov x0,x8
 b check_header_value_fill
.size check_header_value_get,.-check_header_value_get
.section .note.GNU-stack,"",%progbits
