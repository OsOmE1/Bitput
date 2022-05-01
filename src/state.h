#pragma once
#include <ntddk.h>
#include "vector.h"

UNICODE_STRING deviceName, dosDeviceName;
PDEVICE_OBJECT device;
ULONG* protectedProcessList;
// #define MANUALMAPPED