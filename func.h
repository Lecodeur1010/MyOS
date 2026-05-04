#include <efi.h>
#include <efilib.h>

#ifndef FUNC_H
#define FUNC_H

#define CheckError(status) do { if (EFI_ERROR(status)) return status; } while(0)
#define CheckBuffer(buffer) do { if (!(buffer)) return EFI_OUT_OF_RESOURCES; } while(0)

enum VectorSize {
    VECTOR128,
    VECTOR256,
    VECTOR512
};

EFI_INPUT_KEY WaitForInput();
EFI_STATUS ExitBootServices(EFI_HANDLE ImageHandle);
EFI_STATUS FormatWithUnit(UINTN val,CHAR16* buf,UINTN* bufSize);
VOID Char8ToChar16(CHAR8 *str, CHAR16* buf, UINTN lenInChar);
void SerialInit();
void SerialWrite(const CHAR16 *str);
void SerialWrite8(const CHAR8 *str);
#endif