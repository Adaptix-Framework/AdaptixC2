#pragma once

#include <Windows.h>
#include "Packer.h"

#define COMMAND_CD		  8
#define COMMAND_CP        12
#define COMMAND_PWD       4
#define COMMAND_TERMINATE 10


Packer* ProcessCommand(BYTE* recv, ULONG recv_size);