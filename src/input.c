#pragma warning( disable : 4113 4100 4996 4701 4024 4047)
#include "input.h"

// this whole file has to be refactored in the future

PDEVICE_OBJECT mouseDevice, keyboardDevice;
PVOID mouseDeviceNode, keyboardDeviceNode;
MOUSE_INPUT_DATA mouseData;
KEYBOARD_INPUT_DATA keyboardData;
char MouseState[5], KeyboardState[256];
PMOUSE_INPUT_DATA mouseIrp;
PKEYBOARD_INPUT_DATA keyboardIrp;

IRPMJREAD oMouseReadFunc = NULL, oInternalMouseDeviceFunc = NULL;
IRPMJREAD oKeyboardReadFunc = NULL, oInternalKeyboardDeviceFunc = NULL;

KeyboardServiceDpc KeyboardDpcRoutine;
MouseServiceDpc MouseDpcRoutine;

KeyboardInput KeyboardInputRoutine = NULL;
MouseInput MouseInputRoutine = NULL;
ULONGLONG* mouseRoutine = NULL, * keyboardRoutine = NULL;
ULONG keyboardId = 0, mouseId = 0;

int GetKeyState(unsigned char scan) {
	if (KeyboardState[scan - 1]) return 1;
	return 0;
}

void SynthesizeKeyboard(PKEYBOARD_INPUT_DATA data) {
	KIRQL irql;
	char* endptr;
	ULONG fill = 1;

	endptr = (char*)data;
	endptr += sizeof(KEYBOARD_INPUT_DATA);
	data->UnitId = (USHORT)keyboardId;

	KeRaiseIrql(DISPATCH_LEVEL, &irql);

	if (KeyboardDpcRoutine) KeyboardDpcRoutine(keyboardDevice, data, (PKEYBOARD_INPUT_DATA)endptr, &fill);

	KeLowerIrql(irql);
}

int GetMouseState(int key) {
	if (MouseState[key]) return 1;
	return 0;
}

void SynthesizeMouse(PMOUSE_INPUT_DATA data) {
	KIRQL irql;
	char* endptr;
	ULONG fill = 1;

	endptr = (char*)data;
	endptr += sizeof(MOUSE_INPUT_DATA);
	data->UnitId = (USHORT)mouseId;

	KeRaiseIrql(DISPATCH_LEVEL, &irql);

	if (MouseDpcRoutine) MouseDpcRoutine(mouseDevice, data, (PMOUSE_INPUT_DATA)endptr, &fill);

	KeLowerIrql(irql);
}

NTSTATUS InternalIoControl(PDEVICE_OBJECT deviceObj, PIRP irp) {
	NTSTATUS Status = STATUS_SUCCESS;

	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
	ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;

	switch (code) {
	case (KBDCLASS_CONNECT_REQUEST): {
		PCONNECT_DATA cd = stack->Parameters.DeviceIoControl.Type3InputBuffer;
		KeyboardDpcRoutine = (KeyboardServiceDpc)cd->ClassService;
		break;
	}
	case (MOUCLASS_CONNECT_REQUEST): {
		PCONNECT_DATA cd = stack->Parameters.DeviceIoControl.Type3InputBuffer;
		MouseDpcRoutine = (MouseServiceDpc)cd->ClassService;
		break;
	}
	default: {
		Status = STATUS_INVALID_PARAMETER;
		break;
	}
	}

	return Status;
}

void FindDevNodeRecurse(PDEVICE_OBJECT deviceObj, ULONGLONG* node) {
	struct DEVOBJ_EXTENSION_FIX* attachment;
	attachment = deviceObj->DeviceObjectExtension;

	if ((!attachment->AttachedTo) && (!attachment->DeviceNode)) return;

	if ((!attachment->DeviceNode) && (attachment->AttachedTo)) {
		FindDevNodeRecurse(attachment->AttachedTo, node);
		return;
	}
	*node = (ULONGLONG)attachment->DeviceNode;

	return;
}

