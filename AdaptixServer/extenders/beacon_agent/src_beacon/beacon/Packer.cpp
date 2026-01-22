#include "Packer.h"

void* Packer::operator new(size_t sz) 
{
    void* p = MemAllocLocal(sz);
    return p;
}

void Packer::operator delete(void* p) noexcept 
{
    MemFreeLocal(&p, sizeof(Packer));
}

Packer::Packer()
{       
    this->capacity = 4096;
    this->buffer = (BYTE*) MemAllocLocal(this->capacity);
    this->size   = 0;
    this->index  = 0;
}

Packer::Packer(BYTE* buffer, ULONG size)
{
    this->buffer   = buffer;
    this->size     = size;
    this->capacity = size;
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

VOID Packer::EnsureCapacity(ULONG needed)
{
    if (this->index + needed > this->capacity) {
        ULONG new_cap = this->capacity ? (this->capacity * 2) : 4096;
        if (new_cap < this->index + needed)
            new_cap = this->index + needed + 1024;

        this->buffer = (BYTE*)MemReallocLocal(this->buffer, new_cap);
        this->capacity = new_cap;
    }
}

VOID Packer::Pack64( ULONG64 value ) 
{
    this->EnsureCapacity(sizeof(ULONG64));


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
    this->EnsureCapacity(sizeof(ULONG));

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
    this->EnsureCapacity(sizeof(WORD));

    PUCHAR place = this->buffer + this->index;
    place[0] = (value >> 8) & 0xFF;
    place[1] = (value     ) & 0xFF;

    this->size  += sizeof(WORD);
    this->index += sizeof(WORD);
}

VOID Packer::Pack8(BYTE value)
{
    this->EnsureCapacity(sizeof(BYTE));

    (this->buffer + this->index)[0] = value;

    this->size  += 1;
    this->index += 1;
}

VOID Packer::PackBytes(PBYTE data, ULONG data_size)
{
    this->Pack32(data_size);

    if (data_size) {
        EnsureCapacity(data_size);
        memcpy(this->buffer + this->index, data, data_size);
        this->index += data_size;
        this->size = this->index;
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
        EnsureCapacity(data_size);
        memcpy(this->buffer + this->index, data, data_size);
        this->index += data_size;
        this->size = this->index;
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
    MemFreeLocal((LPVOID*)&this->buffer, this->capacity);
    this->index = 0;
    this->size = 0;
    this->capacity = 0;
    if (renew) {
        this->capacity = 4096;
        this->buffer = (BYTE*)MemAllocLocal(this->capacity);
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