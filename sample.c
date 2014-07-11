#include <wdm.h>
#include <Ntstrsafe.h>
#define STDCALL __stdcall

NTSTATUS
	DriverEntry(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath
	);

KSTART_ROUTINE FlushRingBuffer;
KDEFERRED_ROUTINE FlusherDpcRoutine;

typedef struct Buffer{
	SIZE_T FreeSpace;
	SIZE_T Size;
	char* head;
	char* tail;
	char* start;
}  BUFFER;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

//Global variables
SIZE_T  NumberOfBytes=512;
KEVENT ThreadStartedEvent;
KEVENT ThreadStopEvent;
KEVENT ActivateFlusherEvent;
BOOLEAN TimeToExit = FALSE;
BUFFER RingBuffer;
HANDLE FileHandle;//
KSPIN_LOCK BufferLock;
KLOCK_QUEUE_HANDLE LockQueue;
KDPC FlushDpc;
 
VOID CreateRingBuffer(){
	RingBuffer.head=NULL;
	RingBuffer.start=ExAllocatePoolWithTag(
		NonPagedPool,
		NumberOfBytes,
		'plBR');
	if(RingBuffer.start==NULL){
		DbgPrint("Couldn't allocate memory\n");
	}else{
		DbgPrint("Allocated memory succesfully\n");
	}
	RingBuffer.head=RingBuffer.start;
	RingBuffer.tail=RingBuffer.start;
	RingBuffer.Size=NumberOfBytes;
	RingBuffer.FreeSpace=NumberOfBytes;
}

VOID WriteToRingBuffer(char* dataPtr, SIZE_T BytesNum){
	KIRQL PrevIRQL;
	SIZE_T lenght;
	
//	__debugbreak();
	if(dataPtr==NULL || BytesNum==0){
		DbgPrint("Data is not correct : size=0 or wrong pointer");
	}

	//PrevIRQL=KeGetCurrentIrql();
	 KeRaiseIrql(
    		HIGH_LEVEL,
   	        &PrevIRQL);
	KeAcquireInStackQueuedSpinLockAtDpcLevel(&BufferLock, &LockQueue);
	if(RingBuffer.FreeSpace <= BytesNum){
		BytesNum=RingBuffer.FreeSpace - 1;
	}
	
	if(BytesNum > RingBuffer.start  - RingBuffer.head + RingBuffer.Size){
		lenght=	RingBuffer.start - RingBuffer.head + RingBuffer.Size;
		RtlCopyMemory(
			RingBuffer.head,
			dataPtr,
			lenght
		);
		RtlCopyMemory(RingBuffer.start,
			dataPtr+lenght,
			BytesNum-lenght);
	}else{
		RtlCopyMemory(
			RingBuffer.head,
			dataPtr,
			BytesNum);
	}
	RingBuffer.head=(RingBuffer.head - RingBuffer.start + BytesNum) % RingBuffer.Size + RingBuffer.start;
	RingBuffer.FreeSpace-=BytesNum;
    //check if(RingBuffer.FreeSpace < RingBuffer.Size/3) and set flag
	KeReleaseInStackQueuedSpinLockFromDpcLevel(&LockQueue);
	KeLowerIrql(PrevIRQL);

	//if flag
	//it is safe to check without spinlock cause sideeffect is extra flush
	if(RingBuffer.FreeSpace < RingBuffer.Size/3){
		//if (KeGetIrql<2)
		KeSetEvent(&ActivateFlusherEvent,
			0,
			FALSE);
		//else dpc
	}
	//else{
	//	if(KeInsertQueueDpc(&FlushDpc, NULL, NULL) == TRUE){
	//		DbgPrint("We ordered DPC");
	//	}else{
	//		DbgPrint("We DO NOT ordered DPC");
	//	}	
	//}
}
VOID FlushDpcRoutine(IN PKDPC Dpc,
                     IN PVOID DeferredContext,
                     IN PVOID SystemArgument1,
                     IN PVOID SystemArgument2){
	KeSetEvent(&ActivateFlusherEvent,
	0,
	FALSE);
	DbgPrint("DPC executed, Event notified\n");
}