NTSTATUS BitputIoGetDeviceObjectPointer(_In_ PUNICODE_STRING objName, _In_ ACCESS_MASK desiredAccess, _Out_ PFILE_OBJECT* fileObj, _Out_ PDEVICE_OBJECT* deviceObj) {
	PFILE_OBJECT fileObject;
	OBJECT_ATTRIBUTES objectAttributes;
	HANDLE fileHandle;
	IO_STATUS_BLOCK ioStatus;
	InitializeObjectAttributes(&objectAttributes, objName, OBJ_KERNEL_HANDLE, (HANDLE)NULL, (PSECURITY_DESCRIPTOR)NULL);
	NTSTATUS status = ZwOpenFile(&fileHandle, desiredAccess, &objectAttributes, &ioStatus, FILE_SHARE_READ, FILE_NON_DIRECTORY_FILE);
	if (NT_SUCCESS(status)) {
		status = ObReferenceObjectByHandle(fileHandle, 0, *IoFileObjectType, KernelMode, (PVOID*)&fileObject, NULL);
		if (NT_SUCCESS(status)) {
			*fileObj = fileObject;
			*deviceObj = IoGetBaseFileSystemDeviceObject(fileObject);
		}
		ZwClose(fileHandle);
	}
	return status;
}

NTSTATUS CreateMouse(_In_ PDRIVER_OBJECT driverObj) {
	NTSTATUS status = STATUS_SUCCESS;

	memset((void*)&mouseData, 0, sizeof(mouseData));
	memset((void*)MouseState, 0, sizeof(MouseState));

	UNICODE_STRING mouseDeviceName;
	RtlInitUnicodeString(&mouseDeviceName, L"\\Device\\bitputMouse");
	status = IoCreateDevice(driverObj, 0, &mouseDeviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &mouseDevice);
	if (!NT_SUCCESS(status)) {
		return status;
	}
	mouseDevice->Flags |= DO_BUFFERED_IO;
	mouseDevice->Flags &= ~DO_DEVICE_INITIALIZING;


	status = HookMouse(driverObj);
	if (!NT_SUCCESS(status)) {
		mouseDevice = NULL;
		return status;
	}

	ULONGLONG node = 0;
	FindDevNodeRecurse(mouseDevice, &node);
	if (mouseDevice->DeviceObjectExtension) {
		mouseDeviceNode = mouseDevice->DeviceObjectExtension->DeviceNode;
		mouseDevice->DeviceObjectExtension->DeviceNode = (PVOID)node;
	}

	if (!mouseDevice->DriverObject) return status;
	if (!mouseDevice->DriverObject->DriverExtension) return status;

	MouseAddDevice MouseAddDevicePtr;
	MouseAddDevicePtr = (MouseAddDevice)mouseDevice->DriverObject->DriverExtension->AddDevice;

	if (MouseAddDevicePtr) {
		MouseAddDevicePtr(mouseDevice->DriverObject, mouseDevice);
	}
	return status;
}

NTSTATUS HookMouse(_In_ PDRIVER_OBJECT driverObj) {
	NTSTATUS status = STATUS_SUCCESS;

	UNICODE_STRING dirName;
	RtlInitUnicodeString(&dirName, L"\\Device");

	OBJECT_ATTRIBUTES OA;
	InitializeObjectAttributes(&OA, &dirName, OBJ_CASE_INSENSITIVE, NULL, NULL);

	// open device directory
	HANDLE dirHandle;
	status = ZwOpenDirectoryObject(&dirHandle, DIRECTORY_ALL_ACCESS, &OA);
	if (!NT_SUCCESS(status)) return status;

	//  should use ExAllocatePool2 here with tag but for some reason that doesn't work
	PVOID pBuffer = ExAllocatePool(PagedPool, POOL_SIZE);
	PVOID pContext = ExAllocatePool(PagedPool, POOL_SIZE);
	memset(pBuffer, 0, POOL_SIZE);
	memset(pContext, 0, POOL_SIZE);

	UNICODE_STRING uniMouseDeviceName, uniMouseDrvName;
	PDIRECTORY_BASIC_INFORMATION pDirBasicInfo;
	ULONG RetLen;
	int found = 0;
	// try to find a device with a pointer class
	while (1) {
		status = ZwQueryDirectoryObject(dirHandle, pBuffer, POOL_SIZE, TRUE, FALSE, pContext, &RetLen);
		if (!NT_SUCCESS(status)) break;

		pDirBasicInfo = (PDIRECTORY_BASIC_INFORMATION)pBuffer;
		pDirBasicInfo->ObjectName.Length -= 2;

		RtlInitUnicodeString(&uniMouseDrvName, L"PointerClass");

		if (RtlCompareUnicodeString(&pDirBasicInfo->ObjectName, &uniMouseDrvName, FALSE) == 0) {
			mouseId = (ULONG)(*(char*)(pDirBasicInfo->ObjectName.Buffer + pDirBasicInfo->ObjectName.Length));
			mouseId -= 0x30;

			pDirBasicInfo->ObjectName.Length += 2;
			RtlInitUnicodeString(&uniMouseDeviceName, pDirBasicInfo->ObjectName.Buffer);

			found = 1;
			break;
		}
		pDirBasicInfo->ObjectName.Length += 2;
	}
	ExFreePool(pBuffer);
	ExFreePool(pContext);
	ZwClose(dirHandle);

	if (found == 0) return status;

	WCHAR tmpNameBuffer[512];
	swprintf(tmpNameBuffer, L"\\Device\\%s", uniMouseDeviceName.Buffer);
	RtlInitUnicodeString(&uniMouseDeviceName, tmpNameBuffer);

	PFILE_OBJECT mouseFileObject;
	PDEVICE_OBJECT MouseDeviceObject;
	status = BitputIoGetDeviceObjectPointer(&uniMouseDeviceName, FILE_ALL_ACCESS, &mouseFileObject, &MouseDeviceObject);

	if (NT_SUCCESS(status)) {
		mouseDevice = MouseDeviceObject;
		ObDereferenceObject(mouseFileObject);
		oMouseReadFunc = mouseDevice->DriverObject->MajorFunction[IRP_MJ_READ];
		mouseDevice->DriverObject->MajorFunction[IRP_MJ_READ] = MouseProc;
		oInternalMouseDeviceFunc = mouseDevice->DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL];
		mouseDevice->DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = MouseInternalIoctl;

		DbgPrint("[BITPUT] Sucessfully registered mouse device");
	}
	return status;
}

