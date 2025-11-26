// Ref: https://github.com/RIscRIpt/pecoff

package defwin

// Constant offsets within COFF files
const (
	OFFSET_COFF_FILE_HEADER = 0 //MS COFF always begins with FileHeader
)

// Sizes
const (
	SIZEOF_IMAGE_FILE_HEADER    = 20
	SIZEOF_IMAGE_SECTION_HEADER = 40
	SIZEOF_IMAGE_RELOCATION     = 10
	SIZEOF_IMAGE_SYMBOL         = 18
)

// COFF types
type (
	// COFF File Header (presented in both Object and Image files)
	FileHeader struct {
		Machine              uint16
		NumberOfSections     uint16
		TimeDateStamp        uint32
		PointerToSymbolTable uint32
		NumberOfSymbols      uint32
		SizeOfOptionalHeader uint16
		Characteristics      uint16
	}

	SectionHeader struct {
		Name                 [8]uint8
		VirtualSize          uint32
		VirtualAddress       uint32
		SizeOfRawData        uint32
		PointerToRawData     uint32
		PointerToRelocations uint32
		PointerToLineNumbers uint32
		NumberOfRelocations  uint16
		NumberOfLineNumbers  uint16
		Characteristics      uint32
	}

	Relocation struct {
		VirtualAddress   uint32
		SymbolTableIndex uint32
		Type             uint16
	}

	Symbol struct {
		// The name of the symbol.
		// If the name is longer than 8 bytes, first 4 bytes are set to zero
		// and the remaining 4 represent an offset into the string table.
		Name [8]uint8

		// The value that is associated with the symbol.
		// The interpretation of this field depends on SectionNumber and StorageClass.
		// A typical meaning is the relocatable address.
		Value uint32

		// The signed integer that identifies the section,
		// using a one-based index into the section table.
		SectionNumber int16

		// A number that represents type.
		Type SymbolType

		// An enumerated value that represents storage class.
		StorageClass uint8

		// The number of auxiliary symbol table entries that follow this record.
		NumberOfAuxSymbols uint8
	}

	SymbolType struct {
		Base    byte
		Derived byte
	}
)

// Image file machine types
const (
	IMAGE_FILE_MACHINE_UNKNOWN   uint16 = 0x0
	IMAGE_FILE_MACHINE_AM33      uint16 = 0x1d3
	IMAGE_FILE_MACHINE_AMD64     uint16 = 0x8664
	IMAGE_FILE_MACHINE_ARM       uint16 = 0x1c0
	IMAGE_FILE_MACHINE_EBC       uint16 = 0xebc
	IMAGE_FILE_MACHINE_I386      uint16 = 0x14c
	IMAGE_FILE_MACHINE_IA64      uint16 = 0x200
	IMAGE_FILE_MACHINE_M32R      uint16 = 0x9041
	IMAGE_FILE_MACHINE_MIPS16    uint16 = 0x266
	IMAGE_FILE_MACHINE_MIPSFPU   uint16 = 0x366
	IMAGE_FILE_MACHINE_MIPSFPU16 uint16 = 0x466
	IMAGE_FILE_MACHINE_POWERPC   uint16 = 0x1f0
	IMAGE_FILE_MACHINE_POWERPCFP uint16 = 0x1f1
	IMAGE_FILE_MACHINE_R4000     uint16 = 0x166
	IMAGE_FILE_MACHINE_SH3       uint16 = 0x1a2
	IMAGE_FILE_MACHINE_SH3DSP    uint16 = 0x1a3
	IMAGE_FILE_MACHINE_SH4       uint16 = 0x1a6
	IMAGE_FILE_MACHINE_SH5       uint16 = 0x1a8
	IMAGE_FILE_MACHINE_THUMB     uint16 = 0x1c2
	IMAGE_FILE_MACHINE_WCEMIPSV2 uint16 = 0x169
)

