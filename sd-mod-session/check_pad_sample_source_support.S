.text
.global check_pad_source_factory
check_pad_source_factory:
 bti c
 mov x1,x8
 b native_reader_factory
.global check_pad_source_frame
check_pad_source_frame:
 bti c
 mov x2,x8
 b native_reader_frame
.section .note.GNU-stack,"",%progbits
