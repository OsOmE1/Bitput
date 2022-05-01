#pragma warning( disable : 4100 4047 4024 4022 4201 4311 4057 4213 4189 4081 4189 4706 4214 4459 4273 4127 26451 4133 4244)

#include "driver.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, RegisterOBCallback)
#endif

NTSTATUS UnloadDriver(PDRIVER_OBJECT pDriverObject) {
	IoDeleteSymbolicLink(&dosDeviceName);

	if (OBRegistrationHandle) {
		ObUnRegisterCallbacks(OBRegistrationHandle);
		OBRegistrationHandle = NULL;
	}
	CloseMouse(pDriverObject);

	IoDeleteDevice(pDriverObject->DeviceObject);
	return STATUS_SUCCESS;
}

#ifndef MANUALMAPPED
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
#else
NTSTATUS DriverInitialize(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
#endif // !MANUALMAPPED

	DbgPrint("[BITPUT] Loaded Bitput driver!");
	PKLDR_DATA_TABLE_ENTRY DriverSection = (PKLDR_DATA_TABLE_ENTRY)DriverObject->DriverSection;
	DriverSection->Flags |= 0x20; // LDRP_VALID_SECTION 

    RtlInitUnicodeString(&deviceName, L"\\Device\\Bitput");
    RtlInitUnicodeString(&dosDeviceName, L"\\DosDevices\\Bitput");

    IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &device);
    IoCreateSymbolicLink(&dosDeviceName, &deviceName);

	DriverObject->MajorFunction[IRP_MJ_CREATE] = CreateCall;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = CloseCall;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IoControl;
	DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = InternalIoControl;
	DriverObject->DriverUnload = UnloadDriver;

	DriverObject->Flags |= DO_BUFFERED_IO;
	DriverObject->Flags &= ~DO_DEVICE_INITIALIZING;

	NTSTATUS reg = RegisterOBCallback();
	if (reg == STATUS_SUCCESS) {
		DbgPrint("[BITPUT] ObRegisterCallbacks Succeeded.\n");
	} else {
		DbgPrint("[BITPUT] ObRegisterCallbacks Failed.\n");
	}

	CreateMouse(DriverObject);
	CreateKeyboard(DriverObject);

    return reg;
}

#ifdef MANUALMAPPED
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(RegistryPath);

	UNICODE_STRING  drvName;
	NTSTATUS status;
	RtlInitUnicodeString(&drvName, L"\\Driver\\Bitput");
	status = IoCreateDriver(&drvName, &DriverInitialize);
	if (NT_SUCCESS(status))
	{
        DbgPrintEx(0, 0, "Created driver.\n");
	}
	return status;
}
#endif

// https://revers.engineering/superseding-driver-altitude-checks-on-windows/
NTSTATUS RegisterOBCallback() {
	OB_OPERATION_REGISTRATION OBOperationRegistration;
	OB_CALLBACK_REGISTRATION OBOCallbackRegistration;
	UNICODE_STRING lowestAltitude;
	memset(&OBOperationRegistration, 0, sizeof(OB_OPERATION_REGISTRATION));
	memset(&OBOCallbackRegistration, 0, sizeof(OB_CALLBACK_REGISTRATION));
	RtlInt64ToUnicodeString(20000, 10, &lowestAltitude);

	NTSTATUS Status = STATUS_UNSUCCESSFUL;

	if ((USHORT)ObGetFilterVersion() == OB_FLT_REGISTRATION_VERSION)
	{
		OBOperationRegistration.ObjectType = PsProcessType;
		OBOperationRegistration.Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
		OBOperationRegistration.PreOperation = PreOBCallback;
		OBOperationRegistration.PostOperation = NULL;

		OBOCallbackRegistration.Version = OB_FLT_REGISTRATION_VERSION;
		OBOCallbackRegistration.Altitude = lowestAltitude;
		OBOCallbackRegistration.OperationRegistration = &OBOperationRegistration;
		OBOCallbackRegistration.RegistrationContext = NULL;
		OBOCallbackRegistration.OperationRegistrationCount = 1;

		Status = ObRegisterCallbacks(&OBOCallbackRegistration, &OBRegistrationHandle);
	}

	return Status;
}