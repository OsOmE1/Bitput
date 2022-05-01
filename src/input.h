#pragma once
#pragma warning( disable : 4133)

#define _NO_CRT_STDIO_INLINE
#include "Ntifs.h"
#include "Ntddmou.h"
#include "Ntddkbd.h"
#include "Kbdmou.h"
#include <stdio.h>

#define KeyBoardTag	'KYBD'
#define MouseTag	'MOUS'
#define POOL_SIZE 0x1000

#define KBDCLASS_CONNECT_REQUEST 0x0B0203
#define MOUCLASS_CONNECT_REQUEST 0x0F0203
#define MOU_STRING_INC 0x14
#define KBD_STRING_INC 0x15

extern MOUSE_INPUT_DATA mouseData;
extern KEYBOARD_INPUT_DATA keyboardData;

typedef NTSTATUS(*IRPMJREAD) (_In_ PDEVICE_OBJECT, _In_ PIRP);

typedef void(__fastcall* KeyboardServiceDpc)(PDEVICE_OBJECT kbd, PKEYBOARD_INPUT_DATA a1, PKEYBOARD_INPUT_DATA a2, PULONG a3);
typedef void(__fastcall* MouseServiceDpc)(PDEVICE_OBJECT mou, PMOUSE_INPUT_DATA a1, PMOUSE_INPUT_DATA a2, PULONG a3);

typedef NTSTATUS(__fastcall* KeyboardInput)(void* a1, void* a2, void* a3, void* a4, void* a5);
typedef NTSTATUS(__fastcall* MouseInput)(void* a1, void* a2, void* a3, void* a4, void* a5);

typedef NTSTATUS(__fastcall* KeyboardAddDevice)(PDRIVER_OBJECT a1, PDEVICE_OBJECT a2);
typedef NTSTATUS(__fastcall* MouseAddDevice)(PDRIVER_OBJECT a1, PDEVICE_OBJECT a2);
typedef NTSTATUS(__fastcall* KeyboardClassRead)(PDEVICE_OBJECT device, PIRP irp);
typedef NTSTATUS(__fastcall* MouseClassRead)(PDEVICE_OBJECT device, PIRP irp);

void SynthesizeMouse(PMOUSE_INPUT_DATA a1);
int GetMouseState(int key);
void SynthesizeKeyboard(PKEYBOARD_INPUT_DATA a1);
int GetKeyState(unsigned char scan);

NTSTATUS KeyboardApc(void* a1, void* a2, void* a3, void* a4, void* a5);
NTSTATUS MouseApc(void* a1, void* a2, void* a3, void* a4, void* a5);

NTSTATUS InternalIoControl(PDEVICE_OBJECT driverObj, PIRP irp);
NTSTATUS CreateMouse(_In_ PDRIVER_OBJECT driverObj);
NTSTATUS HookMouse(_In_ PDRIVER_OBJECT driverObj);
NTSTATUS MouseProc(_In_ PDEVICE_OBJECT driverObj, _In_ PIRP Irp);
NTSTATUS MouseInternalIoctl(PDEVICE_OBJECT device, PIRP irp);
NTSTATUS CloseMouse(_In_ PDRIVER_OBJECT driverObj);

NTSTATUS CreateKeyboard(_In_ PDRIVER_OBJECT driverObj);
NTSTATUS HookKeyboard(_In_ PDRIVER_OBJECT driverObj);
NTSTATUS KeyboardProc(_In_ PDEVICE_OBJECT driverObj, _In_ PIRP irp);
NTSTATUS KeyboardInternalIoctl(PDEVICE_OBJECT device, PIRP irp);
NTSTATUS CloseKeyboard(_In_ PDRIVER_OBJECT driverObj);

struct DEVOBJ_EXTENSION_FIX {
	USHORT type;
	USHORT size;
	PDEVICE_OBJECT devObj;
	ULONGLONG PowerFlags;
	void* unk;
	ULONGLONG ExtensionFlags;
	void* DeviceNode;
	PDEVICE_OBJECT AttachedTo;
};

typedef struct _DIRECTORY_BASIC_INFORMATION {
	UNICODE_STRING ObjectName;
	UNICODE_STRING ObjectTypeName;

} DIRECTORY_BASIC_INFORMATION, * PDIRECTORY_BASIC_INFORMATION;

NTSYSAPI
NTSTATUS
NTAPI
ZwOpenDirectoryObject(
	_Out_ PHANDLE DirectoryHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES
	ObjectAttributes
);

typedef NTSTATUS(NTAPI* PNTOPENDIRECTORYOBJECT)(
	_Out_ PHANDLE DirectoryHandle,
	_In_ ACCESS_MASK DesiredAccess,
	_In_ POBJECT_ATTRIBUTES
	ObjectAttributes
	);
NTSYSAPI
NTSTATUS
NTAPI
ZwQueryDirectoryObject(
	_In_ HANDLE  DirectoryHandle,
	_Out_ PVOID  Buffer,
	_In_ ULONG   BufferLength,
	_In_ BOOLEAN ReturnSingleEntry,
	_In_ BOOLEAN RestartScan,
	_In_ _Out_ PULONG  Context,
	_Out_ PULONG ReturnLength OPTIONAL
);

typedef NTSTATUS(NTAPI* PNTQUERYDIRECTORYOBJECT)(
	_In_ HANDLE  DirectoryHandle,
	_Out_ PVOID  Buffer,
	_In_ ULONG   BufferLength,
	_In_ BOOLEAN ReturnSingleEntry,
	_In_ BOOLEAN RestartScan,
	_In_ _Out_ PULONG  Context,
	_Out_ PULONG ReturnLength OPTIONAL
	);