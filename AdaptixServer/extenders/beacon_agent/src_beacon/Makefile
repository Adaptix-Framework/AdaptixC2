SOURCES := $(filter-out beacon/config.cpp, $(wildcard beacon/*.cpp))

CXX_X64 := x86_64-w64-mingw32-g++
CXX_X86 := i686-w64-mingw32-g++
ASM_SYNTAX := -masm=intel

BEACON_DIR := "beacon"
FILES_DIR := "files"
HTTP_DIST_DIR := "objects_http"
SMB_DIST_DIR := "objects_smb"
TCP_DIST_DIR := "objects_tcp"
DNS_DIST_DIR := "objects_dns"

HTTP_OBJECTS_X64 := $(patsubst beacon/%.cpp, $(HTTP_DIST_DIR)/%.x64.o, $(SOURCES))
HTTP_OBJECTS_X86 := $(patsubst beacon/%.cpp, $(HTTP_DIST_DIR)/%.x86.o, $(SOURCES))

SMB_OBJECTS_X64 := $(patsubst beacon/%.cpp, $(SMB_DIST_DIR)/%.x64.o, $(SOURCES))
SMB_OBJECTS_X86 := $(patsubst beacon/%.cpp, $(SMB_DIST_DIR)/%.x86.o, $(SOURCES))

TCP_OBJECTS_X64 := $(patsubst beacon/%.cpp, $(TCP_DIST_DIR)/%.x64.o, $(SOURCES))
TCP_OBJECTS_X86 := $(patsubst beacon/%.cpp, $(TCP_DIST_DIR)/%.x86.o, $(SOURCES))

DNS_OBJECTS_X64 := $(patsubst beacon/%.cpp, $(DNS_DIST_DIR)/%.x64.o, $(SOURCES))
DNS_OBJECTS_X86 := $(patsubst beacon/%.cpp, $(DNS_DIST_DIR)/%.x86.o, $(SOURCES))

SECURITY_FLAGS := -fno-stack-protector \
                 -fno-strict-overflow \
                 -fno-delete-null-pointer-checks \
                 -fno-strict-aliasing \
                 -fno-builtin

OPTIMIZATION_FLAGS := -fno-exceptions \
                     -fno-unwind-tables \
                     -fno-asynchronous-unwind-tables

COMMON_FLAGS := -I $(BEACON_DIR) \
                -fpermissive \
                -w \
                $(ASM_SYNTAX) \
                -fPIC \
                $(SECURITY_FLAGS) \
                $(OPTIMIZATION_FLAGS)

# Miniz trim: disable stdio/archive/time/assert to eliminate CRT dependencies.
# Keeps zlib compress/uncompress for DnsCodec.
MINIZ_TRIM_FLAGS := -DMINIZ_NO_STDIO -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_ARCHIVE_WRITING_APIS -DMINIZ_NO_TIME -DMINIZ_NO_ASSERT

.PHONY: all clean pre x64 x86 shellcode

NPROC := $(shell nproc)
ifeq ($(MAKELEVEL), 0)
    MAKEFLAGS += -j$(NPROC) --no-print-directory
endif

all: clean shellcode pre x64 x86
shellcode: clean
pre: shellcode
x64: pre
x86: pre

pre:
	@mkdir -p $(HTTP_DIST_DIR) $(SMB_DIST_DIR) $(TCP_DIST_DIR) $(DNS_DIST_DIR)
	@ # http
	@ cp $(FILES_DIR)/config.tpl $(HTTP_DIST_DIR)/config.cpp
	@ cp $(FILES_DIR)/stub.x64.bin $(HTTP_DIST_DIR)/stub.x64.bin
	@ cp $(FILES_DIR)/stub.x86.bin $(HTTP_DIST_DIR)/stub.x86.bin
	@ # smb
	@ cp $(FILES_DIR)/config.tpl $(SMB_DIST_DIR)/config.cpp
	@ cp $(FILES_DIR)/stub.x64.bin $(SMB_DIST_DIR)/stub.x64.bin
	@ cp $(FILES_DIR)/stub.x86.bin $(SMB_DIST_DIR)/stub.x86.bin
	@ # tcp
	@ cp $(FILES_DIR)/config.tpl $(TCP_DIST_DIR)/config.cpp
	@ cp $(FILES_DIR)/stub.x64.bin $(TCP_DIST_DIR)/stub.x64.bin
	@ cp $(FILES_DIR)/stub.x86.bin $(TCP_DIST_DIR)/stub.x86.bin
	@ # dns
	@ cp $(FILES_DIR)/config.tpl $(DNS_DIST_DIR)/config.cpp
	@ cp $(FILES_DIR)/stub.x64.bin $(DNS_DIST_DIR)/stub.x64.bin
	@ cp $(FILES_DIR)/stub.x86.bin $(DNS_DIST_DIR)/stub.x86.bin

shellcode:
	@ (cd shellcode && $(MAKE) --no-print-directory)

clean:
	@rm -rf $(HTTP_DIST_DIR) $(SMB_DIST_DIR) $(TCP_DIST_DIR) $(DNS_DIST_DIR)
	@mkdir -p $(HTTP_DIST_DIR) $(SMB_DIST_DIR) $(TCP_DIST_DIR) $(DNS_DIST_DIR)

x64: $(HTTP_OBJECTS_X64) $(SMB_OBJECTS_X64) $(TCP_OBJECTS_X64) $(DNS_OBJECTS_X64)
	@ # http
	@$(CXX_X64) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_HTTP -D BUILD_SVC -o $(HTTP_DIST_DIR)/main_service.x64.o
	@$(CXX_X64) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_HTTP -D BUILD_DLL -o $(HTTP_DIST_DIR)/main_dll.x64.o
	@$(CXX_X64) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_HTTP -D BUILD_SHELLCODE -o $(HTTP_DIST_DIR)/main_shellcode.x64.o
	@rm -f $(HTTP_DIST_DIR)/ConnectorSMB.x64.o $(HTTP_DIST_DIR)/ConnectorTCP.x64.o
	@ # smb
	@$(CXX_X64) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_SMB -D BUILD_SVC -o $(SMB_DIST_DIR)/main_service.x64.o
	@$(CXX_X64) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_SMB -D BUILD_DLL -o $(SMB_DIST_DIR)/main_dll.x64.o
	@$(CXX_X64) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_SMB -D BUILD_SHELLCODE -o $(SMB_DIST_DIR)/main_shellcode.x64.o
	@rm -f $(SMB_DIST_DIR)/ConnectorHTTP.x64.o $(SMB_DIST_DIR)/ConnectorTCP.x64.o
	@ # tcp
	@$(CXX_X64) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_SMB -D BUILD_SVC -o $(TCP_DIST_DIR)/main_service.x64.o
	@$(CXX_X64) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_SMB -D BUILD_DLL -o $(TCP_DIST_DIR)/main_dll.x64.o
	@$(CXX_X64) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_SMB -D BUILD_SHELLCODE -o $(TCP_DIST_DIR)/main_shellcode.x64.o
	@rm -f $(TCP_DIST_DIR)/ConnectorHTTP.x64.o $(TCP_DIST_DIR)/ConnectorSMB.x64.o
	@ # dns
	@$(CXX_X64) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_DNS -D BUILD_SVC -o $(DNS_DIST_DIR)/main_service.x64.o
	@$(CXX_X64) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_DNS -D BUILD_DLL -o $(DNS_DIST_DIR)/main_dll.x64.o
	@$(CXX_X64) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_DNS -D BUILD_SHELLCODE -o $(DNS_DIST_DIR)/main_shellcode.x64.o
	@rm -f $(DNS_DIST_DIR)/ConnectorHTTP.x64.o $(DNS_DIST_DIR)/ConnectorSMB.x64.o $(DNS_DIST_DIR)/ConnectorTCP.x64.o

x86: $(HTTP_OBJECTS_X86) $(SMB_OBJECTS_X86) $(TCP_OBJECTS_X86) $(DNS_OBJECTS_X86)
	@ # http
	@$(CXX_X86) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_HTTP -D BUILD_SVC -o $(HTTP_DIST_DIR)/main_service.x86.o
	@$(CXX_X86) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_HTTP -D BUILD_DLL -o $(HTTP_DIST_DIR)/main_dll.x86.o
	@$(CXX_X86) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_HTTP -D BUILD_SHELLCODE -o $(HTTP_DIST_DIR)/main_shellcode.x86.o
	@rm -f $(HTTP_DIST_DIR)/ConnectorSMB.x86.o $(HTTP_DIST_DIR)/ConnectorTCP.x86.o
	@ # smb
	@$(CXX_X86) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_SMB -D BUILD_SVC -o $(SMB_DIST_DIR)/main_service.x86.o
	@$(CXX_X86) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_SMB -D BUILD_DLL -o $(SMB_DIST_DIR)/main_dll.x86.o
	@$(CXX_X86) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_SMB -D BUILD_SHELLCODE -o $(SMB_DIST_DIR)/main_shellcode.x86.o
	@rm -f $(SMB_DIST_DIR)/ConnectorHTTP.x86.o $(SMB_DIST_DIR)/ConnectorTCP.x86.o
	@ # tcp
	@$(CXX_X86) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_SMB -D BUILD_SVC -o $(TCP_DIST_DIR)/main_service.x86.o
	@$(CXX_X86) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_SMB -D BUILD_DLL -o $(TCP_DIST_DIR)/main_dll.x86.o
	@$(CXX_X86) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_SMB -D BUILD_SHELLCODE -o $(TCP_DIST_DIR)/main_shellcode.x86.o
	@rm -f $(TCP_DIST_DIR)/ConnectorHTTP.x86.o $(TCP_DIST_DIR)/ConnectorSMB.x86.o
	@ # dns
	@$(CXX_X86) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_DNS -D BUILD_SVC -o $(DNS_DIST_DIR)/main_service.x86.o
	@$(CXX_X86) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D_WIN32_WINNT=0x0600 -D BEACON_DNS -D BUILD_DLL -o $(DNS_DIST_DIR)/main_dll.x86.o
	@$(CXX_X86) -c $(COMMON_FLAGS) $(BEACON_DIR)/main.cpp -D BEACON_DNS -D BUILD_SHELLCODE -o $(DNS_DIST_DIR)/main_shellcode.x86.o
	@rm -f $(DNS_DIST_DIR)/ConnectorHTTP.x86.o $(DNS_DIST_DIR)/ConnectorSMB.x86.o $(DNS_DIST_DIR)/ConnectorTCP.x86.o


$(HTTP_DIST_DIR)/%.x64.o: beacon/%.cpp
	@$(CXX_X64) -c $(COMMON_FLAGS) -D BEACON_HTTP -c $< -o $@

$(HTTP_DIST_DIR)/%.x86.o: beacon/%.cpp
	@$(CXX_X86) -c $(COMMON_FLAGS) -D BEACON_HTTP -c $< -o $@



$(SMB_DIST_DIR)/%.x64.o: beacon/%.cpp
	@$(CXX_X64) -c $(COMMON_FLAGS) -D BEACON_SMB -c $< -o $@

$(SMB_DIST_DIR)/%.x86.o: beacon/%.cpp
	@$(CXX_X86) -c $(COMMON_FLAGS) -D BEACON_SMB -c $< -o $@



$(TCP_DIST_DIR)/%.x64.o: beacon/%.cpp
	@$(CXX_X64) -c $(COMMON_FLAGS) -D BEACON_TCP -c $< -o $@

$(TCP_DIST_DIR)/%.x86.o: beacon/%.cpp
	@$(CXX_X86) -c $(COMMON_FLAGS) -D BEACON_TCP -c $< -o $@



$(DNS_DIST_DIR)/%.x64.o: beacon/%.cpp
	@$(CXX_X64) -c $(COMMON_FLAGS) -D BEACON_DNS -c $< -o $@

$(DNS_DIST_DIR)/%.x86.o: beacon/%.cpp
	@$(CXX_X86) -c $(COMMON_FLAGS) -D BEACON_DNS -c $< -o $@

# Miniz: compile with trim macros for ALL beacon types to eliminate CRT deps
$(HTTP_DIST_DIR)/miniz.x64.o: beacon/miniz.cpp
	@$(CXX_X64) -c $(COMMON_FLAGS) -D BEACON_HTTP $(MINIZ_TRIM_FLAGS) -c $< -o $@

$(HTTP_DIST_DIR)/miniz.x86.o: beacon/miniz.cpp
	@$(CXX_X86) -c $(COMMON_FLAGS) -D BEACON_HTTP $(MINIZ_TRIM_FLAGS) -c $< -o $@

$(SMB_DIST_DIR)/miniz.x64.o: beacon/miniz.cpp
	@$(CXX_X64) -c $(COMMON_FLAGS) -D BEACON_SMB $(MINIZ_TRIM_FLAGS) -c $< -o $@

$(SMB_DIST_DIR)/miniz.x86.o: beacon/miniz.cpp
	@$(CXX_X86) -c $(COMMON_FLAGS) -D BEACON_SMB $(MINIZ_TRIM_FLAGS) -c $< -o $@

$(TCP_DIST_DIR)/miniz.x64.o: beacon/miniz.cpp
	@$(CXX_X64) -c $(COMMON_FLAGS) -D BEACON_TCP $(MINIZ_TRIM_FLAGS) -c $< -o $@

$(TCP_DIST_DIR)/miniz.x86.o: beacon/miniz.cpp
	@$(CXX_X86) -c $(COMMON_FLAGS) -D BEACON_TCP $(MINIZ_TRIM_FLAGS) -c $< -o $@

$(DNS_DIST_DIR)/miniz.x64.o: beacon/miniz.cpp
	@$(CXX_X64) -c $(COMMON_FLAGS) -D BEACON_DNS $(MINIZ_TRIM_FLAGS) -c $< -o $@

$(DNS_DIST_DIR)/miniz.x86.o: beacon/miniz.cpp
	@$(CXX_X86) -c $(COMMON_FLAGS) -D BEACON_DNS $(MINIZ_TRIM_FLAGS) -c $< -o $@