var MAP_IMAGE_FILE_MACHINE = map[uint16]string{
	IMAGE_FILE_MACHINE_UNKNOWN:   "UNKNOWN",
	IMAGE_FILE_MACHINE_AM33:      "AM33",
	IMAGE_FILE_MACHINE_AMD64:     "AMD64",
	IMAGE_FILE_MACHINE_ARM:       "ARM",
	IMAGE_FILE_MACHINE_EBC:       "EBC",
	IMAGE_FILE_MACHINE_I386:      "I386",
	IMAGE_FILE_MACHINE_IA64:      "IA64",
	IMAGE_FILE_MACHINE_M32R:      "M32R",
	IMAGE_FILE_MACHINE_MIPS16:    "MIPS16",
	IMAGE_FILE_MACHINE_MIPSFPU:   "MIPSFPU",
	IMAGE_FILE_MACHINE_MIPSFPU16: "MIPSFPU16",
	IMAGE_FILE_MACHINE_POWERPC:   "POWERPC",
	IMAGE_FILE_MACHINE_POWERPCFP: "POWERPCFP",
	IMAGE_FILE_MACHINE_R4000:     "R4000",
	IMAGE_FILE_MACHINE_SH3:       "SH3",
	IMAGE_FILE_MACHINE_SH3DSP:    "SH3DSP",
	IMAGE_FILE_MACHINE_SH4:       "SH4",
	IMAGE_FILE_MACHINE_SH5:       "SH5",
	IMAGE_FILE_MACHINE_THUMB:     "THUMB",
	IMAGE_FILE_MACHINE_WCEMIPSV2: "WCEMIPSV2",
}

// File header characteristics
const (
	IMAGE_FILE_RELOCS_STRIPPED         = 0x0001 // Relocation info stripped from file.
	IMAGE_FILE_EXECUTABLE_IMAGE        = 0x0002 // File is executable  (i.e. no unresolved external references).
	IMAGE_FILE_LINE_NUMS_STRIPPED      = 0x0004 // Line nunbers stripped from file.
	IMAGE_FILE_LOCAL_SYMS_STRIPPED     = 0x0008 // Local symbols stripped from file.
	IMAGE_FILE_AGGRESIVE_WS_TRIM       = 0x0010 // Aggressively trim working set
	IMAGE_FILE_LARGE_ADDRESS_AWARE     = 0x0020 // App can handle >2gb addresses
	IMAGE_FILE_BYTES_REVERSED_LO       = 0x0080 // Bytes of machine word are reversed.
	IMAGE_FILE_32BIT_MACHINE           = 0x0100 // 32 bit word machine.
	IMAGE_FILE_DEBUG_STRIPPED          = 0x0200 // Debugging info stripped from file in .DBG file
	IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP = 0x0400 // If Image is on removable media, copy and run from the swap file.
	IMAGE_FILE_NET_RUN_FROM_SWAP       = 0x0800 // If Image is on Net, copy and run from the swap file.
	IMAGE_FILE_SYSTEM                  = 0x1000 // System File.
	IMAGE_FILE_DLL                     = 0x2000 // File is a DLL.
	IMAGE_FILE_UP_SYSTEM_ONLY          = 0x4000 // File should only be run on a UP machine
	IMAGE_FILE_BYTES_REVERSED_HI       = 0x8000 // Bytes of machine word are reversed.
)

