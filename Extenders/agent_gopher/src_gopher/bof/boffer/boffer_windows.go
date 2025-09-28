// Ref: https://github.com/praetorian-inc/goffloader

/*
   Helpers for Adaptix BOF modules that run *inside* the agent process on
   Windows (both x86 and x64).
*/

package boffer

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"math"
	"strings"
	"syscall"
	"unicode/utf16"
	"unsafe"

	"gopher/bof/memory"

	"golang.org/x/sys/windows"
)

type (
	datap struct {
		original uintptr
		buffer   uintptr
		length   uint32
		size     uint32
	}

	formatp struct {
		original uintptr
		buffer   uintptr
		length   uint32
		size     uint32
	}

	TOKEN_ELEVATION struct {
		TokenIsElevated uint32
	}
)

const (
	TOKEN_QUERY      = 0x0008
	TokenElevation   = 20
	STATUS_SUCCESS   = 0
	NtCurrentProcess = ^uintptr(0) // -1 as uintptr

	CP_ACP               = 0
	MB_ERR_INVALID_CHARS = 0x00000008
)

var (
	advapi32                    = syscall.MustLoadDLL("advapi32.dll")
	procSetThreadToken          = advapi32.MustFindProc("SetThreadToken")
	procImpersonateLoggedOnUser = advapi32.MustFindProc("ImpersonateLoggedOnUser")
	procRevertToSelf            = advapi32.MustFindProc("RevertToSelf")
	procGetTokenInformation     = advapi32.MustFindProc("GetTokenInformation")
	procOpenProcessToken        = advapi32.MustFindProc("OpenProcessToken")

	kernel32                = syscall.MustLoadDLL("kernel32.dll")
	procLocalAlloc          = kernel32.MustFindProc("LocalAlloc")
	procGetCurrentProcess   = kernel32.MustFindProc("GetCurrentProcess")
	procCloseHandle         = kernel32.MustFindProc("CloseHandle")
	procMultiByteToWideChar = kernel32.MustFindProc("MultiByteToWideChar")
)

var (
	bofImpersonate uint32 = 0
)

func parsePrintfFormat(fmtStr string, args []uintptr) string {
	result := ""
	argOffset := 0
	i := 0

	for i < len(fmtStr) {
		if fmtStr[i] == '%' && i < len(fmtStr)-1 {
			i++ // ignore '%'

			// ignore flags (-, +, space, #, 0)
			for i < len(fmtStr) && strings.ContainsRune("-+ #0", rune(fmtStr[i])) {
				i++
			}

			// ignore length (numbers or *)
			if i < len(fmtStr) && fmtStr[i] == '*' {
				i++
				argOffset++ // * uses argument
			} else {
				for i < len(fmtStr) && fmtStr[i] >= '0' && fmtStr[i] <= '9' {
					i++
				}
			}

			// ignore accuracy (.numbers or .*)
			if i < len(fmtStr) && fmtStr[i] == '.' {
				i++
				if i < len(fmtStr) && fmtStr[i] == '*' {
					i++
					argOffset++ // .* uses argument
				} else {
					for i < len(fmtStr) && fmtStr[i] >= '0' && fmtStr[i] <= '9' {
						i++
					}
				}
			}

			// read length modifier
			modifier := ""
			if i < len(fmtStr) {
				switch fmtStr[i] {
				case 'h':
					modifier = "h"
					i++
					if i < len(fmtStr) && fmtStr[i] == 'h' {
						modifier = "hh"
						i++
					}
				case 'l':
					modifier = "l"
					i++
					if i < len(fmtStr) && fmtStr[i] == 'l' {
						modifier = "ll"
						i++
					}
				case 'L', 'j', 'z', 't':
					modifier = string(fmtStr[i])
					i++
				}
			}

			// read conversion spec
			if i < len(fmtStr) {
				spec := fmtStr[i]

				if spec == '%' {
					result += "%"
					i++
					continue // %% not uses argument
				}

				if argOffset < len(args) {
					switch spec {
					case 's':
						if modifier == "l" || modifier == "ll" {
							// Wide string
							s := memory.ReadWStringFromPtr(args[argOffset])
							result += s
						} else {
							// ANSI string
							s := memory.ReadCStringFromPtr(args[argOffset])
							// If less than 5 symbols - check for widestring
							if len(s) < 5 && s != "" {
								ws := memory.ReadWStringFromPtr(args[argOffset])
								if ws != "" && len(ws) >= len(s) {
									s = ws
								}
							}
							result += s
						}

					case 'S': // %S = wide string in Windows
						result += memory.ReadWStringFromPtr(args[argOffset])

					case 'c':
						if modifier == "l" {
							// Wide char
							wc := uint16(args[argOffset])
							result += string(utf16.Decode([]uint16{wc}))
						} else {
							result += string(byte(args[argOffset]))
						}

					case 'C': // %C = wide char in Windows
						wc := uint16(args[argOffset])
						result += string(utf16.Decode([]uint16{wc}))

					case 'p':
						result += fmt.Sprintf("0x%x", args[argOffset])

					case 'd', 'i':
						switch modifier {
						case "ll":
							result += fmt.Sprintf("%d", int64(args[argOffset]))
						case "hh":
							result += fmt.Sprintf("%d", int16(args[argOffset]))
						case "h":
							result += fmt.Sprintf("%d", int8(args[argOffset]))
						default:
							result += fmt.Sprintf("%d", int32(args[argOffset]))
						}
					case 'u', 'o', 'x', 'X':
						format := "%" + string(spec)
						switch modifier {
						case "ll":
							result += fmt.Sprintf(format, int64(args[argOffset]))
						case "hh":
							result += fmt.Sprintf(format, int16(args[argOffset]))
						case "h":
							result += fmt.Sprintf(format, int8(args[argOffset]))
						default:
							result += fmt.Sprintf(format, int32(args[argOffset]))
						}
					case 'f', 'F', 'e', 'E', 'g', 'G':
						// Float/double
						result += fmt.Sprintf("%"+string(spec), math.Float64frombits(uint64(args[argOffset])))
					case 'n':
						// Ignore
					default:
						// Unknown spec
						result += fmt.Sprintf("%"+string(spec), args[argOffset])
					}

					argOffset++
				}
				i++
			}
		} else {
			result += string(fmtStr[i])
			i++
		}
	}

	return result
}

