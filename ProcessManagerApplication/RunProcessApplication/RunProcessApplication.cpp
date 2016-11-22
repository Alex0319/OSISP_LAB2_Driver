#include "stdafx.h"

SC_HANDLE scServiceManagerHandle,scServiceHandle;
HANDLE deviceHandle;
bool isSystemStartService=true;
bool isAlive=true;
PROCESS_INFO procInfo;
OPENFILENAME ofn ;
TCHAR filename[_MAX_FNAME]=_T("");

std::string getProcessNameByHandle(HANDLE hProcess)
{
  if (NULL == hProcess)
    return "<unknown>";

  CHAR szProcessName[MAX_PATH] = "<unknown>";

  if(!GetProcessImageFileNameA(hProcess,szProcessName,sizeof(szProcessName)))
      return "<unknown>";
  strcpy(szProcessName,&((strrchr(szProcessName,'\\'))[1]));
  return std::string(szProcessName);
}

std::string getProcessNameByID(DWORD processID)
{
  HANDLE hProcess =OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,false,processID);
  std::string result = getProcessNameByHandle(hProcess);
  CloseHandle(hProcess);
  return result;
}

PPROCESS_INFO SendInfoToDriver(PPROCESS_INFO procInfo,bool isSendYProcessInfo)
{
    DWORD dwRet;
    if(isSendYProcessInfo)
    {
        if (!DeviceIoControl(deviceHandle, IOCTL_CREATE, procInfo, sizeof(PROCESS_INFO), NULL, 0, &dwRet, 0))
          printf("DeviceIOControl Fail %d\n",GetLastError());
    }
    else
        if (!DeviceIoControl(deviceHandle, IOCTL_CLOSE, NULL, 0,procInfo, sizeof(PROCESS_INFO), &dwRet, 0))
          printf("DeviceIOControl Fail %d\n",GetLastError());
    return procInfo;
}

DWORD WINAPI WaitingProcComplete (PVOID Param)
{
    HANDLE hEvent=OpenEvent(SYNCHRONIZE, FALSE,CLOSE_EVENT);
    while(isAlive)
    {
        DWORD status = WaitForSingleObject(hEvent,INFINITE);
        if(status==WAIT_OBJECT_0)
        {
            PPROCESS_INFO terminateProcessInfo=SendInfoToDriver(&procInfo,false);
            HANDLE hProcess=OpenProcess(PROCESS_TERMINATE,true,terminateProcessInfo->pid);
            if(!TerminateProcess(hProcess,EXIT_SUCCESS))
                printf("Process name: %s pid: %d can't be terminated.Error %d\n",terminateProcessInfo->processName,terminateProcessInfo->pid,GetLastError());
            else
                printf("Process name: %s pid: %d terminated\n",terminateProcessInfo->processName,terminateProcessInfo->pid);                
            CloseHandle(hProcess);

        }
    }
	  return 0;
}

DWORD WINAPI CreateProc (PVOID Param)
{
    PROCESS_INFORMATION processInformation;
    HANDLE hEvent=OpenEvent(SYNCHRONIZE, FALSE,CREATE_EVENT);
    while(isAlive)
    {
        DWORD status = WaitForSingleObject(hEvent,2000);
        if(status==WAIT_OBJECT_0)
        {
            if(GetOpenFileName( &ofn ))
            {
                STARTUPINFO startupInfo;
                ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
                startupInfo.cb = sizeof(STARTUPINFO);
                if (!CreateProcess(filename, NULL, NULL, NULL, FALSE, CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &startupInfo, &processInformation))
                    printf("Error create process %s. Error: %d\n",filename,GetLastError());
                else
                {
                    procInfo.pid=processInformation.dwProcessId;
                    strcpy(procInfo.processName,getProcessNameByID(procInfo.pid).c_str());
                    SendInfoToDriver(&procInfo,true);
                    printf("Process name: %s pid: %d created\n",procInfo.processName,procInfo.pid);                    
                }
            }
        }
    }
	  return 0;
}