// Section Characteristics.
const (
	_IMAGE_SCN_TYPE_REG    = 0x00000000 // Reserved.
	_IMAGE_SCN_TYPE_DSECT  = 0x00000001 // Reserved.
	_IMAGE_SCN_TYPE_NOLOAD = 0x00000002 // Reserved.
	_IMAGE_SCN_TYPE_GROUP  = 0x00000004 // Reserved.
	IMAGE_SCN_TYPE_NO_PAD  = 0x00000008 // Reserved.
	_IMAGE_SCN_TYPE_COPY   = 0x00000010 // Reserved.

	IMAGE_SCN_CNT_CODE               = 0x00000020 // Section contains code.
	IMAGE_SCN_CNT_INITIALIZED_DATA   = 0x00000040 // Section contains initialized data.
	IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x00000080 // Section contains uninitialized data.

	IMAGE_SCN_LNK_OTHER  = 0x00000100 // Reserved.
	IMAGE_SCN_LNK_INFO   = 0x00000200 // Section contains comments or some other type of information.
	_IMAGE_SCN_TYPE_OVER = 0x00000400 // Reserved.
	IMAGE_SCN_LNK_REMOVE = 0x00000800 // Section contents will not become part of image.
	IMAGE_SCN_LNK_COMDAT = 0x00001000 // Section contents comdat.

	_IMAGE_SCN_RESERVED_00002000 = 0x00002000 // Reserved.
	_IMAGE_SCN_MEM_PROTECTED     = 0x00004000 // Obsolete
	IMAGE_SCN_NO_DEFER_SPEC_EXC  = 0x00004000 // Reset speculative exceptions handling bits in the TLB entries for this section.
	IMAGE_SCN_GPREL              = 0x00008000 // Section content can be accessed relative to GP
	IMAGE_SCN_MEM_FARDATA        = 0x00008000
	_IMAGE_SCN_MEM_SYSHEAP       = 0x00010000 // Obsolete
	IMAGE_SCN_MEM_PURGEABLE      = 0x00020000
	IMAGE_SCN_MEM_16BIT          = 0x00020000
	IMAGE_SCN_MEM_LOCKED         = 0x00040000
	IMAGE_SCN_MEM_PRELOAD        = 0x00080000

	IMAGE_SCN_ALIGN_1BYTES    = 0x00100000 //
	IMAGE_SCN_ALIGN_2BYTES    = 0x00200000 //
	IMAGE_SCN_ALIGN_4BYTES    = 0x00300000 //
	IMAGE_SCN_ALIGN_8BYTES    = 0x00400000 //
	IMAGE_SCN_ALIGN_16BYTES   = 0x00500000 // Default alignment if no others are specified.
	IMAGE_SCN_ALIGN_32BYTES   = 0x00600000 //
	IMAGE_SCN_ALIGN_64BYTES   = 0x00700000 //
	IMAGE_SCN_ALIGN_128BYTES  = 0x00800000 //
	IMAGE_SCN_ALIGN_256BYTES  = 0x00900000 //
	IMAGE_SCN_ALIGN_512BYTES  = 0x00A00000 //
	IMAGE_SCN_ALIGN_1024BYTES = 0x00B00000 //
	IMAGE_SCN_ALIGN_2048BYTES = 0x00C00000 //
	IMAGE_SCN_ALIGN_4096BYTES = 0x00D00000 //
	IMAGE_SCN_ALIGN_8192BYTES = 0x00E00000 //
	IMAGE_SCN_ALIGN_MASK      = 0x00F00000

	IMAGE_SCN_LNK_NRELOC_OVFL = 0x01000000 // Section contains extended relocations.
	IMAGE_SCN_MEM_DISCARDABLE = 0x02000000 // Section can be discarded.
	IMAGE_SCN_MEM_NOT_CACHED  = 0x04000000 // Section is not cachable.
	IMAGE_SCN_MEM_NOT_PAGED   = 0x08000000 // Section is not pageable.
	IMAGE_SCN_MEM_SHARED      = 0x10000000 // Section is shareable.
	IMAGE_SCN_MEM_EXECUTE     = 0x20000000 // Section is executable.
	IMAGE_SCN_MEM_READ        = 0x40000000 // Section is readable.
	IMAGE_SCN_MEM_WRITE       = 0x80000000 // Section is writeable.
)

