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

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif
SIZE_T  NumberOfBytes=512;
PVOID pEvent;
struct Buffer{
	int Size;
	char* head;
	char* tail;
} ;
struct Buffer RingBuffer; 
VOID CreateRingBuffer(){
	RingBuffer.head=NULL;
	RingBuffer.head=ExAllocatePool(
		NonPagedPool,
		NumberOfBytes
		);
	if(RingBuffer.head==NULL){
		DbgPrint("Memory problems\n");
	}else{
		DbgPrint("Memory OK\n");
	}

	RingBuffer.tail=RingBuffer.head;
	RingBuffer.Size=NumberOfBytes;

}

PVOID WriteToRingBuffer(char** dataPtr, SIZE_T BytesNum){
	if(RingBuffer.tail-RingBuffer.head>=RingBuffer.Size/3){
		KeSetEvent(
			hEvent,
			0,
			TRUE);}
	RtlCopyMemory(
		RingBuffer.head,
		dataPtr,
		BytesNum
		);
	RingBuffer.head+=BytesNum;

}
PVOID FlushRingBuffre(){
	IO_STATUS_BLOCK    ioStatusBlock;
	HANDLE   handle;
	NTSTATUS ntstatus;
	NTSTATUS FlushingToFile;
	OBJECT_ATTRIBUTES ObjAttr;
	UNICODE_STRING pathBuffer;
	size_t  cb;
	PVOID pEvent;
	NTSTATUS Status;
	HANDLE hEvent;
	OBJECT_ATTRIBUTES oa;
	UNICODE_STRING us;
	__debugbreak();
	RtlInitUnicodeString(
		&pathBuffer,
		L"\\DosDevices\\C:\\MyBufferLogger.txt");
	InitializeObjectAttributes(
		&ObjAttr,
		&pathBuffer,  			//ObjectName
		OBJ_CASE_INSENSITIVE, 	//Attributes
		NULL,			//RootDirectory
		NULL);			//SecurityDescriptor
	// Do not try to perform any file operations at higher IRQL levels.
	// Instead, you may use a work item or a system worker thread to perform file operations.

	if(KeGetCurrentIrql() != PASSIVE_LEVEL)
		//  return STATUS_INVALID_DEVICE_STATE; 
		DbgPrint("STATUS_INVALID_DEVICE_STATE\n");

	ntstatus = ZwCreateFile(&handle,
		GENERIC_WRITE,
		&ObjAttr,
		&ioStatusBlock,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		0,
		FILE_OVERWRITE_IF, 
		FILE_SYNCHRONOUS_IO_NONALERT,
		NULL, 0);


	RtlInitUnicodeString(
		&us,
		L"\\BaseNamedObjects\\TestEvent");

	InitializeObjectAttributes(
		&oa,
		&us,  			//ObjectName
		OBJ_CASE_INSENSITIVE, 	//Attributes
		NULL,			//RootDirectory
		NULL);			//SecurityDescriptor

	FlushingToFile=ZwWriteFile(
		handle,
		NULL,
		NULL,
		NULL,
		&ioStatusBlock,
		RingBuffer.tail,
		RingBuffer.head-RingBuffer.tail,
		NULL,
		NULL
		);


	if(FlushingToFile!=STATUS_SUCCESS){
		DbgPrint("Writing to file problem");
	}else{
		DbgPrint("I do not know why");
	}

	ZwClose(handle);

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
	SampleDouble( int * pValue )
{
	DbgPrint("SampleDouble: %d\n", *pValue);
	*pValue *= 2;
	return STATUS_SUCCESS;
}


__declspec(dllexport) NTSTATUS
	SampleReadRegistry( )
{
	DbgPrint("SampleReadRegistry\n");
	return STATUS_SUCCESS;
}

__declspec(dllexport) NTSTATUS
	Klog( )
{
	DbgPrint("Call Klog\n");
	return STATUS_SUCCESS;
}

VOID
	MyThreadStart(__in PVOID  StartContext){
		HANDLE   handle;//Create File handle
		NTSTATUS ntstatus;
		NTSTATUS timerStatus;
		//IO_STATUS_BLOCK    ioStatusBlock;
		//OBJECT_ATTRIBUTES ObjAttr;
		//UNICODE_STRING path;
		LARGE_INTEGER timeout;
//#define  BUFFER_SIZE 30
	//	CHAR     buffer[BUFFER_SIZE];
		//size_t  cb;

		NTSTATUS Status;
		HANDLE hEvent;
		//OBJECT_ATTRIBUTES oa;
		//UNICODE_STRING us;
		int counter=20;
		SIZE_T BytesNUM;
		char* dataPtr;

		__debugbreak();
		dataPtr="123";
		BytesNUM=strlen(dataPtr)*sizeof(char);
		WriteToRingBuffer(&dataPtr, BytesNUM);
		FlushRingBuffre();
		RtlInitUnicodeString(
			&path,
			L"\\DosDevices\\C:\\MyLogger.txt");
		InitializeObjectAttributes(
			&ObjAttr,
			&path,  			//ObjectName
			OBJ_CASE_INSENSITIVE, 	//Attributes
			NULL,			//RootDirectory
			NULL);			//SecurityDescriptor
		// Do not try to perform any file operations at higher IRQL levels.
		// Instead, you may use a work item or a system worker thread to perform file operations.

		if(KeGetCurrentIrql() != PASSIVE_LEVEL)
			//  return STATUS_INVALID_DEVICE_STATE; 
			DbgPrint("STATUS_INVALID_DEVICE_STATE\n");

		ntstatus = ZwCreateFile(&handle,
			GENERIC_WRITE,
			&ObjAttr, &ioStatusBlock, NULL,
			FILE_ATTRIBUTE_NORMAL,
			0,
			FILE_OVERWRITE_IF, 
			FILE_SYNCHRONOUS_IO_NONALERT,
			NULL, 0);


		timeout.QuadPart = -5 * 1000000;

		RtlInitUnicodeString(
			&us,
			L"\\BaseNamedObjects\\TestEvent");

		InitializeObjectAttributes(
			&oa,
			&us,  			//ObjectName
			OBJ_CASE_INSENSITIVE, 	//Attributes
			NULL,			//RootDirectory
			NULL);			//SecurityDescriptor

		Status = ZwCreateEvent(&hEvent,
			EVENT_ALL_ACCESS,
			&oa,
			NotificationEvent,
			FALSE);

		if(NT_SUCCESS(Status)){
			DbgPrint("Event created");
		} else {
			DbgPrint("Event Not created");
		}

		Status = ObReferenceObjectByHandle(
			hEvent, 		//Handle
			EVENT_ALL_ACCESS,	//DesiredAccess
			NULL,			//ObjectType
			KernelMode,		//AccessMode
			&pEvent,		//Object
			NULL);			//HandleInformation

		if (!NT_SUCCESS(Status)) {
			ZwClose(hEvent);
			DbgPrint("Failed to reference event \n");
			//return Status;
		};

		while(counter!=0){



			timerStatus = KeWaitForSingleObject(
				pEvent,
				Executive,
				KernelMode,
				FALSE,
				&timeout
				);




			if(timerStatus == STATUS_TIMEOUT){
				if(NT_SUCCESS(ntstatus)) {
					ntstatus = RtlStringCbPrintfA(buffer, sizeof(buffer), "This is %d test\r\n", counter);
					if(NT_SUCCESS(ntstatus)) {
						ntstatus = RtlStringCbLengthA(buffer, sizeof(buffer), &cb);
						if(NT_SUCCESS(ntstatus)) {
							ntstatus = ZwWriteFile(handle, NULL, NULL, NULL, &ioStatusBlock,
								buffer, cb, NULL, NULL);
						}
					}
					//ZwClose(handle);
				}
			}
			counter--;
		}
		ZwClose(handle);
}



NTSTATUS
	KlogInit( )
{
	NTSTATUS status;
	HANDLE threadHandle;
	CreateRingBuffer();
	DbgPrint("Call KlogInit\n");
	status=PsCreateSystemThread(&threadHandle, 
		GENERIC_ALL,
		NULL,
		NULL,
		NULL,
		MyThreadStart,
		NULL);
	if(NT_SUCCESS(status)){
		DbgPrint("Thread Succes\n");
	}
	else{
		DbgPrint("No thread\n");}
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
	DriverEntry(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath
	)
{

	__debugbreak();
	KlogInit();

	return STATUS_SUCCESS;
}
