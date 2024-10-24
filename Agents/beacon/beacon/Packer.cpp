#include "Packer.h"

Packer::Packer()
{       
    this->original = MemAllocLocal(0);
    this->buffer   = this->original;
    this->size     = 0;
    this->index    = 0;
}

VOID Packer::Add64( ULONG64 value ) 
{
    this->buffer = ApiWin->LocalReAlloc( this->buffer, this->size + sizeof(ULONG64), LMEM_MOVEABLE );

    PUCHAR place = (PUCHAR) this->buffer + this->index;
    place[0] = (value >> 56) & 0xFF;
    place[1] = (value >> 48) & 0xFF;
    place[2] = (value >> 40) & 0xFF;
    place[3] = (value >> 32) & 0xFF;
    place[4] = (value >> 24) & 0xFF;
    place[5] = (value >> 16) & 0xFF;
    place[6] = (value >> 8 ) & 0xFF;
    place[7] = (value      ) & 0xFF;

    this->size  += sizeof(ULONG64);
    this->index += sizeof(ULONG64);
}

VOID Packer::Add32(ULONG value)
{
    this->buffer = ApiWin->LocalReAlloc(this->buffer, this->size + sizeof(ULONG), LMEM_MOVEABLE);

    PUCHAR place = (PUCHAR)this->buffer + this->index;
    place[0] = (value >> 24) & 0xFF;
    place[1] = (value >> 16) & 0xFF;
    place[2] = (value >> 8 ) & 0xFF;
    place[3] = (value      ) & 0xFF;

    this->size  += sizeof(ULONG);
    this->index += sizeof(ULONG);
}

VOID Packer::Add16(WORD value)
{
    this->buffer = ApiWin->LocalReAlloc(this->buffer, this->size + sizeof(WORD), LMEM_MOVEABLE);

    PUCHAR place = (PUCHAR)this->buffer + this->index;
    place[0] = (value >> 8) & 0xFF;
    place[1] = (value) & 0xFF;

    this->size  += sizeof(WORD);
    this->index += sizeof(WORD);
}

VOID Packer::Add8(BYTE value)
{
    this->buffer = ApiWin->LocalReAlloc(this->buffer, this->size + sizeof(BYTE), LMEM_MOVEABLE);

    ((PUCHAR)this->buffer + this->index)[0] = value;

    this->size  += 1;
    this->index += 1;
}

VOID Packer::AddBytes(PBYTE data, ULONG data_size)
{
    this->buffer = ApiWin->LocalReAlloc(this->buffer, this->size + data_size + sizeof(ULONG), LMEM_MOVEABLE);

    this->Add32(data_size);

    if (data_size) {
        memcpy((PUCHAR)this->buffer + this->index, data, data_size);

        this->size  += data_size;
        this->index += data_size;
    }
}

VOID Packer::AddStringA(LPSTR str)
{
    ULONG length = StrLenA(str);// +1;
    this->AddBytes((PBYTE)str, length);
}

PBYTE Packer::GetData()
{
    return (PBYTE)this->buffer;
}

ULONG Packer::GetDataSize()
{
    return this->index;
}