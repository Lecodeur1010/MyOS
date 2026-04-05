#include "func.h"
#include <efi.h>
#include <efilib.h>
#include "display.h"
#include "memory.h"
#include "vars.h"
#include "io.h"

static const char scancode_map[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0,
    '\\','z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ',
    // ...
};

EFI_INPUT_KEY WaitForInput() {
    UINT8 scancode;
    EFI_INPUT_KEY Key = {0};

    // attendre un octet disponible
    while(!(inb(0x64) & 1));
    scancode = inb(0x60);

    // ignorer break codes
    if(scancode & 0x80) return Key;

    // convertir en CHAR16
    Key.UnicodeChar = (CHAR16)scancode_map[scancode];
    return Key;
}

EFI_STATUS ExitBootServices(EFI_HANDLE ImageHandle){
    UINTN MapSize = 0;
    EFI_MEMORY_DESCRIPTOR* Map = NULL;
    UINTN MapKey = NULL;
    UINTN DescriptorSize = 0,DesciptorVersion = 0;
    EFI_STATUS status;
    InBS = FALSE;
    status = uefi_call_wrapper(BS->GetMemoryMap,5,&MapSize,Map,&MapKey,&DescriptorSize,&DesciptorVersion);
    Map = kmalloc(MapSize+2*sizeof(EFI_MEMORY_DESCRIPTOR));
    status = uefi_call_wrapper(BS->GetMemoryMap,5,&MapSize,Map,MapKey,&DescriptorSize,&DesciptorVersion);
    if(EFI_ERROR(status)){
        CPrint(THEME_ERROR,L"Error while fetching memory map : %r",status);
        return status;
    }
    status = uefi_call_wrapper(BS->ExitBootServices,2,ImageHandle,MapKey);
    if(EFI_ERROR(status)){
        CPrint(THEME_ERROR,L"Error while exiting BS : %r",status);
        return status;
    }
    kfree(Map);
    CPrint(THEME_SUCCESS,L"BS exited successfuly !\n");
    return status;
}


