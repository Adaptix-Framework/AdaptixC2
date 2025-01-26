SOURCES := $(wildcard beacon/*.cpp)

OBJECTS_X64 := $(patsubst beacon/%.cpp, objects/%.x64.o, $(SOURCES))
OBJECTS_X86 := $(patsubst beacon/%.cpp, objects/%.x86.o, $(SOURCES))

BEACON_DIR := "beacon"
DIST_DIR := "objects"
FILES_DIR := "files"



all: clean pre x64 x86
	@ cp $(FILES_DIR)/config.tpl $(DIST_DIR)/config.cpp
	@ cp $(FILES_DIR)/stub.x64.bin $(DIST_DIR)/stub.x64.bin
	@ cp $(FILES_DIR)/stub.x86.bin $(DIST_DIR)/stub.x86.bin

pre:
	@ mkdir objects

x64: $(OBJECTS_X64)
	@ x86_64-w64-mingw32-g++ -c -I $(BEACON_DIR) -fno-ident -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables -fno-strict-overflow -fno-delete-null-pointer-checks -fpermissive -w -masm=intel -fPIC $(BEACON_DIR)/main.cpp -D BUILD_SVC -o $(DIST_DIR)/main_service.x64.o
	@ x86_64-w64-mingw32-g++ -c -I $(BEACON_DIR) -fno-ident -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables -fno-strict-overflow -fno-delete-null-pointer-checks -fpermissive -w -masm=intel -fPIC $(BEACON_DIR)/main.cpp -D BUILD_DLL -o $(DIST_DIR)/main_dll.x64.o
	@ x86_64-w64-mingw32-g++ -c -I $(BEACON_DIR) -fno-ident -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables -fno-strict-overflow -fno-delete-null-pointer-checks -fpermissive -w -masm=intel -fPIC $(BEACON_DIR)/main.cpp -D BUILD_SHELLCODE -o $(DIST_DIR)/main_shellcode.x64.o

x86: $(OBJECTS_X86)
	@ i686-w64-mingw32-g++ -c -I $(BEACON_DIR) -fno-ident -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables -fno-strict-overflow -fno-delete-null-pointer-checks -fpermissive -w -masm=intel -fPIC $(BEACON_DIR)/main.cpp -D BUILD_SVC -o $(DIST_DIR)/main_service.x86.o
	@ i686-w64-mingw32-g++ -c -I $(BEACON_DIR) -fno-ident -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables -fno-strict-overflow -fno-delete-null-pointer-checks -fpermissive -w -masm=intel -fPIC $(BEACON_DIR)/main.cpp -D BUILD_DLL -o $(DIST_DIR)/main_dll.x86.o
	@ i686-w64-mingw32-g++ -c -I $(BEACON_DIR) -fno-ident -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables -fno-strict-overflow -fno-delete-null-pointer-checks -fpermissive -w -masm=intel -fPIC $(BEACON_DIR)/main.cpp -D BUILD_SHELLCODE -o $(DIST_DIR)/main_shellcode.x86.o

objects/%.x64.o: beacon/%.cpp
	@ x86_64-w64-mingw32-g++ -c -I $(BEACON_DIR) -fno-ident -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables -fno-strict-overflow -fno-delete-null-pointer-checks -fpermissive -w -masm=intel -fPIC -c $< -o $@

objects/%.x86.o: beacon/%.cpp
	@ i686-w64-mingw32-g++ -c -I $(BEACON_DIR) -fno-ident -fno-stack-protector -fno-exceptions -fno-asynchronous-unwind-tables -fno-strict-overflow -fno-delete-null-pointer-checks -fpermissive -w -masm=intel -fPIC -c $< -o $@

clean:
	@ rm -rf $(DIST_DIR)
