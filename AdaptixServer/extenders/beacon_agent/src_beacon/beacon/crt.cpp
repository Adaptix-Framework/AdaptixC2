#pragma once
#include <windows.h>

//
// Minimal CRT replacements for -nostdlib linking.
// Eliminates KERNEL32.dll and msvcrt.dll from the IAT.
//

//=============================================================================
// Compiler and toolchain detection
//=============================================================================

// Compiler family
#if defined(_MSC_VER) && !defined(__clang__)
#define COMPILER_MSVC  1
#define COMPILER_CLANG 0
#define COMPILER_GCC   0
#elif defined(__clang__)
#define COMPILER_MSVC  0
#define COMPILER_CLANG 1
#define COMPILER_GCC   0
#elif defined(__GNUC__) || defined(__MINGW32__) || defined(__MINGW64__)
#define COMPILER_MSVC  0
#define COMPILER_CLANG 0
#define COMPILER_GCC   1
#else
#error "Unsupported compiler: use MinGW, MSVC, Clang, or Zig"
#endif

// Toolchain detection (affects ABI and naming)
#if defined(__MINGW32__) || defined(__MINGW64__)
#define TOOLCHAIN_MINGW 1
#else
#define TOOLCHAIN_MINGW 0
#endif

// Zig uses Clang frontend, detected via __zig_cc__ or lack of other defines
#if defined(__zig_cc__) || (COMPILER_CLANG && !defined(_MSC_VER) && !TOOLCHAIN_MINGW)
#define TOOLCHAIN_ZIG 1
#else
#define TOOLCHAIN_ZIG 0
#endif

// GCC-compatible inline assembly (GCC, Clang, Zig)
#define ASM_GCC_STYLE (COMPILER_GCC || COMPILER_CLANG)

// MSVC-style intrinsics
#define USE_MSVC_INTRINSICS (COMPILER_MSVC || (COMPILER_CLANG && defined(_MSC_VER)))

#if USE_MSVC_INTRINSICS
#include <intrin.h>
#endif

// Naked function attribute
#if COMPILER_MSVC
#define NAKED_FUNC __declspec(naked)
#elif ASM_GCC_STYLE
#define NAKED_FUNC __attribute__((naked))
#endif