VOID FlushRingBuffer(__in PVOID  StartContext){
	IO_STATUS_BLOCK    ioStatusBlock;
	NTSTATUS Status;
	OBJECT_ATTRIBUTES ObjAttr;
	UNICODE_STRING pathBuffer;
	size_t  cb;
	PVOID pEvent;
	HANDLE hEvent;
	OBJECT_ATTRIBUTES oa;
	UNICODE_STRING us;
	LARGE_INTEGER Timeout;
	int length;

//	__debugbreak();
	KeSetEvent(
    		&ThreadStartedEvent,
    		0,
    		FALSE);
	//InitializeObjectAttributes(
	//	&ObjAttr,
	//	&pathBuffer,  			//ObjectName
	//	OBJ_CASE_INSENSITIVE, 	//Attributes
	//	NULL,			//RootDirectory
	//	NULL);			//SecurityDescriptor
		
	Timeout.QuadPart = -10000000 ;
	
	while((!TimeToExit) || (RingBuffer.head != RingBuffer.tail)){
		Status = KeWaitForSingleObject(
				&ActivateFlusherEvent,
				Executive,
				KernelMode ,
				FALSE,
				&Timeout);
		if(Status == STATUS_TIMEOUT){
			DbgPrint("Flusher waked by timeout");
		} else {
			DbgPrint("Flusher waked by notification");
		}
		/*
		// Do not try to perform any file operations at higher IRQL levels.
		// Instead, you may use a work item or a system worker thread to perform file operations.
		if(KeGetCurrentIrql() != PASSIVE_LEVEL){
			DbgPrint("STATUS_INVALID_DEVICE_STATE\n");
			//TODO: DPC

		}*/
		
		if(RingBuffer.head != RingBuffer.tail){
			head = RingBuffer.head;
			if(head < RingBuffer.tail){
				length = RingBuffer.start + RingBuffer.Size - RingBuffer.tail;
				Status=ZwWriteFile(
					FileHandle,
					NULL,
					NULL,
					NULL,
					&ioStatusBlock,
					RingBuffer.tail,
					length,
					NULL,
					NULL);
				if(Status == STATUS_SUCCESS){
					DbgPrint("1 Write to file succes\n");
				}else{
					DbgPrint("1 Write to file fail\n");
				}

				Status=ZwWriteFile(
					FileHandle,
					NULL,
					NULL,
					NULL,
					&ioStatusBlock,
					RingBuffer.start,
					head-RingBuffer.start,
					NULL,
					NULL);
				if(Status == STATUS_SUCCESS){
					DbgPrint("2 Write to file succes\n");
				}else{
					DbgPrint("2 Write to file fail\n");
				}	
			}else{
				Status=ZwWriteFile(
					FileHandle,
					NULL,
					NULL,
					NULL,
					&ioStatusBlock,
					RingBuffer.tail,
					head-RingBuffer.tail,
					NULL,
					NULL);
				if(Status == STATUS_SUCCESS){
					DbgPrint("Write to file succes\n");
				}else{
					DbgPrint("Write to file fail\n");
				}
			}
			RingBuffer.tail = head;
			RingBuffer.FreeSpace=RingBuffer.Size;	
		} else {
			DbgPrint("Nothing to write\n");
		}
	}

	//KeSetEvent(
    //		&ThreadStopEvent,
    //		0,
    //		FALSE);

	//DbgPrint("Flusher set event and exited: TimeToExit is %d, and (Tail == Head) is %d\n", TimeToExit,(RingBuffer.head == RingBuffer.tail));
	PsTerminateThread(...);
}

NTSTATUS
DllInitialize( IN PUNICODE_STRING pus )
{
//	__debugbreak();
	DbgPrint("SAMPLE: DllInitialize(%S)\n", pus->Buffer );
	return STATUS_SUCCESS;
}

NTSTATUS
DllUnload( )
{
//	__debugbreak();
	DbgPrint("SAMPLE: DllUnload\n");
	return STATUS_SUCCESS;
}

__declspec(dllexport) NTSTATUS
Klog(char* message)
{	
	SIZE_T BytesNum = strlen(message)*sizeof(char);

//	__debugbreak();
	DbgPrint("Call Klog\n");
	
	WriteToRingBuffer(message,BytesNum);
	return STATUS_SUCCESS;
}

