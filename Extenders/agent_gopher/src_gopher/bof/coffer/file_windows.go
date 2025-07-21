// Ref: https://github.com/RIscRIpt/pecoff

package coffer

import (
	"bytes"
	"errors"
	"fmt"
	"strconv"
	"unsafe"

	"gopher/bof/binutil"
	defwin "gopher/bof/defwin"
)

// List of supported coff MachineTypes by this parser
var supportedMachineTypes = [...]uint16{
	defwin.IMAGE_FILE_MACHINE_I386,
	defwin.IMAGE_FILE_MACHINE_AMD64,
}

// List of errors
var (
	ErrAlreadyRead    = errors.New("pecoff: already read")
	ErrUnsuppMachType = errors.New("pecoff: unsupported image file machine type")

	// A group of errors which are most-likey to be returned if a client
	// of this package is using Read* methods in a wrong order.
	ErrNoFileHeader      = errors.New("pecoff: file header is not read")
	ErrNoSectionsHeaders = errors.New("pecoff: headers of sections are not read")

	// A group of errors which are returned by Read* wrapper methods, such as
	//     ReadAll, ReadHeaders, ReadSectionsRawData, ReadDataDirs, etc...
	// These errors specify what exatcly has been failed to read (or check).
	ErrFailReadHeaders         = errors.New("pecoff: failed to read headers")
	ErrFailReadFileHeader      = errors.New("pecoff: failed to read file header")
	ErrFailReadSymbols         = errors.New("pecoff: failed to read symbols")
	ErrFailReadStringTable     = errors.New("pecoff: failed to read string table")
	ErrFailReadSectionsHeaders = errors.New("pecoff: failed to read headers of sections")
	ErrFailReadSectionsData    = errors.New("pecoff: failed to read sections raw data")
	ErrFailReadSectionsRelocs  = errors.New("pecoff: failed to read relocations of sections")
	ErrFailReadSectionsLineNrs = errors.New("pecoff: failed to read line numbers of sections")

	// A group of errors which are to be formatted (used in errorf or wrapErrorf method)
	ErrfFailReadSectionHeader   = "pecoff: failed to read a header of section#%d (%X)"                   //fmt: sectionId, offset
	ErrfFailReadSectionRawData  = "pecoff: failed to read rawdata of section#%d (%X)"                    //fmt: sectionId, offset
	ErrfFailReadSectionReloc    = "pecoff: failed to read relocation#%d of section #%d (%X)"             //fmt: relocationId, sectionId, offset
	ErrfFailReadSymbol          = "pecoff: failed to read symbol#%d (%X)"                                //fmt: symbolId, offset
	ErrfFailReadStrTblSize      = "pecoff: failed to read string table size (%X)"                        //fmt: offset
	ErrfFailReadStrTbl          = "pecoff: failed to read string table (%X)"                             //fmt: offset
	ErrfFailReadImpDesc         = "pecoff: failed to read import descriptor#%d (%X)"                     //fmt: descriptorId, offset
	ErrfFailReadBaseRel         = "pecoff: failed to read base relocation#%d (%X)"                       //fmt: relocationId, offset
	ErrfFailReadBaseRelEntries  = "pecoff: failed to read base relocation#%d entries (%X)"               //fmt: relocationId, offset
	ErrfFailFindBaseRelsFromInt = "pecoff: failed to find base relocations within interval [%08X; %08X)" //fmt: VirtualAddress, VirtualAddress
)

// File contains embedded io.Reader and all the fields of a COFF file.
type File struct {
	binutil.ReaderAtInto
	FileHeader  *defwin.FileHeader
	Sections    *Sections
	Symbols     Symbols
	StringTable StringTable
}

// Explore creates a new File object
func Explore(reader binutil.ReaderAtInto) *File {
	return &File{
		ReaderAtInto: reader,
	}
}

// Seal eliminates all external pointers (relatively to this package), so a
// File object can be long-term stored without holding any (useless) resources.
// For example after calling ReadAll method, and having all the data read from the file.
func (f *File) Seal() {
	f.ReaderAtInto = nil
}

func (f *File) error(err error) error {
	return err
	// return &FileError{error: err}
}

func (f *File) wrapError(innerError error, outerError error) error {
	return &FileError{
		error:      outerError,
		innerError: innerError,
	}
}