extern "C" {

    // memset and memcpy are already provided by ApiLoader.cpp

    // lstrcpynA replacement � ConnectorDNS uses it directly
    static char* __cdecl crt_lstrcpynA(char* dst, const char* src, int maxLen)
    {
        if (!dst) return dst;
        if (maxLen <= 0) return dst;
        int i = 0;
        while (i < maxLen - 1 && src[i]) {
            dst[i] = src[i];
            i++;
        }
        dst[i] = '\0';
        return dst;
    }

    // Provide the __imp_ symbol that the linker looks for
    char* (__cdecl* __imp_lstrcpynA)(char*, const char*, int) = crt_lstrcpynA;

    void* memmove(void* dest, const void* src, size_t n)
    {
        unsigned char* d = (unsigned char*)dest;
        const unsigned char* s = (const unsigned char*)src;
        if (d < s) {
            while (n--)
                *d++ = *s++;
        }
        else {
            d += n;
            s += n;
            while (n--)
                *--d = *--s;
        }
        return dest;
    }

    int memcmp(const void* s1, const void* s2, size_t n)
    {
        const unsigned char* a = (const unsigned char*)s1;
        const unsigned char* b = (const unsigned char*)s2;
        while (n--) {
            if (*a != *b)
                return *a - *b;
            a++;
            b++;
        }
        return 0;
    }

    size_t strlen(const char* s)
    {
        const char* p = s;
        while (*p)
            p++;
        return (size_t)(p - s);
    }

    //
    // Heap functions for miniz (DNS builds need malloc/free/realloc).
    // Resolves RtlAllocateHeap/RtlFreeHeap/RtlReAllocateHeap from ntdll
    // using PEB walk � zero IAT entries.
    //

    typedef void* (NTAPI* RtlAllocateHeap_t)(HANDLE, ULONG, SIZE_T);
    typedef BOOLEAN(NTAPI* RtlFreeHeap_t)(HANDLE, ULONG, PVOID);
    typedef void* (NTAPI* RtlReAllocateHeap_t)(HANDLE, ULONG, PVOID, SIZE_T);

    static RtlAllocateHeap_t   pRtlAllocateHeap = NULL;
    static RtlFreeHeap_t       pRtlFreeHeap = NULL;
    static RtlReAllocateHeap_t pRtlReAllocateHeap = NULL;
    static HANDLE              hProcessHeap = NULL;
    static int                 heapResolved = 0;

    static unsigned long crt_djb2(const char* str)
    {
        unsigned long h = 5381;
        while (*str)
            h = ((h << 5) + h) + (unsigned char)*str++;
        return h;
    }

#define HASH_RtlAllocateHeap    0xc0b381da
#define HASH_RtlFreeHeap        0x70ba71d7
#define HASH_RtlReAllocateHeap  0xbbc97911

    static void resolveHeapFunctions()
    {
        if (heapResolved)
            return;

        // Get PEB via TEB segment register
        BYTE* pPeb;
#ifdef _WIN64
#if USE_MSVC_INTRINSICS
        pPeb = (BYTE*)__readgsqword(0x60);
#elif ASM_GCC_STYLE
        asm volatile("mov %0, gs:[0x60]" : "=r"(pPeb));
#endif
        // PEB+0x30 = ProcessHeap
        hProcessHeap = *(HANDLE*)(pPeb + 0x30);
        // PEB+0x18 = Ldr (PPEB_LDR_DATA)
        BYTE* ldr = *(BYTE**)(pPeb + 0x18);
#else
#if USE_MSVC_INTRINSICS
        pPeb = (BYTE*)__readfsdword(0x30);
#elif ASM_GCC_STYLE
        asm volatile("mov %0, fs:[0x30]" : "=r"(pPeb));
#endif
        hProcessHeap = *(HANDLE*)(pPeb + 0x18);
        BYTE* ldr = *(BYTE**)(pPeb + 0x0C);
#endif

        // Ldr+0x10 (x64) or Ldr+0x0C (x86) = InLoadOrderModuleList
#ifdef _WIN64
        LIST_ENTRY* head = (LIST_ENTRY*)(ldr + 0x10);
#else
        LIST_ENTRY* head = (LIST_ENTRY*)(ldr + 0x0C);
#endif
        LIST_ENTRY* entry = head->Flink;

        while (entry != head) {
            // LDR_DATA_TABLE_ENTRY: InLoadOrderLinks at offset 0
            // BaseDllName at offset 0x58 (x64) or 0x2C (x86) � UNICODE_STRING
            // DllBase at offset 0x30 (x64) or 0x18 (x86)
            BYTE* mod = (BYTE*)entry;

#ifdef _WIN64
            PVOID dllBase = *(PVOID*)(mod + 0x30);
            USHORT nameLen = *(USHORT*)(mod + 0x58);
            WCHAR* nameBuf = *(WCHAR**)(mod + 0x58 + 0x08);
#else
            PVOID dllBase = *(PVOID*)(mod + 0x18);
            USHORT nameLen = *(USHORT*)(mod + 0x2C);
            WCHAR* nameBuf = *(WCHAR**)(mod + 0x2C + 0x04);
#endif

            // Check if this is ntdll.dll (case-insensitive, "ntdll.dll" = 9 chars)
            if (nameLen >= 9 * 2 && nameBuf) {
                WCHAR c0 = nameBuf[0] | 0x20;
                WCHAR c1 = nameBuf[1] | 0x20;
                WCHAR c2 = nameBuf[2] | 0x20;
                WCHAR c3 = nameBuf[3] | 0x20;
                WCHAR c4 = nameBuf[4] | 0x20;

                if (c0 == L'n' && c1 == L't' && c2 == L'd' && c3 == L'l' && c4 == L'l' && nameBuf[5] == L'.') {
                    PBYTE base = (PBYTE)dllBase;
                    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)base;
                    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS)(base + dos->e_lfanew);
                    PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)(base +
                        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

                    DWORD* names = (DWORD*)(base + exports->AddressOfNames);
                    WORD* ordinals = (WORD*)(base + exports->AddressOfNameOrdinals);
                    DWORD* functions = (DWORD*)(base + exports->AddressOfFunctions);

                    for (DWORD i = 0; i < exports->NumberOfNames; i++) {
                        char* fname = (char*)(base + names[i]);
                        unsigned long h = crt_djb2(fname);

                        if (h == HASH_RtlAllocateHeap && !pRtlAllocateHeap)
                            pRtlAllocateHeap = (RtlAllocateHeap_t)(base + functions[ordinals[i]]);
                        else if (h == HASH_RtlFreeHeap && !pRtlFreeHeap)
                            pRtlFreeHeap = (RtlFreeHeap_t)(base + functions[ordinals[i]]);
                        else if (h == HASH_RtlReAllocateHeap && !pRtlReAllocateHeap)
                            pRtlReAllocateHeap = (RtlReAllocateHeap_t)(base + functions[ordinals[i]]);

                        if (pRtlAllocateHeap && pRtlFreeHeap && pRtlReAllocateHeap)
                            break;
                    }
                    break;
                }
            }
            entry = entry->Flink;
        }

        heapResolved = 1;
    }

    void* malloc(size_t size)
    {
        if (!heapResolved)
            resolveHeapFunctions();
        if (!pRtlAllocateHeap || !hProcessHeap)
            return NULL;
        return pRtlAllocateHeap(hProcessHeap, 0, size);
    }

    void free(void* ptr)
    {
        if (!ptr)
            return;
        if (!heapResolved)
            resolveHeapFunctions();
        if (pRtlFreeHeap && hProcessHeap)
            pRtlFreeHeap(hProcessHeap, 0, ptr);
    }

    void* calloc(size_t num, size_t size)
    {
        if (!heapResolved)
            resolveHeapFunctions();
        if (!pRtlAllocateHeap || !hProcessHeap)
            return NULL;
        size_t total = num * size;
        return pRtlAllocateHeap(hProcessHeap, HEAP_ZERO_MEMORY, total);
    }

    void* realloc(void* ptr, size_t size)
    {
        if (!heapResolved)
            resolveHeapFunctions();
        if (!hProcessHeap)
            return NULL;
        if (!ptr)
            return pRtlAllocateHeap(hProcessHeap, 0, size);
        if (size == 0) {
            pRtlFreeHeap(hProcessHeap, 0, ptr);
            return NULL;
        }
        return pRtlReAllocateHeap(hProcessHeap, 0, ptr, size);
    }

    //=============================================================================
    // Stack probe functions (__chkstk / ___chkstk_ms)
    // Required when local variables exceed page size (4KB)
    //=============================================================================

