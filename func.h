#include <efi.h>
#include <efilib.h>

#ifndef FUNC
#define FUNC
EFI_INPUT_KEY WaitForInput();
EFI_INPUT_KEY QwertyToAzerty(EFI_INPUT_KEY key);

void* kmalloc(UINTN Size);
void kfree(void* pointer);
#endif