// export BeaconOutput
func GetCoffOutputForChannel(channel chan<- interface{}) func(int, uintptr, int) uintptr {
	return func(beaconType int, data uintptr, length int) uintptr {
		if length <= 0 {
			return 0
		}
		out := memory.ReadBytesFromPtr(data, uint32(length))

		channel <- beaconType
		channel <- []byte(out)
		return 1
	}
}

// export BeaconPrintf
func GetCoffPrintfForChannel(channel chan<- interface{}) func(int, uintptr, uintptr, uintptr, uintptr, uintptr, uintptr, uintptr, uintptr, uintptr, uintptr, uintptr) uintptr {
	return func(beaconType int, data uintptr, arg0 uintptr, arg1 uintptr, arg2 uintptr, arg3 uintptr, arg4 uintptr, arg5 uintptr, arg6 uintptr, arg7 uintptr, arg8 uintptr, arg9 uintptr) uintptr {
		fmtStr := memory.ReadCStringFromPtr(data)
		args := []uintptr{arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9}

		result := parsePrintfFormat(fmtStr, args)
		channel <- beaconType
		channel <- []byte(result)
		return 0
	}
}

// export BeaconFormatPrintf
func FormatPrintfFunc(format *formatp, fmtPtr uintptr, arg0 uintptr, arg1 uintptr, arg2 uintptr, arg3 uintptr, arg4 uintptr, arg5 uintptr, arg6 uintptr, arg7 uintptr, arg8 uintptr, arg9 uintptr) uintptr {
	if format == nil || format.original == 0 || fmtPtr == 0 {
		return 0
	}

	fmtStr := memory.ReadCStringFromPtr(fmtPtr)
	if fmtStr == "" {
		return 0
	}

	args := []uintptr{arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9}
	result := parsePrintfFormat(fmtStr, args)

	resultLen := uint32(len(result))
	if format.length+resultLen > format.size {
		return 0
	}

	resultBytes := []byte(result)
	for i := uint32(0); i < resultLen; i++ {
		*(*byte)(unsafe.Pointer(format.buffer + uintptr(i))) = resultBytes[i]
	}

	format.buffer += uintptr(resultLen)
	format.length += resultLen
	return 0
}