// Section I386 relocations
const (
	IMAGE_REL_I386_ABSOLUTE = 0x0000 // Reference is absolute, no relocation is necessary
	IMAGE_REL_I386_DIR16    = 0x0001 // Direct 16-bit reference to the symbols virtual address
	IMAGE_REL_I386_REL16    = 0x0002 // PC-relative 16-bit reference to the symbols virtual address
	IMAGE_REL_I386_DIR32    = 0x0006 // Direct 32-bit reference to the symbols virtual address
	IMAGE_REL_I386_DIR32NB  = 0x0007 // Direct 32-bit reference to the symbols virtual address, base not included
	IMAGE_REL_I386_SEG12    = 0x0009 // Direct 16-bit reference to the segment-selector bits of a 32-bit virtual address
	IMAGE_REL_I386_SECTION  = 0x000A
	IMAGE_REL_I386_SECREL   = 0x000B
	IMAGE_REL_I386_TOKEN    = 0x000C // clr token
	IMAGE_REL_I386_SECREL7  = 0x000D // 7 bit offset from base of section containing target
	IMAGE_REL_I386_REL32    = 0x0014 // PC-relative 32-bit reference to the symbols virtual address
)

var MAP_IMAGE_REL_I386 = map[uint16]string{
	IMAGE_REL_I386_ABSOLUTE: "ABSOLUTE",
	IMAGE_REL_I386_DIR16:    "DIR16",
	IMAGE_REL_I386_REL16:    "REL16",
	IMAGE_REL_I386_DIR32:    "DIR32",
	IMAGE_REL_I386_DIR32NB:  "DIR32NB",
	IMAGE_REL_I386_SEG12:    "SEG12",
	IMAGE_REL_I386_SECTION:  "SECTION",
	IMAGE_REL_I386_SECREL:   "SECREL",
	IMAGE_REL_I386_TOKEN:    "TOKEN",
	IMAGE_REL_I386_SECREL7:  "SECREL7",
	IMAGE_REL_I386_REL32:    "REL32",
}

// Section AMD64 relocations
const (
	IMAGE_REL_AMD64_ABSOLUTE = 0x0000 // Reference is absolute, no relocation is necessary
	IMAGE_REL_AMD64_ADDR64   = 0x0001 // 64-bit address (VA).
	IMAGE_REL_AMD64_ADDR32   = 0x0002 // 32-bit address (VA).
	IMAGE_REL_AMD64_ADDR32NB = 0x0003 // 32-bit address w/o image base (RVA).
	IMAGE_REL_AMD64_REL32    = 0x0004 // 32-bit relative address from byte following reloc
	IMAGE_REL_AMD64_REL32_1  = 0x0005 // 32-bit relative address from byte distance 1 from reloc
	IMAGE_REL_AMD64_REL32_2  = 0x0006 // 32-bit relative address from byte distance 2 from reloc
	IMAGE_REL_AMD64_REL32_3  = 0x0007 // 32-bit relative address from byte distance 3 from reloc
	IMAGE_REL_AMD64_REL32_4  = 0x0008 // 32-bit relative address from byte distance 4 from reloc
	IMAGE_REL_AMD64_REL32_5  = 0x0009 // 32-bit relative address from byte distance 5 from reloc
	IMAGE_REL_AMD64_SECTION  = 0x000A // Section index
	IMAGE_REL_AMD64_SECREL   = 0x000B // 32 bit offset from base of section containing target
	IMAGE_REL_AMD64_SECREL7  = 0x000C // 7 bit unsigned offset from base of section containing target
	IMAGE_REL_AMD64_TOKEN    = 0x000D // 32 bit metadata token
	IMAGE_REL_AMD64_SREL32   = 0x000E // 32 bit signed span-dependent value emitted into object
	IMAGE_REL_AMD64_PAIR     = 0x000F
	IMAGE_REL_AMD64_SSPAN32  = 0x0010 // 32 bit signed span-dependent value applied at link time
)

