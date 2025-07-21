// Ref: https://github.com/praetorian-inc/goffloader

package coffer

import (
	_ "embed"
	"fmt"
	"runtime/debug"
	"strings"
	"syscall"
	"unsafe"

	"gopher/bof/binutil"
	"gopher/bof/boffer"
	"gopher/bof/defwin"

	"golang.org/x/sys/windows"
)

const (
	MEM_COMMIT             = windows.MEM_COMMIT
	MEM_RESERVE            = windows.MEM_RESERVE
	MEM_TOP_DOWN           = windows.MEM_TOP_DOWN
	PAGE_EXECUTE_READWRITE = windows.PAGE_EXECUTE_READWRITE
	// PAGE_EXECUTE_READ is a Windows constant used with Windows API calls
	PAGE_EXECUTE_READ = windows.PAGE_EXECUTE_READ
	// PAGE_READWRITE is a Windows constant used with Windows API calls
	PAGE_READWRITE = windows.PAGE_READWRITE
)

var (
	kernel32           = syscall.MustLoadDLL("kernel32.dll")
	procVirtualAlloc   = kernel32.MustFindProc("VirtualAlloc")
	procVirtualProtect = kernel32.MustFindProc("VirtualProtect")
)

func resolveExternalAddress(symbolName string, outChannel chan<- interface{}) uintptr {
	if strings.HasPrefix(symbolName, "__imp_") {
		symbolName = symbolName[6:]
		// 32 bit import names are __imp__
		symbolName = strings.TrimPrefix(symbolName, "_")

		libName := ""
		procName := ""

		// If we're following Dynamic Function Resolution Naming Conventions
		if len(strings.Split(symbolName, "$")) == 2 {
			libName = strings.Split(symbolName, "$")[0] + ".dll"
			procName = strings.Split(symbolName, "$")[1]
		} else {
			procName = symbolName

			switch procName {
			case "FreeLibrary", "LoadLibraryA", "GetProcAddress", "GetModuleHandleA", "GetModuleFileNameA":
				libName = "kernel32.dll"
			case "MessageBoxA":
				libName = "user32.dll"
			case string("BeaconOutput"):
				return windows.NewCallback(boffer.GetCoffOutputForChannel(outChannel))
			case string("BeaconDataParse"):
				return windows.NewCallback(boffer.DataParse)
			case string("BeaconDataInt"):
				return windows.NewCallback(boffer.DataInt)
			case string("BeaconDataShort"):
				return windows.NewCallback(boffer.DataShort)
			case string("BeaconDataLength"):
				return windows.NewCallback(boffer.DataLength)
			case string("BeaconDataExtract"):
				return windows.NewCallback(boffer.DataExtract)
			case string("BeaconPrintf"):
				return windows.NewCallback(boffer.GetCoffPrintfForChannel(outChannel))
			case string("BeaconAddValue"):
				return windows.NewCallback(boffer.AddValue)
			case string("BeaconGetValue"):
				return windows.NewCallback(boffer.GetValue)
			case string("BeaconRemoveValue"):
				return windows.NewCallback(boffer.RemoveValue)
			case string("BeaconFormatAlloc"):
				return windows.NewCallback(boffer.FormatAllocate)
			case string("BeaconFormatReset"):
				return windows.NewCallback(boffer.FormatReset)
			case string("BeaconFormatAppend"):
				return windows.NewCallback(boffer.FormatAppend)
			case string("BeaconFormatFree"):
				return windows.NewCallback(boffer.FormatFree)
			case string("BeaconFormatInt"):
				return windows.NewCallback(boffer.FormatInt)
			case string("BeaconFormatPrintf"):
				return windows.NewCallback(boffer.FormatPrintfFunc)
			case string("BeaconFormatToString"):
				return windows.NewCallback(boffer.FormatToString)
			case string("BeaconUseToken"):
				return windows.NewCallback(boffer.UseToken)
			case string("BeaconRevertToken"):
				return windows.NewCallback(boffer.RevertToken)
			case string("BeaconIsAdmin"):
				return windows.NewCallback(boffer.IsAdmin)
			case string("toWideChar"):
				return windows.NewCallback(boffer.ToWideChar)
			case string("BeaconGetSpawnTo"):
				fallthrough
			case string("BeaconGetSpawnTemporaryProcess"):
				fallthrough
			case string("BeaconInjectProcess"):
				fallthrough
			case string("BeaconInjectTemporaryProcess"):
				fallthrough
			case string("BeaconCleanupProcess"):
				fallthrough
			default:
				fmt.Printf("Unknown symbol: %s\n", procName)
				return 0
			}
		}

		libStringPtr, _ := syscall.LoadLibrary(libName)
		procAddress, _ := syscall.GetProcAddress(libStringPtr, procName)
		return procAddress
	}
	return 0
}

