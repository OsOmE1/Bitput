#pragma once
#include <ntifs.h>
#include <ntdef.h>
#include <minwindef.h>
#include <ntstrsafe.h>
#include "Ntddmou.h"
#include "Ntddkbd.h"
#include "Kbdmou.h"
#include "utility.h"
#include "input.h"

#define IO_TEST CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0420, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)

NTSTATUS IoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS CloseCall(PDEVICE_OBJECT DeviceObject, PIRP irp);
NTSTATUS CreateCall(PDEVICE_OBJECT DeviceObject, PIRP irp);

typedef struct _KERNEL_PING
{
	DWORD SysIntr;
	char* name;

} KERNEL_PING, * PKERNEL_PING;