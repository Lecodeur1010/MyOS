#include "func.h"
#include <efi.h>
#include <efilib.h>
#include "display.h"

CHAR16 QwertyToAzerty(CHAR16 key){
    CHAR16 keyO ;
    switch (key)
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
    default: keyO = key; break;
    }
    return keyO;
}

void* kmalloc(UINTN Size){
    return AllocatePool(Size);
}

void kfree(void* pointer){
    FreePool(pointer);
}



