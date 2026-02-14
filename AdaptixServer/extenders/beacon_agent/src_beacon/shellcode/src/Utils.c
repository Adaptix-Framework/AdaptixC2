/**
 * Utils.c - Replicated from Havoc Shellcode
 */

#include <Utils.h>
#include <Macro.h>

SEC( text, B ) UINT_PTR HashString( LPVOID String, UINT_PTR Length )
{
    PUCHAR	Ptr  = String;
    ULONG Hash;
    HIDDEN_CONST(Hash, 2685, 2696);  // Hash = 5381

    do
    {
        UCHAR character = *Ptr;

        if ( ! Length )
        {
            if ( !*Ptr ) break;
        }
        else
        {
            if ( (ULONG) ( Ptr - (PUCHAR)String ) >= Length ) break;
            if ( !*Ptr ) ++Ptr;
        }

        if ( character >= 'a' )
            character -= 0x20;

        Hash = ( ( Hash << 5 ) + Hash ) + character;
        ++Ptr;
    } while ( TRUE );

    return Hash;
}