var MAP_IMAGE_REL_AMD64 = map[uint16]string{
	IMAGE_REL_AMD64_ABSOLUTE: "ABSOLUTE",
	IMAGE_REL_AMD64_ADDR64:   "ADDR64",
	IMAGE_REL_AMD64_ADDR32:   "ADDR32",
	IMAGE_REL_AMD64_ADDR32NB: "ADDR32NB",
	IMAGE_REL_AMD64_REL32:    "REL32",
	IMAGE_REL_AMD64_REL32_1:  "REL32_1",
	IMAGE_REL_AMD64_REL32_2:  "REL32_2",
	IMAGE_REL_AMD64_REL32_3:  "REL32_3",
	IMAGE_REL_AMD64_REL32_4:  "REL32_4",
	IMAGE_REL_AMD64_REL32_5:  "REL32_5",
	IMAGE_REL_AMD64_SECTION:  "SECTION",
	IMAGE_REL_AMD64_SECREL:   "SECREL",
	IMAGE_REL_AMD64_SECREL7:  "SECREL7",
	IMAGE_REL_AMD64_TOKEN:    "TOKEN",
	IMAGE_REL_AMD64_SREL32:   "SREL32",
	IMAGE_REL_AMD64_PAIR:     "PAIR",
	IMAGE_REL_AMD64_SSPAN32:  "SSPAN32",
}

// Symbol SectionNumber field special values
const (
	// The symbol record is not yet assigned a section.
	// A value of zero indicates that a reference to an external symbol is defined elsewhere.
	// A value of non-zero is a common symbol with a size that is specified by the value.
	IMAGE_SYM_UNDEFINED int16 = 0

	// The symbol has an absolute (non-relocatable) value and is not an address.
	IMAGE_SYM_ABSOLUTE = -1

	// The symbol provides general type or debugging information but does not correspond to a section.
	IMAGE_SYM_DEBUG = -2
)

var MAP_IMAGE_SYM_NUMBER = map[int16]string{
	IMAGE_SYM_UNDEFINED: "UNDEFINED",
	IMAGE_SYM_ABSOLUTE:  "ABSOLUTE",
	IMAGE_SYM_DEBUG:     "DEBUG",
}

// Symbol Type (SymbolType Base) field values
const (
	IMAGE_SYM_TYPE_NULL   byte = 0  // No type information or unknown base type.
	IMAGE_SYM_TYPE_VOID        = 1  // No valid type; used with void pointers and functions
	IMAGE_SYM_TYPE_CHAR        = 2  // A character (signed byte)
	IMAGE_SYM_TYPE_SHORT       = 3  // A 2-byte signed integer
	IMAGE_SYM_TYPE_INT         = 4  // A natural integer type (normally 4 bytes in Windows)
	IMAGE_SYM_TYPE_LONG        = 5  // A 4-byte signed integer
	IMAGE_SYM_TYPE_FLOAT       = 6  // A 4-byte floating-point number
	IMAGE_SYM_TYPE_DOUBLE      = 7  // An 8-byte floating-point number
	IMAGE_SYM_TYPE_STRUCT      = 8  // A structure
	IMAGE_SYM_TYPE_UNION       = 9  // A union
	IMAGE_SYM_TYPE_ENUM        = 10 // An enumerated type
	IMAGE_SYM_TYPE_MOE         = 11 // A member of enumeration (a specific value)
	IMAGE_SYM_TYPE_BYTE        = 12 // A byte; unsigned 1-byte integer
	IMAGE_SYM_TYPE_WORD        = 13 // A word; unsigned 2-byte integer
	IMAGE_SYM_TYPE_UINT        = 14 // An unsigned integer of natural size (normally, 4 bytes)
	IMAGE_SYM_TYPE_DWORD       = 15 // An unsigned 4-byte integer
)

