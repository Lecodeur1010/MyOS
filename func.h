#include <efi.h>
#include <efilib.h>

#ifndef FUNC
#define FUNC
#define Info EFI_WHITE
#define Warning EFI_YELLOW
#define Error EFI_LIGHTRED
#define Sucess EFI_GREEN
void CPrint( UINTN color, CONST CHAR16 *fmt, ...);
CHAR16 QwertyToAzerty(CHAR16 key);
#endif