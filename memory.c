#include <efi.h>
#include <efilib.h>
#include "memory.h"

#define ALIGN16(s) (((s) + 15) & ~15)

static BOOLEAN IsAllocatorReady = FALSE;
static MEM_HEADER* FirstBlock = NULL;
static UINTN DataSize = 0;

// --- Initialisation de l'allocateur ---

EFI_STATUS InitAllocator() {
    EFI_STATUS Status;
    UINTN MemoryMapSize = 0;
    EFI_MEMORY_DESCRIPTOR* MemoryMap = NULL;
    UINTN MapKey;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;

    Status = uefi_call_wrapper(gBS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        return Status;
    }
    MemoryMapSize += 2 * DescriptorSize;
    Status = uefi_call_wrapper(gBS->AllocatePool, 3, EfiLoaderData, MemoryMapSize, (VOID**)&MemoryMap);
    if (EFI_ERROR(Status)) {
        return Status;
    }
    Status = uefi_call_wrapper(gBS->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR(Status)) {
        uefi_call_wrapper(gBS->FreePool, 1, MemoryMap);
        return Status;
    }
    EFI_PHYSICAL_ADDRESS BestBase = 0;
    UINTN MaxPages = 0;
    UINTN Entries = MemoryMapSize / DescriptorSize;
    EFI_MEMORY_DESCRIPTOR* Desc = MemoryMap;

    for (UINTN i = 0; i < Entries; i++) {
        if (Desc->Type == EfiConventionalMemory) {
            if (Desc->NumberOfPages > MaxPages) {
                MaxPages = Desc->NumberOfPages;
                BestBase = Desc->PhysicalStart;
            }
        }
        Desc = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)Desc + DescriptorSize);
    }
    uefi_call_wrapper(gBS->FreePool, 1, MemoryMap);

    if (MaxPages == 0) {
        return EFI_OUT_OF_RESOURCES;
    }

    EFI_PHYSICAL_ADDRESS TargetAddress = BestBase;
    Status = uefi_call_wrapper(gBS->AllocatePages, 4, AllocateAddress, EfiLoaderData, MaxPages, &TargetAddress);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    DataSize = MaxPages * EFI_PAGE_SIZE;
    FirstBlock = (MEM_HEADER*)TargetAddress;
    FirstBlock->Magic = HEAP_MAGIC;
    FirstBlock->Size = DataSize - sizeof(MEM_HEADER);
    FirstBlock->Prev = NULL;
    FirstBlock->Next = NULL;
    FirstBlock->Used = FALSE;
    
    IsAllocatorReady = TRUE;
    return EFI_SUCCESS;
}

// --- kmalloc (First-Fit) ---

VOID* kmalloc(UINTN Size) {
    if (!Size || !IsAllocatorReady)
        return NULL;

    Size = ALIGN16(Size);

    MEM_HEADER* A = FirstBlock;
    while (A) {
        if (!A->Used && A->Size >= Size) {
            // Découpage du bloc (Split) si l'espace restant est suffisant
            if (A->Size >= Size + sizeof(MEM_HEADER) + 16) {
                MEM_HEADER* C = A->Next;
                MEM_HEADER* B = (MEM_HEADER*)((UINT8*)A + sizeof(MEM_HEADER) + Size);
                
                B->Magic = HEAP_MAGIC;
                B->Size = A->Size - Size - sizeof(MEM_HEADER);
                B->Used = FALSE;
                B->Prev = A;
                B->Next = C;

                if (C) {
                    C->Prev = B;
                }
                A->Size = Size;
                A->Next = B;
            }
            A->Used = TRUE;
            return (VOID*)(A + 1); 
        }

        A = A->Next;
    }
    return NULL; // Plus de mémoire disponible
}

// --- kfree ---

VOID kfree(VOID *ptr) {
    if (!ptr || !IsAllocatorReady) return;

    UINTN HeapStart = (UINTN)FirstBlock;
    UINTN HeapEnd   = HeapStart + DataSize;
    UINTN PtrAddr   = (UINTN)ptr;

    // 1. Vérification des bornes du tas
    if (PtrAddr < HeapStart || PtrAddr >= HeapEnd) 
        return;

    // 2. Alignement minimal 16-bits
    if ((PtrAddr & 15) != 0) 
        return;

    MEM_HEADER* B = (MEM_HEADER*)ptr - 1;
    if (B->Magic != HEAP_MAGIC) return;

    B->Used = FALSE;

    // Fusion avec le bloc suivant s'il est libre
    if (B->Next && !B->Next->Used) {
        MEM_HEADER* C = B->Next;
        B->Size += sizeof(MEM_HEADER) + C->Size;
        B->Next = C->Next;
        if (C->Next) {
            C->Next->Prev = B;
        }
    }

    // Fusion avec le bloc précédent s'il est libre
    if (B->Prev && !B->Prev->Used) {
        MEM_HEADER* A = B->Prev;
        A->Size += sizeof(MEM_HEADER) + B->Size;
        A->Next = B->Next;
        if (B->Next) {
            B->Next->Prev = A;
        }
    }
}

// --- krealloc ---

VOID* krealloc(VOID* ptr, UINTN Size) {
    if (!ptr) {
        return kmalloc(Size);
    }
    if (Size == 0) {
        kfree(ptr);
        return NULL;
    }
    MEM_HEADER* A = (MEM_HEADER*)ptr - 1;
    if (A->Magic != HEAP_MAGIC) {
        return NULL;
    }
    Size = ALIGN16(Size);
    if (A->Size >= Size) {
        return ptr;
    }
    if (A->Next && !A->Next->Used && (A->Size + sizeof(MEM_HEADER) + A->Next->Size) >= Size) {
        MEM_HEADER* NextBlock = A->Next;
        A->Size += sizeof(MEM_HEADER) + NextBlock->Size;
        A->Next = NextBlock->Next;
        if (A->Next) {
            A->Next->Prev = A;
        }
        return ptr;
    }
    VOID* nptr = kmalloc(Size);
    if (!nptr) {
        return NULL;
    }
    CopyMem(nptr, ptr, A->Size);
    kfree(ptr);
    return nptr;
}

// --- Statistiques Mémoire ---

VOID GetMemoryDetails(VOID** _DataAdress, UINTN* _DataSize, UINTN* _UsedSize, UINTN *_IncludeHeaders) {
    if (!_DataAdress || !_DataSize || !_UsedSize || !_IncludeHeaders) return;

    *_DataAdress = FirstBlock;
    *_DataSize = DataSize;
    *_UsedSize = 0;
    *_IncludeHeaders = 0;

    MEM_HEADER* Next = FirstBlock;
    while (Next) {
        if (Next->Used) {
            *_UsedSize += Next->Size;
            *_IncludeHeaders += Next->Size + sizeof(MEM_HEADER);
        }
        Next = Next->Next;
    }
}