//should be called at PASSIVE_LEVEL
NTSTATUS
KlogInit()
{
	NTSTATUS Status;// Result of the last operation
	HANDLE FlusherThreadHandle;//Handle of flusher thread
	IO_STATUS_BLOCK    ioStatusBlock;//attr for creating file
	OBJECT_ATTRIBUTES ObjAttr;
	UNICODE_STRING pathToLogFile;
	PKTHREAD pFlusherThread;
	
//	__debugbreak();
	DbgPrint("Call KlogInit\n");

	TimeToExit = FALSE;

	KeInitializeDpc(
    		&FlushDpc,
		FlushDpcRoutine,
    		NULL);

	RtlInitUnicodeString(
		&pathToLogFile,
		L"\\DosDevices\\C:\\MyBufferLogger.txt");
	InitializeObjectAttributes(
		&ObjAttr,
		&pathToLogFile,  			//ObjectName
		OBJ_CASE_INSENSITIVE, 	//Attributes
		NULL,			//RootDirectory
		NULL);			//SecurityDescriptor

	KeInitializeSpinLock(&BufferLock);
		
	// Do not try to perform any file operations at higher IRQL levels.
	// Instead, you may use a work item or a system worker thread to perform file operations.
	if(KeGetCurrentIrql() != PASSIVE_LEVEL){
		//  return STATUS_INVALID_DEVICE_STATE; 
		DbgPrint("STATUS_INVALID_DEVICE_STATE\n");
		return -1;
	}

	Status = ZwCreateFile(&FileHandle,
		GENERIC_WRITE,
		&ObjAttr,
		&ioStatusBlock,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		0,
		FILE_OVERWRITE_IF, 
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL, 0);

	CreateRingBuffer();

	//while(KeGetCurrentIrql() != PASSIVE_LEVEL){
			//  return STATUS_INVALID_DEVICE_STATE; 
	//		DbgPrint("Waiting for my IRQL\n");
//			return(-1);
//		}
	KeInitializeEvent(
		&ThreadStartedEvent,
		SynchronizationEvent,
		FALSE);

	//KeInitializeEvent(
	//	&ThreadStopEvent,
	//	SynchronizationEvent,
	//	FALSE);
		
	KeInitializeEvent(&ActivateFlusherEvent,
		SynchronizationEvent,
		FALSE);

	DbgPrint("KeInitializeEvent was completed\n");
		
	Status=PsCreateSystemThread(&FlusherThreadHandle, 	// ThreadHandle
		GENERIC_ALL,								// DesiredAccess
		NULL,									// ObjectAttributes
		NULL,									//
		NULL,
		FlushRingBuffer,
		NULL);
		
	if(NT_SUCCESS(Status)){
		DbgPrint("Thread created succesfully\n");
	}else{
		DbgPrint("Failed to create thread\n");
	}

	ObReferenceObjectByHandle(	
			FlusherThreadHandle,	// Handle: Specifies an open handle for an object.
			GENERIC_ALL,		// Access mask: Specifies the requested types of access to the object
			NULL,				//ObjectType OPTIONAL
			KernelMode,			//AccessMode
			&pFlusherThread,		//Pointer to a variable that receives a pointer to the object's body
			NULL);				//

	//Wait
	Status=KeWaitForSingleObject(
		&ThreadStartedEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);
		
	if(Status == STATUS_TIMEOUT){
		DbgPrint("Error starting system thread\n");
	}else{
		DbgPrint("Thread started sucessfully\n");
	}
	return STATUS_SUCCESS;
}

__declspec(dllexport) VOID
KlogDone( )
{
//	__debugbreak();
	DbgPrint("Call KlogDone\n");
	TimeToExit = TRUE;

	KeWaitForSingleObject(
		pFlusherThread,
		//&ThreadStopEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	//ExFreePool
	//Dereference
	ZwClose(FileHandle);
}

NTSTATUS
KlogTest( )
{
	KIRQL PrevIrql;
	
//	__debugbreak();
	DbgPrint("Call KlogTest\n");

	KeRaiseIrql(HIGH_LEVEL,
		&PrevIrql);

	Klog("I,m HIGH!!!\n");
	KeLowerIrql(PrevIrql);
	return STATUS_SUCCESS;
}

// The DLL must have an entry point, but it is never called.
//
NTSTATUS
DriverEntry(IN PDRIVER_OBJECT DriverObject,
		IN PUNICODE_STRING RegistryPath)
{

	__debugbreak();
	KlogInit();
	Klog("1111111111111111111111111111111111111111111111111!");
	KlogTest();
	Klog("22222222222222222222222222222222222222222222222222");
	KlogTest();
	Klog("33333333333333333333333333333333333333333333333333");
	KlogTest();
	Klog("44444444444444444444444444444444444444444444444444");
	KlogTest();
	Klog("55555555555555555555555555555555555555555555555555");
	KlogTest();
	Klog("66666666666666666666666666666666666666666666666666");
	KlogTest();
	Klog("77777777777777777777777777777777777777777777777777");
	KlogTest();
	Klog("88888888888888888888888888888888888888888888888888");
	KlogTest();
	Klog("99999999999999999999999999999999999999999999999999");
	KlogTest();
	Klog("00000000000000000000000000000000000000000000000000");
	Klog("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");
	Klog("++++++++++++++++++++++++++++++++++++++++++++++++++");
	Klog("--------------------------------------------------");
	Klog("==================================================");
	KlogDone();
	return STATUS_SUCCESS;
}


{
a();if () goto erra;
b();if () goto errb;
c();if () goto errc;
d();if () goto errd
return STATUS_SUCCESS;
errd:
~c();
errc:
~b();
errb:
~a();
erra:
return Status;
}

unload()
{
~d();
~c();
~b();
~a();
}

