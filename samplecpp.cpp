#include <wdm.h>
__debugbreak();
EXTERN_C NTSTATUS
DriverEntry(
  IN PDRIVER_OBJECT DriverObject,
  IN PUNICODE_STRING RegistryPath
);
__debugbreak();
#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

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
    DbgPrint("SAMPLE: DllUnload\n");
    return STATUS_SUCCESS;
}

NTSTATUS
SampleDouble( int * pValue )
{
    DbgPrint("SampleDouble: %d\n", *pValue);
    *pValue *= 2;
    return STATUS_SUCCESS;
}


NTSTATUS
SampleReadRegistry( )
{
    DbgPrint("SampleReadRegistry\n");
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
    return STATUS_SUCCESS;
}
