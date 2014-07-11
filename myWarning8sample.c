#include <wdm.h>
#include <Ntstrsafe.h>
#define STDCALL __stdcall

/*
*  Undocument function 
*/

NTSTATUS STDCALL ZwCreateEvent(
	OUT PHANDLE  EventHandle,
	IN ACCESS_MASK  DesiredAccess,
	IN POBJECT_ATTRIBUTES  ObjectAttributes OPTIONAL,
	IN EVENT_TYPE  EventType,
	IN BOOLEAN  InitialState);

NTSTATUS
	DriverEntry(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath
	);

//KSTART_ROUTINE FlushRingBuffer;

typedef struct Buffer{
	int Size;
	char* head;
	char* tail;
}  BUFFER;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

//Global variables
SIZE_T  NumberOfBytes=512;
PKEVENT pThreadStartedEvent;
PVOID pThreadStopEvent;
KEVENT pActivateFlusherEvent;
BOOLEAN TimeToExit;
BUFFER RingBuffer;
HANDLE   FileHandle;//
 
VOID CreateRingBuffer(){
	RingBuffer.head=NULL;
	RingBuffer.head=ExAllocatePool(
		NonPagedPool,
		NumberOfBytes
		);
	if(RingBuffer.head==NULL){
		DbgPrint("Couldn't allocate memory\n");
	}else{
		DbgPrint("Allocated memory succesfully\n");
	}
	RingBuffer.tail=RingBuffer.head;
	RingBuffer.Size=NumberOfBytes;
}

VOID WriteToRingBuffer(char** dataPtr, SIZE_T BytesNum){
	RtlCopyMemory(
		RingBuffer.head,
		dataPtr,
		BytesNum
		);
	RingBuffer.head+=BytesNum;
	KeSetEvent(&pActivateFlusherEvent,
		0,
		FALSE);
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
	PLARGE_INTEGER Timeout;
	__debugbreak();
	KeSetEvent(
    		&pThreadStartedEvent,
    		0,
    		FALSE);
	__debugbreak();
	InitializeObjectAttributes(
		&ObjAttr,
		&pathBuffer,  			//ObjectName
		OBJ_CASE_INSENSITIVE, 	//Attributes
		NULL,			//RootDirectory
		NULL);			//SecurityDescriptor
		__debugbreak();
	//Timeout.QuadPart = -100000000 ;
	Timeout = -100000000 ;
	while((!TimeToExit) || (RingBuffer.tail-RingBuffer.head)!=0){
		__debugbreak();
		KeWaitForSingleObject(
				&pActivateFlusherEvent,
				Executive,
				KernelMode ,
				FALSE,
				&Timeout);

		__debugbreak();
		// Do not try to perform any file operations at higher IRQL levels.
		// Instead, you may use a work item or a system worker thread to perform file operations.
		if(KeGetCurrentIrql() != PASSIVE_LEVEL){
			//  return STATUS_INVALID_DEVICE_STATE; 
			DbgPrint("STATUS_INVALID_DEVICE_STATE\n");
//			return(-1);
		}
		//TODO: DPC
		__debugbreak();	
		Status=ZwWriteFile(
			FileHandle,
			NULL,
			NULL,
			NULL,
			&ioStatusBlock,
			RingBuffer.tail,
			RingBuffer.head-RingBuffer.tail,
			NULL,
			NULL
			);

		__debugbreak();
		if(Status!=STATUS_SUCCESS){
			DbgPrint("Write to file succes");
		}else{
			DbgPrint("Write to file fail");
		}
	}

	ZwClose(FileHandle); //TODO: move to KLogDone();
}

NTSTATUS
DllInitialize( IN PUNICODE_STRING pus )
{
	__debugbreak();
	DbgPrint("SAMPLE: DllInitialize(%S)\n", pus->Buffer );
	return STATUS_SUCCESS;
}

NTSTATUS
DllUnload( )
{
	__debugbreak();
	DbgPrint("SAMPLE: DllUnload\n");
	return STATUS_SUCCESS;
}

__declspec(dllexport) NTSTATUS
Klog(char* message)
{
	
	__debugbreak();
	DbgPrint("Call Klog\n");
	return STATUS_SUCCESS;
}

NTSTATUS
KlogInit( )
{
	NTSTATUS Status;// Result of the last operation
	HANDLE FlusherThreadHandle;//Handle of flusher thread
	IO_STATUS_BLOCK    ioStatusBlock;//attr for creating file
	OBJECT_ATTRIBUTES ObjAttr;
	UNICODE_STRING pathToLogFile;
	PKTHREAD pFlusherThread;
	//PVOID pEvent;
	//HANDLE hEvent;

	__debugbreak();
	DbgPrint("Call KlogInit\n");

	RtlInitUnicodeString(
		&pathToLogFile,
		L"\\DosDevices\\C:\\MyBufferLogger.txt");
	InitializeObjectAttributes(
		&ObjAttr,
		&pathToLogFile,  			//ObjectName
		OBJ_CASE_INSENSITIVE, 	//Attributes
		NULL,			//RootDirectory
		NULL);			//SecurityDescriptor
		
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
	__debugbreak();
	while(KeGetCurrentIrql() != PASSIVE_LEVEL){
			//  return STATUS_INVALID_DEVICE_STATE; 
			DbgPrint("Waiting for my IRQL\n");
//			return(-1);
		}
		__debugbreak();
	KeInitializeEvent(
		&pThreadStartedEvent,
		SynchronizationEvent,
		FALSE);
		DbgPrint("KeInitializeEvent was completed\n");
	__debugbreak();
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
		&pThreadStartedEvent,
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
	DbgPrint("Call KlogDone\n");
	// return STATUS_SUCCESS;
}

NTSTATUS
KlogTest( )
{
	DbgPrint("Call KlogTest\n");
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
	//Klog()

	return STATUS_SUCCESS;
}