void WaitExit()
{
    HANDLE createThread = CreateThread(0,0,CreateProc,NULL,0,NULL);
    HANDLE closeThread = CreateThread(0,0,WaitingProcComplete,NULL,0,NULL);
    do
    {
        TCHAR buf[1024];
        printf("Write exit to close application\n");
        scanf_s("%s",buf);
        if (!_tcscmp(buf,L"exit"))
            break;
    }while (true);
    isAlive = false;
    if (WaitForSingleObject(createThread,2000)!=WAIT_OBJECT_0)
    {
        printf("Timeout close createThread, terminated\n");
        TerminateThread(createThread,EXIT_FAILURE);
    }
    if (WaitForSingleObject(closeThread,2000)!=WAIT_OBJECT_0)
    {
        printf("Timeout close closeThread, terminated\n");
        TerminateThread(closeThread,EXIT_FAILURE);
    }
}

void SynchronizeWithDriver()
{
    deviceHandle=CreateFile(L"\\\\.\\"DRIVERNAME,GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
    if(deviceHandle != INVALID_HANDLE_VALUE)
    {
        WaitExit();
        CloseHandle(deviceHandle);
    }
    else
        printf("Get Device handle Fail : 0x%X\n", GetLastError());
}

bool StartService()
{
    SERVICE_STATUS serviceStatus;
    QueryServiceStatus(scServiceHandle,&serviceStatus);
    if(serviceStatus.dwCurrentState!=SERVICE_RUNNING)
        if(!StartService(scServiceHandle,NULL,NULL))
        {
            printf("Driver service start error\n");
            return false;
        }
        else
            isSystemStartService=false;
    printf("Driver service running\n");
    return true;
}

TCHAR* GetServiceName(TCHAR* lastSlach,TCHAR* filename)
{
    if(lastSlach)
        return &lastSlach[1];
    return filename;
}

SC_HANDLE CreateDriverService()
{
    TCHAR filename[_MAX_FNAME];
    GetModuleFileName(NULL,filename,_MAX_FNAME);
    TCHAR* serviceName=GetServiceName(wcsrchr(filename,'\\'),filename);
    ZeroMemory(serviceName,wcslen(serviceName));
    _tcscpy_s(serviceName,_tcslen(serviceName),DRIVERNAME L".sys");
    return CreateService(scServiceManagerHandle,DRIVERNAME,DRIVERNAME,SERVICE_ALL_ACCESS,SERVICE_KERNEL_DRIVER,SERVICE_DEMAND_START,
    SERVICE_ERROR_NORMAL,	filename, NULL, NULL,	NULL, NULL, NULL);
}

void CloseService()
{
    if(!isSystemStartService)
    {
        printf("Stop service");
        SERVICE_STATUS serviceStatus;
        if(!ControlService(scServiceHandle,SERVICE_CONTROL_STOP,&serviceStatus))
            printf("Don't stop service");
    }
    if (scServiceHandle != NULL)
        DeleteService(scServiceHandle);
    CloseServiceHandle(scServiceHandle);
}

void InitializeService()
{
    if((scServiceHandle=OpenService(scServiceManagerHandle,DRIVERNAME,SERVICE_ALL_ACCESS)) || 
      (scServiceHandle = CreateDriverService()))
    {
        if(StartService())
            SynchronizeWithDriver();
        CloseService();
    }
}

void InitializeServiceManager()
{
    if(scServiceManagerHandle=OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS))
    {
        InitializeService();
        CloseServiceHandle(scServiceManagerHandle);
    }
    else
        printf("OpenScManager error %X",GetLastError());
}

void InitializeFileOpenStruct()
{
    ZeroMemory( &ofn , sizeof( ofn));
    ofn.lStructSize = sizeof ( OPENFILENAME );
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = filename ;
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrFilter = NULL;
    ofn.nFilterIndex =1;
  	ofn.lpstrDefExt = _T("exe");
    ofn.lpstrFileTitle = _T("Select process") ;
    ofn.nMaxFileTitle = 0 ;
    ofn.lpstrInitialDir=NULL ;
    ofn.Flags = OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST ;
}

int _tmain(int argc, _TCHAR* argv[])
{
    InitializeFileOpenStruct();
    InitializeServiceManager();
    system("PAUSE");
    return 0;
}
