%include "x86_abi_support.asm"

section .text
    global sym(vpx_reset_mmx_state)
sym(vpx_reset_mmx_state):
    emms
    ret
