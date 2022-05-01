#pragma warning( disable : 4701 4100 4047 4024 4022 4201 4311 4057 4213 4189 4081 4189 4706 4214 4459 4273)
#include "relay.h"

NTSTATUS CreateCall(PDEVICE_OBJECT DeviceObject, PIRP irp) {
	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;

	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS CloseCall(PDEVICE_OBJECT DeviceObject, PIRP irp) {
	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;

	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS IoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	NTSTATUS Status = STATUS_SUCCESS;
	ULONG SizeInfo = 0;

	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;

	switch (code) {
	case (IO_TEST): {
		DbgPrint("[BITPUT] Pressing windows icon!");

		mouseData.ButtonFlags |= MOUSE_RIGHT_BUTTON_DOWN;
		mouseData.LastX = 1000;
		mouseData.LastY = 500;
		mouseData.Flags |= MOUSE_MOVE_RELATIVE;
		SynthesizeMouse(&mouseData);
		Sleep(50);

		mouseData.ButtonFlags &= ~MOUSE_RIGHT_BUTTON_DOWN;
		mouseData.ButtonFlags |= MOUSE_RIGHT_BUTTON_UP;
		SynthesizeMouse(&mouseData);
		break;
	}
	default: {
		Status = STATUS_INVALID_PARAMETER;
		break;
	}
	}
	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = SizeInfo;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return Status;
}