func virtualAlloc(lpAddress uintptr, dwSize uintptr, flAllocationType uint32, flProtect uint32) (uintptr, error) {
	ret, _, err := procVirtualAlloc.Call(
		lpAddress,
		dwSize,
		uintptr(flAllocationType),
		uintptr(flProtect),
	)
	if ret == 0 {
		return 0, err
	}
	return ret, nil
}

func isSpecialSymbol(sym *SymbolParsed) bool {
	return sym.StorageClass == defwin.IMAGE_SYM_CLASS_EXTERNAL && sym.SectionNumber == 0
}

func isImportSymbol(sym *SymbolParsed) bool {
	return strings.HasPrefix(sym.NameString(), "__imp_")
}

func processRelocation(symbolDefAddress uintptr, sectionAddress uintptr, reloc defwin.Relocation, symbol *SymbolParsed) {
	symbolOffset := (uintptr)(reloc.VirtualAddress)

	absoluteSymbolAddress := symbolOffset + sectionAddress

	segmentValue := *(*uint32)(unsafe.Pointer(absoluteSymbolAddress))

	if (symbol.StorageClass == defwin.IMAGE_SYM_CLASS_STATIC && symbol.Value != 0) ||
		(symbol.StorageClass == defwin.IMAGE_SYM_CLASS_EXTERNAL && symbol.SectionNumber != 0) {
		symbolOffset = (uintptr)(symbol.Value)
	} else {
		symbolDefAddress += (uintptr)(segmentValue)
	}

	symbolRefAddress := sectionAddress

	//TODO: Handle x86 cases as well
	switch reloc.Type {
	case defwin.IMAGE_REL_AMD64_ADDR64:
		addr := (*uint64)(unsafe.Pointer(absoluteSymbolAddress))
		fmt.Sprintf("Symbol Ref Address: 0x%x\n", addr)
		*addr = uint64(symbolDefAddress)
	case defwin.IMAGE_REL_AMD64_ADDR32NB:
		addr := (*uint32)(unsafe.Pointer(absoluteSymbolAddress))
		valueToWrite := symbolDefAddress - (symbolRefAddress + 4 + symbolOffset)
		fmt.Sprintf("Symbol Ref Address: 0x%x\n", addr)
		*addr = uint32(valueToWrite)
	case defwin.IMAGE_REL_AMD64_REL32, defwin.IMAGE_REL_AMD64_REL32_1, defwin.IMAGE_REL_AMD64_REL32_2, defwin.IMAGE_REL_AMD64_REL32_3, defwin.IMAGE_REL_AMD64_REL32_4, defwin.IMAGE_REL_AMD64_REL32_5:
		relativeSymbolDefAddress := symbolDefAddress - (uintptr)(reloc.Type-4) - (absoluteSymbolAddress + 4)
		addr := (*uint32)(unsafe.Pointer(absoluteSymbolAddress))
		fmt.Sprintf("Symbol Ref Address: 0x%x\n", addr)
		*addr = uint32(relativeSymbolDefAddress)
	default:
		fmt.Printf("Unsupported relocation type: %d\n", reloc.Type)
	}
}

type CoffSection struct {
	Section *Section
	Address uintptr
}

func Load(coffBytes []byte, argBytes []byte) (string, error) {
	return LoadWithMethod(coffBytes, argBytes, "go")
}