var MAP_IMAGE_SYM_TYPE = [...]string{
	/* IMAGE_SYM_TYPE_NULL   */ "NULL",
	/* IMAGE_SYM_TYPE_VOID   */ "VOID",
	/* IMAGE_SYM_TYPE_CHAR   */ "CHAR",
	/* IMAGE_SYM_TYPE_SHORT  */ "SHORT",
	/* IMAGE_SYM_TYPE_INT    */ "INT",
	/* IMAGE_SYM_TYPE_LONG   */ "LONG",
	/* IMAGE_SYM_TYPE_FLOAT  */ "FLOAT",
	/* IMAGE_SYM_TYPE_DOUBLE */ "DOUBLE",
	/* IMAGE_SYM_TYPE_STRUCT */ "STRUCT",
	/* IMAGE_SYM_TYPE_UNION  */ "UNION",
	/* IMAGE_SYM_TYPE_ENUM   */ "ENUM",
	/* IMAGE_SYM_TYPE_MOE    */ "MOE",
	/* IMAGE_SYM_TYPE_BYTE   */ "BYTE",
	/* IMAGE_SYM_TYPE_WORD   */ "WORD",
	/* IMAGE_SYM_TYPE_UINT   */ "UINT",
	/* IMAGE_SYM_TYPE_DWORD  */ "DWORD",
}

// Symbol Type (SymbolType Derived) field values
const (
	IMAGE_SYM_DTYPE_NULL     byte = 0 // No derived type; the symbol is a simple scalar variable.
	IMAGE_SYM_DTYPE_POINTER       = 1 // The symbol is a pointer to base type.
	IMAGE_SYM_DTYPE_FUNCTION      = 2 // The symbol is a function that returns a base type.
	IMAGE_SYM_DTYPE_ARRAY         = 3 // The symbol is an array of base type.
)

var MAP_IMAGE_SYM_DTYPE = [...]string{
	/* IMAGE_SYM_DTYPE_NULL     */ "NULL",
	/* IMAGE_SYM_DTYPE_POINTER  */ "POINTER",
	/* IMAGE_SYM_DTYPE_FUNCTION */ "FUNCTION",
	/* IMAGE_SYM_DTYPE_ARRAY    */ "ARRAY",
}

// Symbol StorageClass field values
const (
	IMAGE_SYM_CLASS_NULL             uint8 = 0    // No assigned storage class.
	IMAGE_SYM_CLASS_AUTOMATIC              = 1    // The automatic (stack) variable. The Value field specifies the stack frame offset.
	IMAGE_SYM_CLASS_EXTERNAL               = 2    // A value that Microsoft tools use for external symbols. The Value field indicates the size if the section number is IMAGE_SYM_UNDEFINED (0). If the section number is not zero, then the Value field specifies the offset within the section.
	IMAGE_SYM_CLASS_STATIC                 = 3    // The offset of the symbol within the section. If the Value field is zero, then the symbol represents a section name.
	IMAGE_SYM_CLASS_REGISTER               = 4    // A register variable. The Value field specifies the register number.
	IMAGE_SYM_CLASS_EXTERNAL_DEF           = 5    // A symbol that is defined externally.
	IMAGE_SYM_CLASS_LABEL                  = 6    // A code label that is defined within the module. The Value field specifies the offset of the symbol within the section.
	IMAGE_SYM_CLASS_UNDEFINED_LABEL        = 7    // A reference to a code label that is not defined.
	IMAGE_SYM_CLASS_MEMBER_OF_STRUCT       = 8    // The structure member. The Value field specifies the nth member.
	IMAGE_SYM_CLASS_ARGUMENT               = 9    // A formal argument (parameter) of a function. The Value field specifies the nth argument.
	IMAGE_SYM_CLASS_STRUCT_TAG             = 10   // The structure tag-name entry.
	IMAGE_SYM_CLASS_MEMBER_OF_UNION        = 11   // A union member. The Value field specifies the nth member.
	IMAGE_SYM_CLASS_UNION_TAG              = 12   // The Union tag-name entry.
	IMAGE_SYM_CLASS_TYPE_DEFINITION        = 13   // A Typedef entry.
	IMAGE_SYM_CLASS_UNDEFINED_STATIC       = 14   // A static data declaration.
	IMAGE_SYM_CLASS_ENUM_TAG               = 15   // An enumerated type tagname entry.
	IMAGE_SYM_CLASS_MEMBER_OF_ENUM         = 16   // A member of an enumeration. The Value field specifies the nth member.
	IMAGE_SYM_CLASS_REGISTER_PARAM         = 17   // A register parameter.
	IMAGE_SYM_CLASS_BIT_FIELD              = 18   // A bit-field reference. The Value field specifies the nth bit in the bit field.
	IMAGE_SYM_CLASS_BLOCK                  = 100  // A .bb (beginning of block) or .eb (end of block) record. The Value field is the relocatable address of the code location.
	IMAGE_SYM_CLASS_FUNCTION               = 101  // A value that Microsoft tools use for symbol records that define the extent of a function: begin function (.bf), end function (.ef), and lines in function (.lf). For .lf records, the Value field gives the number of source lines in the function. For .ef records, the Value field gives the size of the function code.
	IMAGE_SYM_CLASS_END_OF_STRUCT          = 102  // An end-of-structure entry.
	IMAGE_SYM_CLASS_FILE                   = 103  // A value that Microsoft tools, as well as traditional COFF format, use for the source-file symbol record. The symbol is followed by auxiliary records that name the file.
	IMAGE_SYM_CLASS_SECTION                = 104  // A definition of a section (Microsoft tools use STATIC storage class instead).
	IMAGE_SYM_CLASS_WEAK_EXTERNAL          = 105  // A weak external. For more information, see section 5.5.3, “Auxiliary Format 3: Weak Externals.”
	IMAGE_SYM_CLASS_CLR_TOKEN              = 107  // A CLR token symbol. The name is an ASCII string that consists of the hexadecimal value of the token. For more information, see section 5.5.7, “CLR Token Definition (Object Only).”
	IMAGE_SYM_CLASS_END_OF_FUNCTION  uint8 = 0xFF // A special symbol that represents the end of function, for debugging purposes.
)