// export BeaconDataExtract
func DataExtract(datap *datap, size *uint32) uintptr {
	if datap.length <= 0 {
		return 0
	}

	binaryLength := *(*uint32)(unsafe.Pointer(datap.buffer))
	datap.buffer += uintptr(4)
	datap.length -= 4
	if datap.length < binaryLength {
		return 0
	}

	out := make([]byte, binaryLength)
	memory.MemCpy(uintptr(unsafe.Pointer(&out[0])), datap.buffer, binaryLength)
	if uintptr(unsafe.Pointer(size)) != uintptr(0) && binaryLength != 0 {
		*size = binaryLength
	}

	datap.buffer += uintptr(binaryLength)
	datap.length -= binaryLength
	return uintptr(unsafe.Pointer(&out[0]))
}

// export BeaconDataParse
func DataParse(datap *datap, buff uintptr, size uint32) uintptr {
	if size <= 0 {
		return 0
	}
	datap.original = buff
	datap.buffer = buff + uintptr(4)
	datap.length = size - 4
	datap.size = size - 4
	return 1
}

// export BeaconDataInt
func DataInt(datap *datap) uintptr {
	value := memory.ReadUIntFromPtr(datap.buffer)
	datap.buffer += uintptr(4)
	datap.length -= 4
	return uintptr(value)
}

func DataLength(datap *datap) uintptr {
	return uintptr(datap.length)
}

// export BeaconDataShort
func DataShort(datap *datap) uintptr {
	if datap.length < 2 {
		return 0
	}

	value := memory.ReadShortFromPtr(datap.buffer)
	datap.buffer += uintptr(2)
	datap.length -= 2
	return uintptr(value)
}

var keyStore = make(map[string]uintptr, 0)

// export BeaconAddValue
func AddValue(key uintptr, ptr uintptr) uintptr {
	sKey := memory.ReadCStringFromPtr(key)
	keyStore[sKey] = ptr
	return uintptr(1)
}

// export BeaconGetValue
func GetValue(key uintptr) uintptr {
	sKey := memory.ReadCStringFromPtr(key)
	if value, exists := keyStore[sKey]; exists {
		return value
	}
	return uintptr(0)
}

// export BeaconRemoveValue
func RemoveValue(key uintptr) uintptr {
	sKey := memory.ReadCStringFromPtr(key)
	if _, exists := keyStore[sKey]; exists {
		delete(keyStore, sKey)
		return uintptr(1)
	}
	return uintptr(0)
}

// export BeaconFormatAlloc
func FormatAllocate(format *formatp, maxsz uint32) uintptr {
	if format == nil {
		return 0
	}

	ptr, _, _ := procLocalAlloc.Call(
		uintptr(0x0040), // LPTR
		uintptr(maxsz),
	)

	if ptr == 0 {
		return 0
	}

	format.original = ptr
	format.buffer = ptr
	format.length = 0
	format.size = maxsz

	return 0
}

// export BeaconFormatReset
func FormatReset(format *formatp) uintptr {
	if format == nil || format.original == 0 {
		return 0
	}

	memory.MemSet(format.original, 0, format.size)
	format.buffer = format.original
	format.length = format.size

	return 0
}

// export BeaconFormatAppend
func FormatAppend(format *formatp, text uintptr, len uint32) uintptr {
	if format == nil || len <= 0 || text == 0 {
		return 0
	}

	available := format.size - format.length
	if len > available {
		len = available
	}

	memory.MemCpy(format.buffer, text, len)
	format.buffer += uintptr(len)
	format.length += len

	return 0
}

// export BeaconFormatFree
func FormatFree(format *formatp) uintptr {
	if format != nil && format.original != 0 {
		windows.LocalFree(windows.Handle(format.original))
		format.original = 0
		format.buffer = 0
		format.length = 0
		format.size = 0
	}

	return 0
}

// export BeaconFormatToString
func FormatToString(format *formatp, size *uint32) uintptr {
	if format == nil || format.original == 0 {
		return 0
	}

	if size != nil {
		*size = format.length
	}

	// Проверяем, что есть место для '\0'
	if format.length >= format.size {
		return format.original // буфер полон, не можем добавить терминатор
	}

	*(*byte)(unsafe.Pointer(format.buffer)) = 0
	format.buffer++
	format.length++

	return format.original
}

