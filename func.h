#include <efi.h>
#include <efilib.h>

#ifndef FUNC
#define FUNC
CHAR16 QwertyToAzerty(CHAR16 key);

void* kmalloc(UINTN Size);
void kfree(void* pointer);
#endif