func LoadWithMethod(coffBytes []byte, argBytes []byte, method string) (string, error) {
	output := make(chan interface{})

	parsedCoff := Explore(binutil.WrapByteSlice(coffBytes))
	parsedCoff.ReadAll()
	parsedCoff.Seal()

	sections := make(map[string]CoffSection, parsedCoff.Sections.Len())

	gotBaseAddress := uintptr(0)
	gotOffset := 0
	gotSize := uint32(0)
	var gotMap = make(map[string]uintptr)

	bssBaseAddress := uintptr(0)
	bssOffset := 0
	bssSize := uint32(0)

	for _, symbol := range parsedCoff.Symbols {
		if isSpecialSymbol(symbol) {
			if isImportSymbol(symbol) {
				gotSize += 8
			} else {
				bssSize += symbol.Value + 8 //leave room for null bytes
			}
		}
	}

	for _, section := range parsedCoff.Sections.Array() {
		allocationSize := uintptr(section.SizeOfRawData)
		if strings.HasPrefix(section.NameString(), ".bss") {
			allocationSize = uintptr(bssSize)
		}

		if allocationSize == 0 {
			continue
		}

		addr, err := virtualAlloc(0, allocationSize, MEM_COMMIT|MEM_RESERVE|MEM_TOP_DOWN, PAGE_READWRITE)
		if err != nil {
			return "", fmt.Errorf("VirtualAlloc failed: %s", err.Error())
		}

		if strings.HasPrefix(section.NameString(), ".bss") {
			bssBaseAddress = addr
		}

		copy((*[1 << 30]byte)(unsafe.Pointer(addr))[:], section.RawData())

		allocatedSection := CoffSection{
			Section: section,
			Address: addr,
		}

		sections[section.NameString()] = allocatedSection
	}

	gotBaseAddress, err := virtualAlloc(0, uintptr(gotSize), MEM_COMMIT|MEM_RESERVE|MEM_TOP_DOWN, PAGE_READWRITE)
	if err != nil {
		return "", fmt.Errorf("VirtualAlloc failed: %s", err.Error())
	}

	for _, section := range parsedCoff.Sections.Array() {
		sectionVirtualAddr := sections[section.NameString()].Address
		fmt.Sprintf("Section: %s\n", section.NameString())

		for _, reloc := range section.Relocations() {

			symbol := parsedCoff.Symbols[reloc.SymbolTableIndex]

			if symbol.StorageClass > 3 {
				continue
			}

			symbolTypeString := defwin.MAP_IMAGE_SYM_CLASS[symbol.StorageClass]
			fmt.Sprintf("0x%08X %s %s\n", reloc.VirtualAddress, symbolTypeString, symbol.NameString())
			symbolDefAddress := uintptr(0)

			if isSpecialSymbol(symbol) {
				if isImportSymbol(symbol) {
					externalAddress := resolveExternalAddress(symbol.NameString(), output)

					if externalAddress == 0 {
						return "", fmt.Errorf("failed to resolve external address for symbol: %s", symbol.NameString())
					}

					if existingGotAddress, exists := gotMap[symbol.NameString()]; exists {
						symbolDefAddress = existingGotAddress
					} else {
						symbolDefAddress = gotBaseAddress + uintptr(gotOffset*8)
						gotOffset += 1
						gotMap[symbol.NameString()] = symbolDefAddress
					}
					copy((*[8]byte)(unsafe.Pointer(symbolDefAddress))[:], (*[8]byte)(unsafe.Pointer(&externalAddress))[:])
				} else {
					symbolDefAddress = bssBaseAddress + uintptr(bssOffset)
					bssOffset += int(symbol.Value) + 8
				}
			} else {
				targetSection := parsedCoff.Sections.Array()[symbol.SectionNumber-1]
				symbolDefAddress = sections[targetSection.NameString()].Address + uintptr(symbol.Value)
			}

			fmt.Sprintf("Symbol Def Address: 0x%x\n", symbolDefAddress)
			processRelocation(symbolDefAddress, sectionVirtualAddr, reloc, symbol)
		}

		if section.Characteristics&defwin.IMAGE_SCN_MEM_EXECUTE != 0 {
			oldProtect := PAGE_READWRITE
			_, _, errVirtualProtect := procVirtualProtect.Call(sectionVirtualAddr, uintptr(section.SizeOfRawData), PAGE_EXECUTE_READ, uintptr(unsafe.Pointer(&oldProtect)))
			if errVirtualProtect != nil && errVirtualProtect.Error() != "The operation completed successfully." {
				return "", fmt.Errorf("Error calling VirtualProtect:\r\n%s", errVirtualProtect.Error())
			}
		}
	}

	// Call the entry point
	go invokeMethod(method, argBytes, parsedCoff, sections, output)

	bofOutput := ""
	for msg := range output {
		bofOutput += msg.(string) + "\n"
	}
	return bofOutput, nil
}

func invokeMethod(methodName string, argBytes []byte, parsedCoff *File, sectionMap map[string]CoffSection, outChannel chan<- interface{}) {
	defer close(outChannel)

	// Catch unexpected panics and propagate them to the output channel
	// This prevents the host program from terminating unexpectedly
	defer func() {
		if r := recover(); r != nil {
			errorMsg := fmt.Sprintf("Panic occurred when executing COFF: %v\n%s", r, debug.Stack())
			outChannel <- errorMsg
		}
	}()

	// Call the entry point
	for _, symbol := range parsedCoff.Symbols {
		if symbol.NameString() == methodName {
			mainSection := parsedCoff.Sections.Array()[symbol.SectionNumber-1]
			entryPoint := sectionMap[mainSection.NameString()].Address + uintptr(symbol.Value)

			if len(argBytes) == 0 {
				argBytes = make([]byte, 1)
			}
			syscall.SyscallN(entryPoint, uintptr(unsafe.Pointer(&argBytes[0])), uintptr((len(argBytes))))
		}
	}
}
