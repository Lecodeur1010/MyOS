#include "func.h"
#include <efi.h>
#include <efilib.h>
#include "display.h"

EFI_INPUT_KEY WaitForInput(){
    EFI_INPUT_KEY Key;
    Key.ScanCode=0;
    Key.UnicodeChar=0;
    UINTN index;
    EFI_STATUS status;
    uefi_call_wrapper(ST->BootServices->WaitForEvent, 3,
                      1, ST->ConIn->WaitForKey, &index);
    status = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2,
                               ST->ConIn, &Key);
    if(EFI_ERROR(status)){
        return Key;
    }
    // Convert QWERTY to AZERTY; comment line 20 and uncomment line 21 to disable this feature
    return QwertyToAzerty(Key);
    //return Key;
}

EFI_INPUT_KEY QwertyToAzerty(EFI_INPUT_KEY key){
    CHAR16 keyO ;
    switch (key.UnicodeChar)
    {
    case L'q': keyO = L'a'; break;
    case L'Q': keyO = L'A'; break;
    case L'w': keyO = L'z'; break;
    case L'W': keyO = L'Z'; break;
    case L'a': keyO = L'q'; break;
    case L'A': keyO = L'Q'; break;
    case L'z': keyO = L'w'; break;
    case L'Z': keyO = L'W'; break;
    case L';': keyO = L'm'; break;
    case L':': keyO = L'M'; break;
    case L'm': keyO = L','; break;
    case L'M': keyO = L'?'; break;
    case L'!': keyO = L'1'; break;
    case L'@': keyO = L'2'; break;
    case L'#': keyO = L'3'; break;
    case L'$': keyO = L'4'; break;
    case L'%': keyO = L'5'; break;
    case L'^': keyO = L'6'; break;
    case L'&': keyO = L'7'; break;
    case L'*': keyO = L'8'; break;
    case L'(': keyO = L'9'; break;
    case L')': keyO = L'0'; break;
    case L'.': keyO = L':'; break;
    case L'<': keyO = L'.'; break;
    default: keyO = key.UnicodeChar; break;
    }
    EFI_INPUT_KEY key1;
    key.UnicodeChar = keyO;
    return key;
}

void* kmalloc(UINTN Size){
    return AllocatePool(Size);
}

void kfree(void* pointer){
    FreePool(pointer);
}



