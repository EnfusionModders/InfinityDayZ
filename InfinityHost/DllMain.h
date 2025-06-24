#pragma once

#include <windows.h>
#include <stdio.h>
#include <intrin.h>
#include "exports.h"

#pragma intrinsic(_ReturnAddress)

BOOL WINAPI DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved);