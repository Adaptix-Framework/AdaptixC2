extern Entry
global Start

STUB_SIZE_MARKER equ 0x13371337

section .text$A
    Start:
        push    rsi
        push    rdi
        mov     rsi, rsp
        and     rsp, 0FFFFFFFFFFFFFFF0h
        sub     rsp, 020h
        call    .get_current_pos

    .get_current_pos:
        pop     rcx
        and     rcx, 0FFFFFFFFFFFFF000h
        add     rcx, STUB_SIZE_MARKER
        call    Entry

        mov     rsp, rsi
        pop     rdi
        pop     rsi
        ret

section .text$G
    db 'ENDOFCODE'