func (f *File) errorf(format string, a ...interface{}) error {
	return f.error(fmt.Errorf(format, a...))
}

func (f *File) wrapErrorf(innerError error, format string, a ...interface{}) error {
	return f.wrapError(innerError, fmt.Errorf(format, a...))
}

// ReadAll parses pe/coff file reading all the data of the file into the memory.
// Returns an error if any occured during the parsing.
func (f *File) ReadAll() (err error) {
	if err = f.ReadHeaders(); err != nil {
		return f.wrapError(err, ErrFailReadHeaders)
	}
	if err = f.ReadStringTable(); err != nil {
		return f.wrapError(err, ErrFailReadStringTable)
	}
	if err = f.ReadSymbols(); err != nil {
		return f.wrapError(err, ErrFailReadSymbols)
	}
	if err = f.ReadSectionsHeaders(); err != nil {
		return f.wrapError(err, ErrFailReadSectionsHeaders)
	}
	if err = f.ReadSectionsRawData(); err != nil {
		return f.wrapError(err, ErrFailReadSectionsData)
	}
	if err = f.ReadSectionsRelocations(); err != nil {
		return f.wrapError(err, ErrFailReadSectionsRelocs)
	}

	return
}

// Returned in terms of a COFF file.
func (f *File) GetFileHeaderOffset() int64 {
	return defwin.OFFSET_COFF_FILE_HEADER
}

func (f *File) getOptHeaderOffset() int64 {
	return f.GetFileHeaderOffset() + int64(defwin.SIZEOF_IMAGE_FILE_HEADER)
}

func (f *File) getSectionsHeadersOffset() int64 {
	return f.getOptHeaderOffset() + int64(f.FileHeader.SizeOfOptionalHeader)
}

func (f *File) getStringTableOffset() int64 {
	return int64(f.FileHeader.PointerToSymbolTable) + int64(f.FileHeader.NumberOfSymbols)*defwin.SIZEOF_IMAGE_SYMBOL
}

// Is64Bit returns true if Machine type of file header equals to AMD64 or IA64
// If FileHeader is nil (i.e. wasn't read) an error ErrNoFileHeader is returned.
func (f *File) Is64Bit() (bool, error) {
	if f.FileHeader == nil {
		return false, f.error(ErrNoFileHeader)
	}
	return f.is64Bit(), nil
}

func (f *File) is64Bit() bool {
	return f.FileHeader.Machine == defwin.IMAGE_FILE_MACHINE_AMD64 ||
		f.FileHeader.Machine == defwin.IMAGE_FILE_MACHINE_IA64
}

// IsSupportedMachineType returns true only if Machine type (in the FileHeader)
// is fully supported by all the functions in this package.
// If FileHeader is nil, an error ErrNoFileHeader is returned.
func (f *File) IsSupportedMachineType() (bool, error) {
	if f.FileHeader == nil {
		return false, f.error(ErrNoFileHeader)
	}
	return f.isSupportedMachineType(), nil
}

// isSupportedMachineType is an unsafe version of an exported function, which
// must be used only internally and only after FileHeader has been read.
func (f *File) isSupportedMachineType() bool {
	for _, sm := range supportedMachineTypes {
		if sm == f.FileHeader.Machine {
			return true
		}
	}
	return false
}

// ReadHeaders reads:
//   - FileHeader;
//   - Headers of sections;
//
// Returns error if any
func (f *File) ReadHeaders() (err error) {
	if err = f.ReadFileHeader(); err != nil {
		return f.wrapError(err, ErrFailReadFileHeader)
	}
	if !f.isSupportedMachineType() {
		return f.error(ErrUnsuppMachType)
	}

	return nil
}

// ReadFileHeader reads PE/COFF file header.
// Returns an error ErrAlreadyRead, if it has already been read,
// or an error from ReadAtInto method, if any.
func (f *File) ReadFileHeader() error {
	if f.FileHeader != nil {
		return f.error(ErrAlreadyRead)
	}
	fileHeader := new(defwin.FileHeader)
	if err := f.ReadAtInto(fileHeader, f.GetFileHeaderOffset()); err != nil {
		return f.error(err)
	}
	f.FileHeader = fileHeader
	return nil
}