var MAP_IMAGE_SYM_CLASS = map[uint8]string{
	IMAGE_SYM_CLASS_NULL:             "NULL",
	IMAGE_SYM_CLASS_AUTOMATIC:        "AUTOMATIC",
	IMAGE_SYM_CLASS_EXTERNAL:         "EXTERNAL",
	IMAGE_SYM_CLASS_STATIC:           "STATIC",
	IMAGE_SYM_CLASS_REGISTER:         "REGISTER",
	IMAGE_SYM_CLASS_EXTERNAL_DEF:     "EXTERNAL_DEF",
	IMAGE_SYM_CLASS_LABEL:            "LABEL",
	IMAGE_SYM_CLASS_UNDEFINED_LABEL:  "UNDEFINED_LABEL",
	IMAGE_SYM_CLASS_MEMBER_OF_STRUCT: "MEMBER_OF_STRUCT",
	IMAGE_SYM_CLASS_ARGUMENT:         "ARGUMENT",
	IMAGE_SYM_CLASS_STRUCT_TAG:       "STRUCT_TAG",
	IMAGE_SYM_CLASS_MEMBER_OF_UNION:  "MEMBER_OF_UNION",
	IMAGE_SYM_CLASS_UNION_TAG:        "UNION_TAG",
	IMAGE_SYM_CLASS_TYPE_DEFINITION:  "TYPE_DEFINITION",
	IMAGE_SYM_CLASS_UNDEFINED_STATIC: "UNDEFINED_STATIC",
	IMAGE_SYM_CLASS_ENUM_TAG:         "ENUM_TAG",
	IMAGE_SYM_CLASS_MEMBER_OF_ENUM:   "MEMBER_OF_ENUM",
	IMAGE_SYM_CLASS_REGISTER_PARAM:   "REGISTER_PARAM",
	IMAGE_SYM_CLASS_BIT_FIELD:        "BIT_FIELD",
	IMAGE_SYM_CLASS_BLOCK:            "BLOCK",
	IMAGE_SYM_CLASS_FUNCTION:         "FUNCTION",
	IMAGE_SYM_CLASS_END_OF_STRUCT:    "END_OF_STRUCT",
	IMAGE_SYM_CLASS_FILE:             "FILE",
	IMAGE_SYM_CLASS_SECTION:          "SECTION",
	IMAGE_SYM_CLASS_WEAK_EXTERNAL:    "WEAK_EXTERNAL",
	IMAGE_SYM_CLASS_CLR_TOKEN:        "CLR_TOKEN",
	IMAGE_SYM_CLASS_END_OF_FUNCTION:  "END_OF_FUNCTION",
}

