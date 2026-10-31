.text
.global check_unique_name
check_unique_name:
 bti c
 mov x3,x8
 b unique_name
.global check_empty_audio_source
check_empty_audio_source:
 bti c
 mov x1,x8
 b empty_audio_source
.global check_fail_sample_creation
check_fail_sample_creation:
 bti c
 mov x5,x8
 b fail_sample_creation
.section .note.GNU-stack,"",%progbits
