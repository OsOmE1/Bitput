#pragma once
#include <ntifs.h>
#include "vector.h"
#include "state.h"

PLOAD_IMAGE_NOTIFY_ROUTINE LoadImageCallback(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo);
PCREATE_PROCESS_NOTIFY_ROUTINE_EX ProcessNotifyCallback(HANDLE parentId, HANDLE processId, PPS_CREATE_NOTIFY_INFO notifyInfo);
OB_PREOP_CALLBACK_STATUS PreOBCallback(PVOID RegistrationContext, POB_PRE_OPERATION_INFORMATION OperationInformation);