NTSTATUS UnHookMouse(_In_ PDRIVER_OBJECT driverObj) {
	if (mouseRoutine) *mouseRoutine = (ULONGLONG)MouseInputRoutine;
	if (oMouseReadFunc) {
		mouseDevice->DriverObject->MajorFunction[IRP_MJ_READ] = oMouseReadFunc;
		oMouseReadFunc = NULL;
	}
	if (oInternalMouseDeviceFunc) {
		mouseDevice->DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = oInternalMouseDeviceFunc;
		oInternalMouseDeviceFunc = NULL;
	}

	return STATUS_SUCCESS;
}

NTSTATUS MouseApc(void* a1, void* a2, void* a3, void* a4, void* a5) {

	if (mouseIrp->ButtonFlags & MOUSE_LEFT_BUTTON_DOWN) {
		MouseState[0] = 1;
	} else if (mouseIrp->ButtonFlags & MOUSE_LEFT_BUTTON_UP) {
		MouseState[0] = 0;
	} else if (mouseIrp->ButtonFlags & MOUSE_RIGHT_BUTTON_DOWN) {
		MouseState[1] = 1;
	} else if (mouseIrp->ButtonFlags & MOUSE_RIGHT_BUTTON_UP) {
		MouseState[1] = 0;
	} else if (mouseIrp->ButtonFlags & MOUSE_MIDDLE_BUTTON_DOWN) {
		MouseState[2] = 1;
	} else if (mouseIrp->ButtonFlags & MOUSE_MIDDLE_BUTTON_UP) {
		MouseState[2] = 0;
	} else if (mouseIrp->ButtonFlags & MOUSE_BUTTON_4_DOWN) {
		MouseState[3] = 1;
	} else if (mouseIrp->ButtonFlags & MOUSE_BUTTON_4_UP) {
		MouseState[3] = 0;
	} else if (mouseIrp->ButtonFlags & MOUSE_BUTTON_5_DOWN) {
		MouseState[4] = 1;
	} else if (mouseIrp->ButtonFlags & MOUSE_BUTTON_5_UP) {
		MouseState[4] = 0;
	}

	return MouseInputRoutine(a1, a2, a3, a4, a5);
}

NTSTATUS MouseProc(_In_ PDEVICE_OBJECT driverObj, _In_ PIRP irp) {
	NTSTATUS status;
	ULONGLONG* routine;

	routine = (ULONGLONG*)irp;
	routine += 0xb;

	if (!MouseInputRoutine) {
		MouseInputRoutine = (MouseInput)*routine;
	}

	*routine = (ULONGLONG)MouseApc;
	mouseRoutine = routine;

	mouseIrp = (PMOUSE_INPUT_DATA)irp->UserBuffer;

	status = oMouseReadFunc(driverObj, irp);
	return status;
}

NTSTATUS MouseInternalIoctl(PDEVICE_OBJECT device, PIRP irp) {
	PIO_STACK_LOCATION ios;
	PCONNECT_DATA cd;

	ios = IoGetCurrentIrpStackLocation(irp);

	if (ios->Parameters.DeviceIoControl.IoControlCode == MOUCLASS_CONNECT_REQUEST) {
		cd = ios->Parameters.DeviceIoControl.Type3InputBuffer;

		MouseDpcRoutine = (MouseServiceDpc)cd->ClassService;
	}

	return STATUS_SUCCESS;
}