// ReadSectionsHeaders reads headers of sections of a PE/COFF file.
// Returns an error ErrAlreadyRead, if it has already been read,
// or an error from ReadAtInto method, if any.
func (f *File) ReadSectionsHeaders() error {
	if f.FileHeader == nil {
		return f.error(ErrNoFileHeader)
	}
	if f.Sections != nil {
		return f.error(ErrAlreadyRead)
	}
	sections := newSections(int(f.FileHeader.NumberOfSections))
	baseOffset := f.getSectionsHeadersOffset()
	for i := range sections.array {
		s := new(Section)
		s.id = i
		offset := baseOffset + int64(i)*defwin.SIZEOF_IMAGE_SECTION_HEADER
		if err := f.ReadAtInto(&s.SectionHeader, offset); err != nil {
			return f.wrapErrorf(err, ErrfFailReadSectionHeader, i, offset)
		}
		nullIndex := 0
		for nullIndex < 8 && s.Name[nullIndex] != 0 {
			nullIndex++
		}
		s.nameString = string(s.Name[:nullIndex])
		if s.Name[0] == '/' && f.StringTable != nil {
			// If section name contains garbage, just ignore it.
			// So, if something fails here (err != nil),
			// nothing critical happens can be safely ignored.
			strTblOffset, err := strconv.Atoi(string(s.Name[1:nullIndex]))
			if err == nil {
				nameString, err := f.StringTable.GetString(strTblOffset)
				if err == nil {
					s.nameString = nameString
				}
			}
		}
		sections.array[i] = s
	}
	// Sort is required for efficient work of Sections.GetByVA method,
	// which uses a binary search algorithm of sort.Search
	// to find a section by VirtualAddress.
	sections.sort()
	f.Sections = sections
	return nil
}

// ReadSectionsRawData reads contents (raw data) of all sections into memory.
// Headers of sections must be read before calling this method,
// otherwise an error ErrNoSectionsHeaders is returned.
// An error is returned if any occured while reading data.
func (f *File) ReadSectionsRawData() error {
	if f.Sections == nil {
		return f.error(ErrNoSectionsHeaders)
	}
	for i, s := range f.Sections.array {
		rawData := make([]byte, s.SizeOfRawData)
		if s.SizeOfRawData != 0 {
			if _, err := f.ReadAt(rawData, int64(s.PointerToRawData)); err != nil {
				return f.wrapErrorf(err, ErrfFailReadSectionRawData, i, s.PointerToRawData)
			}
		}
		s.rawData = rawData
	}
	return nil
}

// ReadSectionsRelocations reads relocations of all sections.
// Headers of sections must be read before calling this method,
// otherwise an error ErrNoSectionsHeaders is returned.
// An error is returned if any occured while reading data.
func (f *File) ReadSectionsRelocations() error {
	if f.Sections == nil {
		return f.error(ErrNoSectionsHeaders)
	}
	for i, s := range f.Sections.array {
		relocations := make([]defwin.Relocation, s.NumberOfRelocations)
		for j := range relocations {
			offset := int64(s.PointerToRelocations) + int64(j)*defwin.SIZEOF_IMAGE_RELOCATION
			if err := f.ReadAtInto(&relocations[j], offset); err != nil {
				return f.wrapErrorf(err, ErrfFailReadSectionReloc, j, i, offset)
			}
		}
		s.relocations = relocations
	}
	return nil
}

// ReadSymbols reads all symbols from the symbol table.
// FileHeader must be read before calling this method,
// otherwise an error ErrNoFileHeader is returned.
// An error is returned if any occured while reading data.
func (f *File) ReadSymbols() error {
	if f.FileHeader == nil {
		return f.error(ErrNoFileHeader)
	}
	symbols := make(Symbols, int(f.FileHeader.NumberOfSymbols))
	baseOffset := int64(f.FileHeader.PointerToSymbolTable)
	for i := range symbols {
		s := new(SymbolParsed)
		offset := baseOffset + int64(i)*defwin.SIZEOF_IMAGE_SYMBOL
		if err := f.ReadAtInto(&s.Symbol, offset); err != nil {
			return f.wrapErrorf(err, ErrfFailReadSymbol, i, offset)
		}
		// If the name is longer than 8 bytes, first 4 bytes are set to zero
		// and the remaining 4 represent an offset into the string table.
		if *(*uint32)(unsafe.Pointer(&s.Name[0])) == 0 {
			strTblOffset := int(*(*uint32)(unsafe.Pointer(&s.Name[4])))
			nameString, err := f.StringTable.GetString(strTblOffset)
			if err == nil {
				s.nameString = nameString
			} else {
				s.nameString = fmt.Sprintf("/%d", strTblOffset)
			}
		} else {
			nullIndex := 0
			for nullIndex < 8 && s.Name[nullIndex] != 0 {
				nullIndex++
			}
			s.nameString = string(s.Name[:nullIndex])
		}
		symbols[i] = s
	}
	f.Symbols = symbols
	return nil
}

