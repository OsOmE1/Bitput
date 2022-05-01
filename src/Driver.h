#pragma once
#include "callbacks.h"
#include "structs.h"
#include "relay.h"

PVOID OBRegistrationHandle;

NTSTATUS UnloadDriver(PDRIVER_OBJECT pDriverObject);
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath);
NTSTATUS RegisterOBCallback();
