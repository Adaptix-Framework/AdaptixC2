;
; chkstk_x64.asm - Stack probe for MSVC x64 builds
; MSVC x64 does not support inline assembly, so this must be a separate file.
; Assemble with: ml64 /c chkstk_x64.asm
;

PUBLIC _chkstk
PUBLIC __chkstk

_TEXT SEGMENT

_chkstk PROC
    push    rcx
    push    rax
    cmp     rax, 1000h
    lea     rcx, [rsp + 24]
    jb      done
probe_loop:
    sub     rcx, 1000h
    test    QWORD PTR [rcx], rcx
    sub     rax, 1000h
    cmp     rax, 1000h
    ja      probe_loop
done:
    sub     rcx, rax
    test    QWORD PTR [rcx], rcx
    pop     rax
    pop     rcx
    ret
_chkstk ENDP

__chkstk PROC
    ; Alias - some code paths call __chkstk instead of _chkstk
    jmp     _chkstk
__chkstk ENDP

_TEXT ENDS
END
