#include <efi.h>
#include <efilib.h>

#ifndef MEMORY
#define MEMORY

typedef struct {
    UINTN From;
    BOOLEAN Used;
} MEMORY_MAP;

void* kmalloc(UINTN Size);
void kfree(void* pointer);
#endif