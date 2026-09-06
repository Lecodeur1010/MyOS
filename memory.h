#pragma once

#include <efi.h>
#include <efilib.h>

#define HEAP_MAGIC 0x48454150 // ASCII pour "HEAP"

typedef struct _MEM_HEADER {
    UINT32 Magic;           // HEAP_MAGIC
    BOOLEAN Used;
    UINT8 Reserved[3];      // Padding
    UINTN Size;             // Size, excluding headers
    struct _MEM_HEADER* Next; 
    struct _MEM_HEADER* Prev;
} MEM_HEADER;


VOID* kmalloc(UINTN Size);
VOID kfree(VOID* ptr);
VOID* krealloc(VOID* ptr, UINTN Size);
EFI_STATUS InitAllocator();
VOID GetMemoryDetails(VOID** _DataAdress, UINTN* _DataSize, UINTN* _UsedSize, UINTN *_IncludeHeaders);

