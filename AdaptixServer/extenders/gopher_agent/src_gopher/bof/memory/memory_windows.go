// Ref: https://github.com/praetorian-inc/goffloader

package memory

import (
	"unicode/utf16"
	"unsafe"
)

func MemCpy(dst, src uintptr, n uint32) {
	if n <= 0 {
		return
	}

	dstSlice := unsafe.Slice((*byte)(unsafe.Pointer(dst)), n)
	srcSlice := unsafe.Slice((*byte)(unsafe.Pointer(src)), n)
	copy(dstSlice, srcSlice)
}

func MemSet(dst uintptr, val byte, n uint32) {
	if n <= 0 {
		return
	}

	dstSlice := unsafe.Slice((*byte)(unsafe.Pointer(dst)), n)
	for i := range dstSlice {
		dstSlice[i] = val
	}
}

func ReadBytesFromPtr(src uintptr, length uint32) []byte {
	out := make([]byte, length)
	MemCpy(uintptr(unsafe.Pointer(&out[0])), src, length)
	return out
}

func ReadUIntFromPtr(src uintptr) uint32 {
	return *(*uint32)(unsafe.Pointer(src))
}

func ReadShortFromPtr(src uintptr) uint16 {
	return *(*uint16)(unsafe.Pointer(src))
}

func ReadCStringFromPtr(src uintptr) string {
	if src == 0 {
		return ""
	}
	str := ""
	offset := 0
	for {
		c := *(*byte)(unsafe.Pointer(src + uintptr(offset)))
		if c == 0 {
			break
		}
		str += string(c)
		offset++
	}
	return str
}

func ReadWStringFromPtr(src uintptr) string {
	if src == 0 {
		return ""
	}
	var codeUnits []uint16
	offset := uintptr(0)
	for {
		c := *(*uint16)(unsafe.Pointer(src + offset))
		if c == 0 {
			break
		}
		codeUnits = append(codeUnits, c)
		offset += 2
	}
	return string(utf16.Decode(codeUnits))
}
