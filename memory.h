#include <efi.h>
#include <efilib.h>
#ifndef MEMORY_H
#define MEMORY_H

typedef struct {
    UINTN From;
    BOOLEAN Used;
} MEMORY_MAP;

#include "vars.h"

void* kmalloc(UINTN Size);
void kfree(void* pointer);
EFI_STATUS krealloc(VOID** pointer,UINTN size);
static inline void addToUsed(INT64 val){UsedMem+=val;}
#endif