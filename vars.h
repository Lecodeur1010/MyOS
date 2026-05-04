#include <efi.h>
#include <efilib.h>

#ifndef VARS_H
#define VARS_H
extern BOOLEAN InBS;
extern UINTN MemSize;
extern UINTN UsableMemSize;
extern UINTN UsedMem;

extern VOID* HeapStart;
extern MEMORY_MAP* MapStart;
extern UINTN HeapSize;
extern UINTN MapSize;

#endif