// export BeaconFormatInt
func FormatInt(format *formatp, value int32) uintptr {
	if format == nil || format.original == 0 {
		return 0
	}

	if format.length+4 > format.size {
		return 0
	}

	indata := uint32(value)
	outdata := indata

	testint := uint32(0xaabbccdd)
	if *(*byte)(unsafe.Pointer(&testint)) == 0xdd {
		// Little-endian to Big-endian
		outdata = (indata&0xff)<<24 |
			(indata&0xff00)<<8 |
			(indata&0xff0000)>>8 |
			(indata&0xff000000)>>24
	}

	*(*uint32)(unsafe.Pointer(format.buffer)) = outdata

	format.length += 4
	format.buffer += 4

	return 0
}

// export BeaconUseToken
func UseToken(token uintptr) uintptr {
	if token == 0 {
		return 0 // FALSE
	}

	// try SetThreadToken
	ret, _, _ := procSetThreadToken.Call(0, token)
	if ret != 0 {
		bofImpersonate = 2
		return 1 // TRUE
	}

	// try ImpersonateLoggedOnUser
	ret, _, _ = procImpersonateLoggedOnUser.Call(token)
	if ret != 0 {
		bofImpersonate = 2
		return 1 // TRUE
	}

	return 0 // FALSE
}

// export BeaconRevertToken
func RevertToken() {
	procRevertToSelf.Call()
	bofImpersonate = 0
}

// export IsElevate
func IsElevate() uint32 {
	var (
		hToken    uintptr
		elevation TOKEN_ELEVATION
		cbSize    uint32 = uint32(unsafe.Sizeof(elevation))
	)

	// get current process handle
	currentProcess, _, _ := procGetCurrentProcess.Call()

	// OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)
	ret, _, _ := procOpenProcessToken.Call(
		currentProcess,
		TOKEN_QUERY,
		uintptr(unsafe.Pointer(&hToken)),
	)

	if ret == 0 {
		return 0 // FALSE
	}

	// GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)
	ret, _, _ = procGetTokenInformation.Call(
		hToken,
		TokenElevation,
		uintptr(unsafe.Pointer(&elevation)),
		uintptr(unsafe.Sizeof(elevation)),
		uintptr(unsafe.Pointer(&cbSize)),
	)

	// CloseHandle(hToken)
	if hToken != 0 {
		procCloseHandle.Call(hToken)
	}

	if ret == 0 {
		return 0
	}

	return elevation.TokenIsElevated
}

// export IsAdmin
func IsAdmin() uintptr {
	return uintptr(IsElevate())
}

// export toWideChar
func ToWideChar(src uintptr, dst uintptr, max int32) uintptr {
	const sizeofWcharT = 2 // sizeof(wchar_t) in Windows

	if max < sizeofWcharT {
		return 0 // FALSE
	}

	// call MultiByteToWideChar
	// -1 means that src = null-terminated string
	ret, _, _ := procMultiByteToWideChar.Call(
		uintptr(CP_ACP),
		uintptr(MB_ERR_INVALID_CHARS),
		src,
		^uintptr(0), // -1: null-terminated
		dst,
		uintptr(max/sizeofWcharT),
	)

	// MultiByteToWideChar returns number of wchar_t
	// 0 on error is FALSE
	return ret
}

// export AxAddScreenshot
func AxAddScreenshot(channel chan<- interface{}) func(uintptr, uintptr, int) uintptr {
	return func(note uintptr, data uintptr, length int) uintptr {
		if length <= 0 {
			return 0
		}
		outNote := memory.ReadCStringFromPtr(note)
		outData := memory.ReadBytesFromPtr(data, uint32(length))

		buf := new(bytes.Buffer)

		binary.Write(buf, binary.LittleEndian, uint32(len(outNote)))
		buf.Write([]byte(outNote))
		buf.Write([]byte(outData))

		channel <- 0x81 // CALLBACK_AX_SCREENSHOT
		channel <- buf.Bytes()
		return 1
	}
}

// export AxDownloadMemory
func AxDownloadMemory(channel chan<- interface{}) func(uintptr, uintptr, int) uintptr {
	return func(filename uintptr, data uintptr, length int) uintptr {
		if length <= 0 {
			return 0
		}
		outFilename := memory.ReadCStringFromPtr(filename)
		outData := memory.ReadBytesFromPtr(data, uint32(length))

		buf := new(bytes.Buffer)

		binary.Write(buf, binary.LittleEndian, uint32(len(outFilename)))
		buf.Write([]byte(outFilename))
		buf.Write([]byte(outData))

		channel <- 0x82 // CALLBACK_AX_DOWNLOAD_MEM
		channel <- buf.Bytes()
		return 1
	}
}
