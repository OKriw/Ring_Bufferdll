#include <wdm.h>
#include <Ntstrsafe.h>
#define STDCALL __stdcall

NTSTATUS
	DriverEntry(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath
	);

//KSTART_ROUTINE FlushRingBuffer;

typedef struct Buffer{
	int FreeSpace;
	int Size;
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
HANDLE   FileHandle;//
 
VOID CreateRingBuffer(){
	RingBuffer.head=NULL;
	RingBuffer.start=ExAllocatePool(
		NonPagedPool,
		NumberOfBytes
		);
	if(RingBuffer.head==NULL){
		DbgPrint("Couldn't allocate memory\n");
	}else{
		DbgPrint("Allocated memory succesfully\n");
	}
	RingBuffer.head=RingBuffer.start;
	RingBuffer.tail=RingBuffer.start;
	RingBuffer.Size=NumberOfBytes;
	RingBuffer.FreeSpace=NumberOfBytes
}

VOID WriteToRingBuffer(char* dataPtr, SIZE_T BytesNum){
	int lenght;
	if(dataPtr==NULL || BytesNum==0){
		DbgPrint("Data is not correct : size=0 or wrong pointer");
	}
	
	if(RingBuffer.FreeSpace < BytesNum){
		BytesNum=RingBuffer.FreeSpace;
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
	RingBuffer.head=(RingBuffer.start - RingBuffer.head + NumBytes) % RingBuffer.Size + RingBuffer.start;
	RingBuffer.FreeSpace-=NumBytes;

	if(RingBuffer.FreeSpace < RingBuffer.Size/3){
		KeSetEvent(&ActivateFlusherEvent,
			0,
			FALSE);
		}
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

	__debugbreak();
	KeSetEvent(
    		&ThreadStartedEvent,
    		0,
    		FALSE);
	
	InitializeObjectAttributes(
		&ObjAttr,
		&pathBuffer,  			//ObjectName
		OBJ_CASE_INSENSITIVE, 	//Attributes
		NULL,			//RootDirectory
		NULL);			//SecurityDescriptor
		
	Timeout.QuadPart = -10000000 ;
	
	while((!TimeToExit) || (RingBuffer.tail-RingBuffer.head)!=0){
		KeWaitForSingleObject(
				&ActivateFlusherEvent,
				Executive,
				KernelMode ,
				FALSE,
				&Timeout);
				
		// Do not try to perform any file operations at higher IRQL levels.
		// Instead, you may use a work item or a system worker thread to perform file operations.
		if(KeGetCurrentIrql() != PASSIVE_LEVEL){
			DbgPrint("STATUS_INVALID_DEVICE_STATE\n");
			//TODO: DPC

		}
		
		if(RingBuffer.head != RingBuffer.tail){

			if(RingBuffer.head < RingBuffer.tail){
				length = RingBuffer.start + RingBuffer.size - RingBuffer.tail
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
					DbgPrint("Write to file succes\n");
				}else{
					DbgPrint("Write to file fail\n");
				}

				Status=ZwWriteFile(
					FileHandle,
					NULL,
					NULL,
					NULL,
					&ioStatusBlock,
					RingBuffer.start,
					RingBuffer.head-RingBuffer.start,
					NULL,
					NULL);
				if(Status == STATUS_SUCCESS){
					DbgPrint("Write to file succes\n");
				}else{
					DbgPrint("Write to file fail\n");
				}	
			}else{
				Status=ZwWriteFile(
					FileHandle,
					NULL,
					NULL,
					NULL,
					&ioStatusBlock,
					RingBuffer.tail,
					RingBuffer.tail-RingBuffer.head,
					NULL,
					NULL);
				if(Status == STATUS_SUCCESS){
					DbgPrint("Write to file succes\n");
				}else{
					DbgPrint("Write to file fail\n");
				}
			}
			RingBuffer.tail = RingBuffer.head;
			RingBuffer.FreeSpace=RingBuffer.size;	
		} else {
			DbgPrint("Nothing to write\n");e
		}
	}

	KeSetEvent(
    		&ThreadStopEvent,
    		0,
    		FALSE);
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
	SIZE_T BytesNum = strlen(message)*sizeof(char);

	__debugbreak();
	DbgPrint("Call Klog\n");
	
	WriteToRingBuffer(message,BytesNum);
	return STATUS_SUCCESS;
}

NTSTATUS
KlogInit()
{
	NTSTATUS Status;// Result of the last operation
	HANDLE FlusherThreadHandle;//Handle of flusher thread
	IO_STATUS_BLOCK    ioStatusBlock;//attr for creating file
	OBJECT_ATTRIBUTES ObjAttr;
	UNICODE_STRING pathToLogFile;
	PKTHREAD pFlusherThread;

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

	while(KeGetCurrentIrql() != PASSIVE_LEVEL){
			//  return STATUS_INVALID_DEVICE_STATE; 
			DbgPrint("Waiting for my IRQL\n");
//			return(-1);
		}
	KeInitializeEvent(
		&ThreadStartedEvent,
		SynchronizationEvent,
		FALSE);

	KeInitializeEvent(
		&ThreadStopEvent,
		SynchronizationEvent,
		FALSE);
		
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
	__debugbreak();
	DbgPrint("Call KlogDone\n");
	TimeToExit = TRUE;

	KeWaitForSingleObject(
		&ThreadStopEvent,
		Executive,
		KernelMode,
		FALSE,
		NULL);

	ZwClose(FileHandle);
	// return STATUS_SUCCESS;
}

NTSTATUS
KlogTest( )
{
	__debugbreak();
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
	__debugbreak();
	Klog("First\n");
	Klog("Second\\n");
	Klog("Third\\n");
	KlogDone();
	return STATUS_SUCCESS;
}