NTSTATUS CloseMouse(_In_ PDRIVER_OBJECT driverObj) {
	NTSTATUS status = STATUS_SUCCESS;
	UnHookMouse(driverObj);

	if (mouseDevice) {
		if (mouseDevice->DeviceObjectExtension) mouseDevice->DeviceObjectExtension->DeviceNode = mouseDeviceNode;
		IoDeleteDevice(mouseDevice);
		mouseDevice = NULL;
	}

	return status;

}

NTSTATUS CreateKeyboard(_In_ PDRIVER_OBJECT driverObj) {
	NTSTATUS status = STATUS_SUCCESS;
	KeyboardAddDevice KeyboardAddDevicePtr;
	ULONGLONG node = 0;
	int i;

	memset((void*)&keyboardData, 0, sizeof(keyboardData));
	for (i = 0; i < 256; i++) KeyboardState[i] = 0;

	UNICODE_STRING keyboardDeviceName;
	RtlInitUnicodeString(&keyboardDeviceName, L"\\Device\\bitputKeyboard");
	status = IoCreateDevice(driverObj, 0, &keyboardDeviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &keyboardDevice);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	keyboardDevice->Flags |= DO_BUFFERED_IO;
	keyboardDevice->Flags &= ~DO_DEVICE_INITIALIZING;


	status = HookKeyboard(driverObj);
	if (!NT_SUCCESS(status)) {
		return status;
	}

	FindDevNodeRecurse(keyboardDevice, &node);

	if (keyboardDevice->DeviceObjectExtension) {
		keyboardDeviceNode = keyboardDevice->DeviceObjectExtension->DeviceNode;
		keyboardDevice->DeviceObjectExtension->DeviceNode = (PVOID)node;
	}

	if (keyboardDevice->DriverObject) return status;
	if (keyboardDevice->DriverObject->DriverExtension) return status;

	KeyboardAddDevicePtr = (KeyboardAddDevice)keyboardDevice->DriverObject->DriverExtension->AddDevice;

	if (KeyboardAddDevicePtr) {
		KeyboardAddDevicePtr(keyboardDevice->DriverObject, keyboardDevice);
	}

	return status;

}

NTSTATUS HookKeyboard(_In_ PDRIVER_OBJECT driverObj) {
	NTSTATUS status = STATUS_SUCCESS;

	OBJECT_ATTRIBUTES objectAttributes;
	UNICODE_STRING dirName;
	RtlInitUnicodeString(&dirName, L"\\Device");
	InitializeObjectAttributes(&objectAttributes, &dirName, OBJ_CASE_INSENSITIVE, NULL, NULL);

	HANDLE dirHandle;
	status = ZwOpenDirectoryObject(&dirHandle, DIRECTORY_ALL_ACCESS, &objectAttributes);
	if (!NT_SUCCESS(status)) return status;

	PVOID pBuffer;
	PVOID pContext;
	pBuffer = ExAllocatePool(PagedPool, POOL_SIZE);
	pContext = ExAllocatePool(PagedPool, POOL_SIZE);
	memset(pBuffer, 0, POOL_SIZE);
	memset(pContext, 0, POOL_SIZE);

	int found = 0;
	UNICODE_STRING keyboardDeviceName;
	while (1) {
		ULONG RetLen;
		status = ZwQueryDirectoryObject(dirHandle, pBuffer, POOL_SIZE, TRUE, FALSE, pContext, &RetLen);
		if (!NT_SUCCESS(status))	break;

		PDIRECTORY_BASIC_INFORMATION pDirBasicInfo;
		pDirBasicInfo = (PDIRECTORY_BASIC_INFORMATION)pBuffer;

		pDirBasicInfo->ObjectName.Length -= 2;

		UNICODE_STRING uniKbdDrv;
		RtlInitUnicodeString(&uniKbdDrv, L"KeyboardClass");

		if (RtlCompareUnicodeString(&pDirBasicInfo->ObjectName, &uniKbdDrv, FALSE) == 0) {
			keyboardId = (ULONG)(*(char*)(pDirBasicInfo->ObjectName.Buffer + pDirBasicInfo->ObjectName.Length));
			keyboardId -= 0x30;
			pDirBasicInfo->ObjectName.Length += 2;
			RtlInitUnicodeString(&keyboardDeviceName, pDirBasicInfo->ObjectName.Buffer);

			found = 1;
			break;
		}
		pDirBasicInfo->ObjectName.Length += 2;
	}

	ExFreePool(pBuffer);
	ExFreePool(pContext);
	ZwClose(dirHandle);
	if (found == 0) return status;

	WCHAR tmpNameBuffer[512];
	swprintf(tmpNameBuffer, L"\\Device\\%s", keyboardDeviceName.Buffer);
	RtlInitUnicodeString(&keyboardDeviceName, tmpNameBuffer);

	PFILE_OBJECT KbdFileObject;
	PDEVICE_OBJECT KbdDeviceObject;
	status = BitputIoGetDeviceObjectPointer(&keyboardDeviceName, FILE_ALL_ACCESS, &KbdFileObject, &KbdDeviceObject);

	if (NT_SUCCESS(status))
	{
		keyboardDevice = KbdDeviceObject;
		ObDereferenceObject(KbdFileObject);
		oKeyboardReadFunc = keyboardDevice->DriverObject->MajorFunction[IRP_MJ_READ];
		keyboardDevice->DriverObject->MajorFunction[IRP_MJ_READ] = KeyboardProc;

		oInternalKeyboardDeviceFunc = keyboardDevice->DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL];
		keyboardDevice->DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = KeyboardInternalIoctl;

		DbgPrint("[BITPUT] Sucessfully registered keyboard device");
	}
	return STATUS_SUCCESS;
}

