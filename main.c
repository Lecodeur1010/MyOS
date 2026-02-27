#include <efi.h>
#include <efilib.h>
#include "func.h"
#include "cmd.h"

CHAR16 WaitForInput()
{
    EFI_INPUT_KEY Key;
    UINTN index;
    EFI_STATUS status;
    uefi_call_wrapper(ST->BootServices->WaitForEvent, 3,
                      1, ST->ConIn->WaitForKey, &index);
    status = uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2,
                               ST->ConIn, &Key);
    if(EFI_ERROR(status)){
        return 0;
    }
    return Key.UnicodeChar;
}

CHAR16* WaitForCommand(){
    CHAR16* buffer = AllocatePool(256*sizeof(CHAR16));
    uint8_t pos = 0;
    if(!buffer)return NULL;
    CPrint(EFI_YELLOW,WorkingDir);
    CPrint(EFI_YELLOW,L">");
    while(1){
        CHAR16 Key = WaitForInput();

        
        if(!Key)continue;
        if(Key == L'\b' && pos>0){
            pos--;
            CPrint(Info,L"\b \b");
        }
        else if(Key == L'\r' || Key == L'\n'){
            buffer[pos]=L'\0';
            CPrint(Info,L"\r\n");
            return buffer;           
        }
        else if (pos < 255 && Key >= ' '){
            buffer[pos++]=Key;
            CPrint(Info,L"%c",Key);

        }
        
    }

}

EFI_STATUS RunCMD(CHAR16* buffer){
    UINTN Offset = 0;
    while(buffer[Offset] == L' ')Offset++;
    if(buffer[Offset]==L'\0') {
        FreePool(buffer);
        return EFI_SUCCESS;
    }
    CHAR16* Args = NULL;        
    for (UINTN i = 0; buffer[i+Offset] != L'\0'; i++) {
        if (buffer[i+Offset] == L' ') {
            buffer[i+Offset] = L'\0';
            Args = &buffer[i + 1 + Offset];
            break;
        }
    }
    for(UINTN i = 0;i<CMD_COUNT;i++){
        if(!StrCmp(buffer+Offset,Commands[i].name)){
            Commands[i].func(Args);
            FreePool(buffer);
            return EFI_SUCCESS;
        }
    }
    CPrint(Error,L"Error : CMD \"%s\" not recognized\n",buffer);
    FreePool(buffer);
    return EFI_NOT_FOUND;

}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    uefi_call_wrapper(SystemTable->ConOut->ClearScreen,1,SystemTable->ConOut);    
    CPrint(Info,L"MyOS (Beta version)\n\"help\" for help\n");
    Init(ImageHandle);
    while(1){
        CHAR16* cmd = WaitForCommand();
        if(cmd)
            RunCMD(cmd);}
    return EFI_SUCCESS;
}