// ReadStringTable reads the whole COFF string table into the memory.
// FileHeader must be read before calling this method,
// otherwise an error ErrNoFileHeader is returned.
// An error is returned if any occured while reading data.
func (f *File) ReadStringTable() error {
	if f.FileHeader == nil {
		return f.error(ErrNoFileHeader)
	}
	offset := f.getStringTableOffset()
	// According to the Microsoft's PE/COFF file specification,
	// symbols and string table *should* only exist in the COFF files.
	// But some compilers of awesome languages (such as Go) ignore this fact,
	// and still have symbols and string table in the PE (.exe) files.
	// Also it's nothing told about if string table can exist w/o symbols, but
	// this is important as calculation of the pointer to the string table is based
	// on the FileHeader.PointerToSymbolTable and FileHeader.NumberOfSymbols.
	// So if offset is 0, there are apparently no symbols and no string table.
	if offset == 0 {
		return nil
	}
	var size uint32
	if err := f.ReadAtInto(&size, offset); err != nil {
		return f.wrapErrorf(err, ErrfFailReadStrTblSize, offset)
	}
	table := make([]byte, size)
	if _, err := f.ReadAt(table, offset); err != nil {
		return f.wrapErrorf(err, ErrfFailReadStrTbl, offset)
	}
	f.StringTable = table
	return nil
}

// FileError represents an internal error which may occur while
// reading/parsing a PE/COFF file.
type FileError struct {
	error
	innerError error
}

// (Error must be already implemented in FileError by embedded `error`)
// func (e *FileError) Error() string {
// 	return e.error.Error()
// }

// ToMultiError returns a flattened list of errors
// formed from 'recursive' innerErrors
func (e *FileError) ToMultiError() (list MultiError) {
	err := e
	for err != nil {
		list = append(list, err.error)
		if innerError, ok := err.innerError.(*FileError); ok {
			err = innerError
		} else {
			if err.innerError != nil {
				list = append(list, err.innerError)
			}
			break
		}
	}
	return
}

// ErrorFlatten returns err.ToMultiError if `err` implements FileError,
// if not, an `err` without modifications is returned.
func ErrorFlatten(err error) error {
	if fe, ok := err.(*FileError); ok {
		return fe.ToMultiError()
	}
	return err
}

// MultiError represents a collection of errors
type MultiError []error

// Error checks count of errors which are stored in the MultiError and
// returns a string represntation of an error, if:
//
//	0 entries   : "" (empty string);
//	1 entry     : Error() value of the first error;
//	>=2 entries : Error() values separated with new line character ('\n').
func (e MultiError) Error() string {
	switch len(e) {
	case 0:
		return ""
	case 1:
		return e[0].Error()
	default:
		var buf bytes.Buffer
		// no error checking of Write* methods,
		// as according to the docs, err is always nil.
		buf.WriteString(fmt.Sprintf("pecoff: multi error (%d)\n", len(e)))
		for _, err := range e {
			buf.WriteRune('\t')
			buf.WriteString(err.Error())
			buf.WriteRune('\n')
		}
		return buf.String()
	}
}

type StringTable []byte

// List of errors which can be returned by methods of StringTable.
var (
	ErrStrOffOutOfBounds = errors.New("string offset is out of bounds")
)

// GetString returns a string which starts at
// specified offset inside the string table.
func (t StringTable) GetString(offset int) (string, error) {
	if offset < 0 || offset >= len(t) {
		return "", ErrStrOffOutOfBounds
	}
	nullIndex := offset
	// all strings must be null-terminated,
	// but we don't want to crash unexpectedly,
	// so let's check slice bounds anyway.
	for nullIndex < len(t) && t[nullIndex] != 0 {
		nullIndex++
	}
	// if nullIndex == len(t) { panic("last string is not null-terminated") }
	return string(t[offset:nullIndex]), nil
}
