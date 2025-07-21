#include "Packer.h"

Packer::Packer()
{       
    this->buffer = (BYTE*) MemAllocLocal(4);
    this->size   = 4;
    this->index  = 0;
}

Packer::Packer(BYTE* buffer, ULONG size)
{
    this->buffer   = buffer;
    this->size     = size;
    this->index    = 0;
}

Packer::~Packer(){}

VOID Packer::Set32(ULONG index, ULONG value)
{
    PUCHAR place = this->buffer + index;
    place[0] = (value >> 24) & 0xFF;
    place[1] = (value >> 16) & 0xFF;
    place[2] = (value >> 8 ) & 0xFF;
    place[3] = (value      ) & 0xFF;
}

VOID Packer::Pack64( ULONG64 value ) 
{
    this->buffer = (BYTE*)MemReallocLocal( this->buffer, this->size + sizeof(ULONG64) );

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

VOID Packer::Pack32(ULONG value)
{
    this->buffer = (BYTE*)MemReallocLocal(this->buffer, this->size + sizeof(ULONG));

    PUCHAR place = this->buffer + this->index;
    place[0] = (value >> 24) & 0xFF;
    place[1] = (value >> 16) & 0xFF;
    place[2] = (value >> 8 ) & 0xFF;
    place[3] = (value      ) & 0xFF;

    this->size  += sizeof(ULONG);
    this->index += sizeof(ULONG);
}

VOID Packer::Pack16(WORD value)
{
    this->buffer = (BYTE*)MemReallocLocal(this->buffer, this->size + sizeof(WORD));

    PUCHAR place = this->buffer + this->index;
    place[0] = (value >> 8) & 0xFF;
    place[1] = (value     ) & 0xFF;

    this->size  += sizeof(WORD);
    this->index += sizeof(WORD);
}

VOID Packer::Pack8(BYTE value)
{
    this->buffer = (BYTE*)MemReallocLocal(this->buffer, this->size + sizeof(BYTE));

    (this->buffer + this->index)[0] = value;

    this->size  += 1;
    this->index += 1;
}

VOID Packer::PackBytes(PBYTE data, ULONG data_size)
{
    this->Pack32(data_size);

    if (data_size) {
 
        this->buffer = (BYTE*)MemReallocLocal(this->buffer, this->size + data_size);

        memcpy( this->buffer + this->index, data, data_size);

        this->size  += data_size;
        this->index += data_size;
    }
}

VOID Packer::PackStringA(LPSTR str)
{
    ULONG length = StrLenA(str); // +1;
    this->PackBytes( (BYTE*) str, length);
}

VOID Packer::PackFlatBytes(PBYTE data, ULONG data_size)
{
    if (data_size) {

        this->buffer = (BYTE*)MemReallocLocal(this->buffer, this->size + data_size);

        memcpy(this->buffer + this->index, data, data_size);

        this->size += data_size;
        this->index += data_size;
    }
}


PBYTE Packer::data()
{
    return this->buffer;
}

ULONG Packer::datasize()
{
    return this->index;
}

VOID Packer::Clear(BOOL renew)
{
    MemFreeLocal((LPVOID*)&this->buffer, this->size);
    this->index = 0;
    this->size = 0;
    if (renew) {
        this->size = 4;
        this->buffer = (BYTE*)MemAllocLocal(4);
    }
}

BYTE Packer::Unpack8()
{
    ULONG value = 0;
    if (this->size - this->index < 1)
        return 0;

    memcpy(&value, this->buffer + this->index, 1);

    this->index += 1;

    return value;
}

ULONG Packer::Unpack32()
{
    ULONG value = 0;
    if ( this->size - this->index < 4 )
        return 0;

    memcpy(&value, this->buffer + this->index, 4);

    this->index += 4;

    return value;
}

BYTE* Packer::UnpackBytes(ULONG* str_size)
{
    *str_size = this->Unpack32();

    if ( this->size - this->index < *str_size )
        return NULL;

    if (*str_size == 0)
        return NULL;

    BYTE* out = this->buffer + this->index;
    this->index += *str_size;

    return out;
}

BYTE* Packer::UnpackBytesCopy(ULONG* str_size)
{
    *str_size = this->Unpack32();

    if (this->size - this->index < *str_size)
        return NULL;

    if (*str_size == 0)
        return NULL;

    BYTE* out = (PBYTE) MemAllocLocal(*str_size);
    memcpy(out, this->buffer + this->index, *str_size);

    this->index += *str_size;

    return out;
}