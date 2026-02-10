CCX64	= x86_64-w64-mingw32-gcc
CCX86	= i686-w64-mingw32-gcc

CFLAGS	=  -Os -fno-asynchronous-unwind-tables -nostdlib
CFLAGS	+= -fno-ident -fpack-struct=8 -falign-functions=1
CFLAGS	+= -s -ffunction-sections -falign-jumps=1 -w
CFLAGS	+= -falign-labels=1 -fPIC -Wl,-Tscripts/Linker.ld
CFLAGS	+= -Wl,-s,--no-seh,--enable-stdcall-fixup,--image-base,0

OUTPUT_DIR = ../files
BUILD_DIR  = build

EXECUTABLE_X64 = $(BUILD_DIR)/Shellcode.x64.exe
EXECUTABLE_X86 = $(BUILD_DIR)/Shellcode.x86.exe
RAWBINARY_X64  = $(OUTPUT_DIR)/stub.x64.bin
RAWBINARY_X86  = $(OUTPUT_DIR)/stub.x86.bin

all: x64 x86

x64: clean_x64
	@ mkdir -p $(BUILD_DIR)
	@ mkdir -p $(OUTPUT_DIR)

	@ nasm -f win64 asm/x64/start.asm -o $(BUILD_DIR)/Asm.x64.o
	@ $(CCX64) src/*.c $(BUILD_DIR)/Asm.x64.o -o $(EXECUTABLE_X64) $(CFLAGS) $(LFLAGS) -Iinclude -masm=intel

	@ python3 scripts/extract.py -f $(EXECUTABLE_X64) -o $(RAWBINARY_X64)

x86: clean_x86
	@ mkdir -p $(BUILD_DIR)
	@ mkdir -p $(OUTPUT_DIR)

	@ nasm -f win32 asm/x86/start.asm -o $(BUILD_DIR)/Asm.x86.o
	@ $(CCX86) src/*.c $(BUILD_DIR)/Asm.x86.o -o $(EXECUTABLE_X86) $(CFLAGS) $(LFLAGS) -Iinclude -masm=intel

	@ python3 scripts/extract.py -f $(EXECUTABLE_X86) -o $(RAWBINARY_X86)

clean_x64:
	@ rm -f $(BUILD_DIR)/*.x64.o
	@ rm -f $(EXECUTABLE_X64)

clean_x86:
	@ rm -f $(BUILD_DIR)/*.x86.o
	@ rm -f $(EXECUTABLE_X86)

clean:
	@ rm -rf $(BUILD_DIR)
	@ echo "[*] Cleaned build directory"