#if ASM_GCC_STYLE
// GCC/Clang/MinGW/Zig: emit __main stub for static constructor init
    void __main(void) {}

#ifdef _WIN64
    NAKED_FUNC void ___chkstk_ms(void)
    {
        asm volatile(
            "push   rcx\n"
            "push   rax\n"
            "cmp    rax, 0x1000\n"
            "lea    rcx, [rsp + 24]\n"
            "jb     2f\n"
            "1:\n"
            "sub    rcx, 0x1000\n"
            "test   [rcx], rcx\n"
            "sub    rax, 0x1000\n"
            "cmp    rax, 0x1000\n"
            "ja     1b\n"
            "2:\n"
            "sub    rcx, rax\n"
            "test   [rcx], rcx\n"
            "pop    rax\n"
            "pop    rcx\n"
            "ret\n"
            );
    }

    // Clang on Windows may call __chkstk instead of ___chkstk_ms
    NAKED_FUNC void __chkstk(void)
    {
        asm volatile(
            "push   rcx\n"
            "push   rax\n"
            "cmp    rax, 0x1000\n"
            "lea    rcx, [rsp + 24]\n"
            "jb     2f\n"
            "1:\n"
            "sub    rcx, 0x1000\n"
            "test   [rcx], rcx\n"
            "sub    rax, 0x1000\n"
            "cmp    rax, 0x1000\n"
            "ja     1b\n"
            "2:\n"
            "sub    rcx, rax\n"
            "test   [rcx], rcx\n"
            "pop    rax\n"
            "pop    rcx\n"
            "ret\n"
            );
    }
#else // x86
    NAKED_FUNC void ___chkstk_ms(void)
    {
        asm volatile(
            "push   ecx\n"
            "push   eax\n"
            "cmp    eax, 0x1000\n"
            "lea    ecx, [esp + 12]\n"
            "jb     2f\n"
            "1:\n"
            "sub    ecx, 0x1000\n"
            "test   [ecx], ecx\n"
            "sub    eax, 0x1000\n"
            "cmp    eax, 0x1000\n"
            "ja     1b\n"
            "2:\n"
            "sub    ecx, eax\n"
            "test   [ecx], ecx\n"
            "pop    eax\n"
            "pop    ecx\n"
            "ret\n"
            );
    }

    NAKED_FUNC void __chkstk(void)
    {
        asm volatile(
            "push   ecx\n"
            "push   eax\n"
            "cmp    eax, 0x1000\n"
            "lea    ecx, [esp + 12]\n"
            "jb     2f\n"
            "1:\n"
            "sub    ecx, 0x1000\n"
            "test   [ecx], ecx\n"
            "sub    eax, 0x1000\n"
            "cmp    eax, 0x1000\n"
            "ja     1b\n"
            "2:\n"
            "sub    ecx, eax\n"
            "test   [ecx], ecx\n"
            "pop    eax\n"
            "pop    ecx\n"
            "ret\n"
            );
    }

    // Some linkers look for _alloca or _chkstk aliases
    NAKED_FUNC void _chkstk(void)
    {
        asm volatile("jmp ___chkstk_ms\n");
    }

    NAKED_FUNC void _alloca(void)
    {
        asm volatile("jmp ___chkstk_ms\n");
    }
#endif // _WIN64

#elif COMPILER_MSVC
// MSVC: x86 uses inline asm, x64 requires external chkstk_x64.asm
#if !defined(_WIN64)
    NAKED_FUNC void __cdecl _chkstk(void)
    {
        __asm {
            push    ecx
            push    eax
            cmp     eax, 1000h
            lea     ecx, [esp + 12]
            jb      done
            probe_loop :
            sub     ecx, 1000h
                test[ecx], ecx
                sub     eax, 1000h
                cmp     eax, 1000h
                ja      probe_loop
                done :
            sub     ecx, eax
                test[ecx], ecx
                pop     eax
                pop     ecx
                ret
        }
    }
#endif // !_WIN64
#endif // COMPILER_MSVC

} // extern "C"
