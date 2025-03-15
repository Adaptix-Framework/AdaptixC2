SOURCES := $(wildcard beacon/*.cpp)

OBJECTS_X64 := $(patsubst beacon/%.cpp, objects/%.x64.o, $(SOURCES))
OBJECTS_X86 := $(patsubst beacon/%.cpp, objects/%.x86.o, $(SOURCES))

BEACON_DIR := "beacon"
DIST_DIR := "objects"
FILES_DIR := "files"

# Detect CPU cores for parallel compilation
NPROC := $(shell nproc)

# Security-related compilation flags
SECURITY_FLAGS := -fno-stack-protector \
                 -fno-strict-overflow \
                 -fno-delete-null-pointer-checks \
                 -fno-strict-aliasing \
                 -fno-builtin

# Optimization and debugging flags
OPTIMIZATION_FLAGS := -O1 \
                     -fomit-frame-pointer \
                     -fno-exceptions \
                     -fno-unwind-tables \
                     -fno-asynchronous-unwind-tables

# Common compilation flags
COMMON_FLAGS := -I $(BEACON_DIR) \
                -fpermissive \
                -w \
                -masm=intel \
                -fPIC \
                $(SECURITY_FLAGS) \
                $(OPTIMIZATION_FLAGS)

.PHONY: all clean pre x64 x86 info multi

# Force directory creation before compilation starts
$(shell mkdir -p objects)

# Set the number of jobs for parallel compilation
MAKEFLAGS += -j$(NPROC)

all: clean
	@echo "\033[1;32m  [+] Starting project compilation, using $(NPROC) CPU cores for parallel compilation\033[0m"
	@$(MAKE) x64 x86
	@ cp $(FILES_DIR)/config.tpl $(DIST_DIR)/config.cpp
	@ cp $(FILES_DIR)/stub.x64.bin $(DIST_DIR)/stub.x64.bin
	@ cp $(FILES_DIR)/stub.x86.bin $(DIST_DIR)/stub.x86.bin
	@echo "\033[1;32m  [+] Compilation complete\033[0m"

# Parallel compilation of x64 object files
x64: $(OBJECTS_X64)
	@echo "\033[1;34m  [*] Compiling x64 object files\033[0m"
	@ $(MAKE) -j$(NPROC) main_x64

main_x64:
	@ x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BUILD_SVC -o $(DIST_DIR)/main_service.x64.o
	@ x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BUILD_DLL -o $(DIST_DIR)/main_dll.x64.o
	@ x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BUILD_SHELLCODE -o $(DIST_DIR)/main_shellcode.x64.o

# Parallel compilation of x86 object files
x86: $(OBJECTS_X86)
	@echo "\033[1;34m  [*] Compiling x86 object files\033[0m"
	@ $(MAKE) -j$(NPROC) main_x86

main_x86:
	@ i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BUILD_SVC -o $(DIST_DIR)/main_service.x86.o
	@ i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BUILD_DLL -o $(DIST_DIR)/main_dll.x86.o
	@ i686-w64-mingw32-g++ -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BUILD_SHELLCODE -o $(DIST_DIR)/main_shellcode.x86.o

# Parallel compilation rules
objects/%.x64.o: beacon/%.cpp
	@ x86_64-w64-mingw32-g++ -c $(COMMON_FLAGS) -c $< -o $@

objects/%.x86.o: beacon/%.cpp
	@ i686-w64-mingw32-g++ -c $(COMMON_FLAGS) -c $< -o $@

clean:
	@echo "\033[1;33m  [*] Cleaning object files...\033[0m"
	@ rm -rf $(DIST_DIR)
	@ mkdir -p $(DIST_DIR)

info:
	@echo "Using $(NPROC) CPU cores for parallel compilation"
	@echo "Source files: $(SOURCES)"
	@echo "X64 objects: $(OBJECTS_X64)"
	@echo "X86 objects: $(OBJECTS_X86)"
	@echo "Security flags: $(SECURITY_FLAGS)"
	@echo "Optimization flags: $(OPTIMIZATION_FLAGS)"
	