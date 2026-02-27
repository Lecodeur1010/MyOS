#include "func.h"
#include <efi.h>
#include <efilib.h>
extern EFI_SYSTEM_TABLE *ST;
EFI_FILE_PROTOCOL *root;
//Utilities 
void CPrint(UINTN color, CONST CHAR16 *fmt, ...){
    va_list args;
    CHAR16 buffer[256];

    va_start(args, fmt);    
    UnicodeVSPrint(buffer, sizeof(buffer), fmt, args);
    va_end(args);                            
    uefi_call_wrapper(gST->ConOut->SetAttribute,2,gST->ConOut, color); 
    uefi_call_wrapper(gST->ConOut->OutputString,2,gST->ConOut, buffer);
    uefi_call_wrapper(gST->ConOut->SetAttribute,2,gST->ConOut, Info);
}
