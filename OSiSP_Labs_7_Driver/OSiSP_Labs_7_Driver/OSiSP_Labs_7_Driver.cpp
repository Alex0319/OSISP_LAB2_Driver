#include "stdafx.h"

#ifdef __cplusplus
    extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath);
#endif
#define IOCTL_CREATE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_CLOSE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define BUFFER_SIZE 1024 

extern "C" char* PsGetProcessImageFileName(IN PEPROCESS Process);
extern "C" NTSTATUS PsLookupProcessByProcessId(HANDLE ProcessId,PEPROCESS *Process);

void DriverUnload(IN PDRIVER_OBJECT DriverObject);
NTSTATUS DriverDispatchIoctl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

PDEVICE_OBJECT DeviceObject = NULL;
PROCESS_PAIRS_INFO procPairsInfo;
PROCESS_INFO procInfo;

char* GetProcessNameFromPid(HANDLE pid)
{
    PEPROCESS Process;
    if(PsLookupProcessByProcessId(pid,&Process)==STATUS_INVALID_PARAMETER)
        return NULL;
    char* processName=PsGetProcessImageFileName(Process);
    ObDereferenceObject(Process);
    return processName;
}

bool CheckProcessInfo(char* currentProcessName,int currentPid,char* processName,int pid)
{
    if(currentProcessName!=NULL && processName!=NULL && (strcmp(currentProcessName,processName)==0 && currentPid==pid))
    {
        DbgPrint("Process name:%s pid:%d terminated\n",processName,pid);
        return true;
    }
    return false;
}

void SendSignalToCreateProcessY()
{
    PLOCAL_DEVICE_INFO DeviceExtension=(PLOCAL_DEVICE_INFO)DeviceObject->DeviceExtension;
    KeSetEvent(DeviceExtension->pCreateEventObject,0,FALSE);
    KeClearEvent(DeviceExtension->pCreateEventObject);
}

void SendSignalToTerminateProcess()
{
    PLOCAL_DEVICE_INFO DeviceExtension=(PLOCAL_DEVICE_INFO)DeviceObject->DeviceExtension;
    KeSetEvent(DeviceExtension->pCloseEventObject,0,FALSE);
    KeClearEvent(DeviceExtension->pCloseEventObject);
}

void SetTerminateProcessInfo(char* processName,int pid)
{
    procInfo.pid=pid;
    strcpy(procInfo.processName,processName);
    SendSignalToTerminateProcess();    
}

VOID ProcessNotifyRoutine(IN HANDLE ParentId, IN HANDLE ProcessId, IN BOOLEAN Create) {
    PAGED_CODE();
    char* processName=GetProcessNameFromPid(ProcessId);
    if(Create)
    {
        if(processName!=NULL && strcmp("ProcessManagerA",processName)!=0 && procPairsInfo.pidX==0)
        {
            SendSignalToCreateProcessY();
            procPairsInfo.pidX=(int)ProcessId;
            strcpy(procPairsInfo.processXName,processName);
            DbgPrint("Process name: %s pid: %d created\n",processName,(int)ProcessId);
        }
    }
    else
    {
        if(CheckProcessInfo(processName,(int)ProcessId,procPairsInfo.processXName,procPairsInfo.pidX))
            SetTerminateProcessInfo(procPairsInfo.processYName,procPairsInfo.pidY);
        if(CheckProcessInfo(processName,(int)ProcessId,procPairsInfo.processYName,procPairsInfo.pidY))
            SetTerminateProcessInfo(procPairsInfo.processXName,procPairsInfo.pidX);
    }
}

NTSTATUS Driver(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    Irp->IoStatus.Status = STATUS_SUCCESS;

    Irp->IoStatus.Information=0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath)
{
	  UNICODE_STRING DeviceName,Win32Device,CreateEventName,CloseEventName;
    PLOCAL_DEVICE_INFO DeviceExtension;
	  NTSTATUS status;
    HANDLE CreateEventHandle,CloseEventHandle;

	  RtlInitUnicodeString(&DeviceName,L"\\Device\\OSiSP_Labs_7_Driver");
	  RtlInitUnicodeString(&Win32Device,L"\\DosDevices\\OSiSP_Labs_7_Driver");	

    for (unsigned int i=0;i<=IRP_MJ_MAXIMUM_FUNCTION;++i)
        DriverObject->MajorFunction[i]=Driver;
	  DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDispatchIoctl;
    DriverObject->DriverUnload = DriverUnload;
	  status = IoCreateDevice(DriverObject, sizeof(LOCAL_DEVICE_INFO), &DeviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
	  if (!NT_SUCCESS(status))
		    return status;
	  if (!DeviceObject)
		    return STATUS_UNEXPECTED_IO_ERROR;

    DeviceExtension=(PLOCAL_DEVICE_INFO)DeviceObject->DeviceExtension;

	  DeviceObject->Flags |= DO_DIRECT_IO;
	  DeviceObject->AlignmentRequirement = FILE_WORD_ALIGNMENT;
	  status = IoCreateSymbolicLink(&Win32Device, &DeviceName);

	  DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    status = PsSetCreateProcessNotifyRoutine(ProcessNotifyRoutine, FALSE);
	  if (!NT_SUCCESS(status))
		    return status;
	  RtlInitUnicodeString(&CreateEventName,L"\\BaseNamedObjects\\CreateEvent");
	  RtlInitUnicodeString(&CloseEventName,L"\\BaseNamedObjects\\CloseEvent");
    DeviceExtension->pCreateEventObject=IoCreateNotificationEvent(&CreateEventName,&CreateEventHandle);
    DeviceExtension->pCloseEventObject=IoCreateNotificationEvent(&CloseEventName,&CloseEventHandle);
    KeClearEvent(DeviceExtension->pCreateEventObject);
    KeClearEvent(DeviceExtension->pCloseEventObject);
    return STATUS_SUCCESS;
}

void DriverUnload(IN PDRIVER_OBJECT DriverObject)
{
    PsSetCreateProcessNotifyRoutine(&ProcessNotifyRoutine, TRUE);

	  UNICODE_STRING Win32Device;
	  RtlInitUnicodeString(&Win32Device,L"\\DosDevices\\OSiSP_Labs_7_Driver");
	  IoDeleteSymbolicLink(&Win32Device);
	  IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS DriverDispatchIoctl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
    ULONG inSize,outSize;

    DbgPrint("Dispatch IOctl\n");
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PIO_STACK_LOCATION irpStack  = IoGetCurrentIrpStackLocation(Irp);
    inSize = irpStack->Parameters.DeviceIoControl.InputBufferLength;
	  outSize = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    switch(irpStack->Parameters.DeviceIoControl.IoControlCode)
    {
        case IOCTL_CREATE:
        {
            RtlCopyMemory(&procInfo,(PPROCESS_INFO)Irp->AssociatedIrp.SystemBuffer,sizeof(PROCESS_INFO));
            procPairsInfo.pidY=procInfo.pid;
            strcpy(procPairsInfo.processYName,procInfo.processName);
            Irp->IoStatus.Information= 0;
            break;
        }
        case IOCTL_CLOSE:
        {
            RtlCopyMemory(Irp->AssociatedIrp.SystemBuffer,&procInfo,sizeof(PROCESS_INFO));            
            Irp->IoStatus.Information= outSize;
            RtlZeroMemory(&procInfo,sizeof(PROCESS_INFO));
            RtlZeroMemory(&procPairsInfo,sizeof(PROCESS_PAIRS_INFO));
            break;
        }
    }

    Irp->IoStatus.Status = ntStatus;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return ntStatus;
}