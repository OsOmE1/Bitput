#pragma warning( disable : 4100 4047 4024 4022 4201 4311 4057 4213 4189 4081 4189 4706 4214 4459 4273 4242 4244 4127)

#include "callbacks.h"

PLOAD_IMAGE_NOTIFY_ROUTINE LoadImageCallback(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo) {
    // Process load image event
	return STATUS_SUCCESS;
}

PCREATE_PROCESS_NOTIFY_ROUTINE_EX ProcessNotifyCallback(HANDLE parentId, HANDLE processId, PPS_CREATE_NOTIFY_INFO notifyInfo) {
	ULONG PID = (ULONG)processId;
	if (notifyInfo) {
        // Process create event
        return STATUS_SUCCESS;
	}
    // Process terminate event
	return STATUS_SUCCESS;
}

OB_PREOP_CALLBACK_STATUS PreOBCallback(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION OperationInformation) {
    size_t ppCount = vector_size(protectedProcessList);
    if (ppCount == 0) {
        return OB_PREOP_SUCCESS;
    }
    PEPROCESS OpenedProcess = (PEPROCESS)OperationInformation->Object;
    ULONG OpenedProcessID = (ULONG)PsGetProcessId(OpenedProcess);
    PEPROCESS CurrentProcess = PsGetCurrentProcess();

    if (OperationInformation->KernelHandle == 1 || OpenedProcess == CurrentProcess)
    {
        return OB_PREOP_SUCCESS;
    }

    for (int i = 0; i < ppCount; i++)
    {
        ULONG pid = protectedProcessList[i];
        if (pid == OpenedProcessID)
        {
            OperationInformation->Parameters->CreateHandleInformation.DesiredAccess = (SYNCHRONIZE | 0x1000 /*PROCESS_QUERY_LIMITED_INFORMATION*/);
        }
    }
    return OB_PREOP_SUCCESS;
}