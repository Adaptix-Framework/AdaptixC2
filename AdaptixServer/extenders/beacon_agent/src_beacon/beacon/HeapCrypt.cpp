#include "HeapCrypt.h"
#include "utils.h"

struct HEAP_NODE {
    PVOID      ptr;
    DWORD      size;
    HEAP_NODE* next;
};

static HEAP_NODE* g_heapHead = NULL;
static BYTE       g_heapKey[16];
static BOOL       g_heapInitialized = FALSE;

void HeapCryptInit()
{
    g_heapInitialized = TRUE;
}

void HeapTrack(PVOID ptr, DWORD size)
{
    if (!g_heapInitialized || !ptr || size == 0)
        return;

    HEAP_NODE* node = (HEAP_NODE*)ApiWin->LocalAlloc(LPTR, sizeof(HEAP_NODE));
    if (!node)
        return;

    node->ptr  = ptr;
    node->size = size;
    node->next = g_heapHead;
    g_heapHead = node;
}

void HeapUntrack(PVOID ptr)
{
    if (!g_heapInitialized || !ptr)
        return;

    HEAP_NODE** prev = &g_heapHead;
    HEAP_NODE*  cur  = g_heapHead;

    while (cur) {
        if (cur->ptr == ptr) {
            *prev = cur->next;
            ApiWin->LocalFree(cur);
            return;
        }
        prev = &cur->next;
        cur  = cur->next;
    }
}

static void XorBuffer(PBYTE buf, DWORD size, PBYTE key, DWORD keyLen)
{
    for (DWORD i = 0; i < size; i++)
        buf[i] ^= key[i % keyLen];
}

void HeapEncryptAll()
{
    for (int i = 0; i < 16; i++)
        g_heapKey[i] = (BYTE)(GenerateRandom32() & 0xFF);

    HEAP_NODE* cur = g_heapHead;
    while (cur) {
        if (cur->ptr && cur->size > 0)
            XorBuffer((PBYTE)cur->ptr, cur->size, g_heapKey, 16);
        cur = cur->next;
    }
}

void HeapDecryptAll()
{
    HEAP_NODE* cur = g_heapHead;
    while (cur) {
        if (cur->ptr && cur->size > 0)
            XorBuffer((PBYTE)cur->ptr, cur->size, g_heapKey, 16);
        cur = cur->next;
    }
}