NTSTATUS UnHookKeyboard(_In_ PDRIVER_OBJECT driverObj) {
	if (keyboardRoutine) *keyboardRoutine = (ULONGLONG)KeyboardInputRoutine;

	if (oKeyboardReadFunc) {
		keyboardDevice->DriverObject->MajorFunction[IRP_MJ_READ] = oKeyboardReadFunc;
		oKeyboardReadFunc = NULL;
	}
	if (oInternalKeyboardDeviceFunc) {
		keyboardDevice->DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = oInternalKeyboardDeviceFunc;
		oInternalKeyboardDeviceFunc = NULL;
	}

	return STATUS_SUCCESS;
}


NTSTATUS KeyboardApc(void* a1, void* a2, void* a3, void* a4, void* a5) {
	unsigned char max = (unsigned char)keyboardIrp->MakeCode;

	if (!keyboardIrp->Flags) {
		KeyboardState[(max)-1] = 1;
	} else if (keyboardIrp->Flags & KEY_BREAK) {
		KeyboardState[(max)-1] = 0;
	}

	return KeyboardInputRoutine(a1, a2, a3, a4, a5);
}

NTSTATUS KeyboardProc(_In_ PDEVICE_OBJECT deviceObj, _In_ PIRP irp) {
	PCONNECT_DATA cd;
	NTSTATUS status;

	ULONGLONG* routine;

	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
	if (stack->Parameters.DeviceIoControl.IoControlCode == KBDCLASS_CONNECT_REQUEST && KeyboardDpcRoutine == NULL) {
		cd = stack->Parameters.DeviceIoControl.Type3InputBuffer;

		KeyboardDpcRoutine = (KeyboardServiceDpc)cd->ClassService;
	}

	routine = (ULONGLONG*)irp;
	routine += 0xb;

	if (!KeyboardInputRoutine) {
		KeyboardInputRoutine = (KeyboardInput)*routine;
	}

	*routine = (ULONGLONG)KeyboardApc;
	keyboardRoutine = routine;

	keyboardIrp = (PKEYBOARD_INPUT_DATA)irp->UserBuffer;

	status = oKeyboardReadFunc(deviceObj, irp);

	return status;
}

NTSTATUS KeyboardInternalIoctl(PDEVICE_OBJECT deviceObj, PIRP irp) {
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
	if (stack->Parameters.DeviceIoControl.IoControlCode == KBDCLASS_CONNECT_REQUEST)
	{
		PCONNECT_DATA cd = stack->Parameters.DeviceIoControl.Type3InputBuffer;

		KeyboardDpcRoutine = (KeyboardServiceDpc)cd->ClassService;
	}

	return STATUS_SUCCESS;
}

NTSTATUS CloseKeyboard(_In_ PDRIVER_OBJECT driverObj) {
	NTSTATUS status = STATUS_SUCCESS;

	UnHookKeyboard(driverObj);
	if (keyboardDevice) {
		if (keyboardDevice->DeviceObjectExtension) keyboardDevice->DeviceObjectExtension->DeviceNode = keyboardDeviceNode;
		IoDeleteDevice(keyboardDevice);
		keyboardDevice = NULL;
	}

	return status;

}