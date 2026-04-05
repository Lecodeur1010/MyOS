#include <efi.h>
#include <efilib.h>

#ifndef FUNC
#define FUNC
EFI_INPUT_KEY WaitForInput();
EFI_INPUT_KEY QwertyToAzerty(EFI_INPUT_KEY key);
EFI_STATUS ExitBootServices(EFI_HANDLE ImageHandle);
#endif