// Base relocations
const (
	IMAGE_REL_BASED_ABSOLUTE       = 0  // The base relocation is skipped. This type can be used to pad a block.
	IMAGE_REL_BASED_HIGH           = 1  // The base relocation adds the high 16 bits of the difference to the 16-bit field at offset. The 16-bit field represents the high value of a 32-bit word.
	IMAGE_REL_BASED_LOW            = 2  // The base relocation adds the low 16 bits of the difference to the 16-bit field at offset. The 16-bit field represents the low half of a 32-bit word.
	IMAGE_REL_BASED_HIGHLOW        = 3  // The base relocation applies all 32 bits of the difference to the 32-bit field at offset.
	IMAGE_REL_BASED_HIGHADJ        = 4  // The base relocation adds the high 16 bits of the difference to the 16-bit field at offset. The 16-bit field represents the high value of a 32-bit word. The low 16 bits of the 32-bit value are stored in the 16-bit word that follows this base relocation. This means that this base relocation occupies two slots.
	IMAGE_REL_BASED_MIPS_JMPADDR   = 5  // The base relocation applies to a MIPS jump instruction.
	IMAGE_REL_BASED_ARM_MOV32A     = 5  // The base relocation applies the difference to the 32-bit value encoded in the immediate fields of a contiguous MOVW+MOVT pair in ARM mode at offset.
	IMAGE_REL_BASED_RESERVED       = 6  // shouldn't be set
	IMAGE_REL_BASED_ARM_MOV32T     = 7  // The base relocation applies the difference to the 32-bit value encoded in the immediate fields of a contiguous MOVW+MOVT pair in Thumb mode at offset.
	IMAGE_REL_BASED_UNKNOWN        = 8  // ???
	IMAGE_REL_BASED_MIPS_JMPADDR16 = 9  // The base relocation applies to a MIPS16 jump instruction.
	IMAGE_REL_BASED_DIR64          = 10 // The base relocation applies the difference to the 64-bit field at offset.
	IMAGE_NUMBEROF_IMAGE_REL_BASED = 11
)

// Base relocation entry bit fields
const (
	IMAGE_REL_BASED_BITS_TYPE   = 4
	IMAGE_REL_BASED_BITS_OFFSET = 12

	IMAGE_REL_BASED_BLOCK_MAX_VA = 1 << IMAGE_REL_BASED_BITS_OFFSET
)

var MAP_IMAGE_REL_BASED = [...]string{
	/* IMAGE_REL_BASED_ABSOLUTE                  */ "ABSOLUTE",
	/* IMAGE_REL_BASED_HIGH                      */ "HIGH",
	/* IMAGE_REL_BASED_LOW                       */ "LOW",
	/* IMAGE_REL_BASED_HIGHLOW                   */ "HIGHLOW",
	/* IMAGE_REL_BASED_HIGHADJ                   */ "HIGHADJ",
	/* IMAGE_REL_BASED_MIPS_JMPADDR / ARM_MOV32A */ "MIPS_JMPADDR / ARM_MOV32A",
	/* IMAGE_REL_BASED_RESERVED                  */ "RESERVED",
	/* IMAGE_REL_BASED_ARM_MOV32T                */ "ARM_MOV32T",
	/* IMAGE_REL_BASED_UNKNOWN                   */ "UNKNOWN",
	/* IMAGE_REL_BASED_MIPS_JMPADDR16            */ "MIPS_JMPADDR16",
	/* IMAGE_REL_BASED_DIR64                     */ "DIR64",
}
