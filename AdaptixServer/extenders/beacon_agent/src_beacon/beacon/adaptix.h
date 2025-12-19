#pragma once

#include "beacon.h"

#define CALLBACK_AX_SCREENSHOT   0x81
#define CALLBACK_AX_DOWNLOAD_MEM 0x82
#define CALLBACK_AX_TARGET_ADD   0x83

void AxAddScreenshot(char* note, char* data, int len);

void AxDownloadMemory(char* filename, char* data, int len);

void AxAddTarget(char* computer, char* domain, char* address, int os, char* os_desk, char* tag, char* info, int alive);