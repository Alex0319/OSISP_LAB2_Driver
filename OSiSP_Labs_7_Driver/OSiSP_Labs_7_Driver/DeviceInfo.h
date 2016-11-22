#ifndef DEVICE_INFO
    #define DEVICE_INFO
#endif

#define MAX_PROCESS_NAME 256

typedef struct _LOCAL_DEVICE_INFO {
    PKEVENT pCreateEventObject;
    PKEVENT pCloseEventObject;
} LOCAL_DEVICE_INFO, *PLOCAL_DEVICE_INFO;

typedef struct _PROCESS_PAIRS_INFO {
    int pidX,pidY;
    char processXName[MAX_PROCESS_NAME];
    char processYName[MAX_PROCESS_NAME];
} PROCESS_PAIRS_INFO, *PPROCESS_PAIRS_INFO;

typedef struct _PROCESS_INFO{
    int pid;
    char processName[MAX_PROCESS_NAME];
} PROCESS_INFO,*PPROCESS_INFO;
