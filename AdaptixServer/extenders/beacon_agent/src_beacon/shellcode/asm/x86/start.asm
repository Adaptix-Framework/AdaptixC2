extern _Entry
global _Start

STUB_SIZE_MARKER equ 0x13381338

section .text$A
    _Start:
        push    esi
        push    edi
        mov     esi, esp
        and     esp, 0FFFFFFF0h
        call    get_eip
    get_eip:
        pop     edi
        and     edi, 0FFFFF000h
        add     edi, STUB_SIZE_MARKER
        push    edi
        call    _Entry
        add     esp, 4
        mov     esp, esi
        pop     edi
        pop     esi
        ret

section .text$G
    db 'ENDOFCODE'