#pragma once

#include "beacon.h"

#define CALLBACK_AX_SCREENSHOT 0x81

void AxAddScreenshot(char* note, char* data, int len);
