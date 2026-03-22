#include <efi.h>
#include <efilib.h>
#include "func.h"
#include "cmd.h"
#include "display.h"

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
    // Convert QWERTY to AZERTY; comment line 20 and uncomment line 21 to disable this feature
    return QwertyToAzerty(Key.UnicodeChar);
    //return Key.UnicodeChar;
}

CHAR16* WaitForCommand(){
    CHAR16* buffer = kmalloc(256*sizeof(CHAR16));
    uint8_t pos = 0;
    
    if(!buffer)return NULL;
    void *prompt = GetPrompt();
    CPrint(THEME_PROMPT,prompt);
    kfree(prompt);

    while(1){
        CHAR16 Key = WaitForInput();

        
        if(!Key)continue;
        if(Key == L'\b' && pos>0){
            pos--;
            CPrint(THEME_INFO,L"\b \b");
        }
        else if(Key == L'\r' || Key == L'\n'){
            buffer[pos]=L'\0';
            CPrint(THEME_INFO,L"\r\n");
            return buffer;           
        }
        else if (pos < 255 && Key >= ' '){
            buffer[pos++]=Key;
            CPrint(THEME_INFO,L"%c",Key);

        }
        
    }

}

EFI_STATUS RunCMD(CHAR16* buffer){
    UINTN Offset = 0;
    while(buffer[Offset] == L' ')Offset++;
    if(buffer[Offset]==L'\0') {
        kfree(buffer);
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
            kfree(buffer);
            return EFI_SUCCESS;
        }
    }
    CPrint(THEME_ERROR,L"Error : CMD \"%s\" not recognized\n",buffer);
    kfree(buffer);
    return EFI_NOT_FOUND;

}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    
    InitializeLib(ImageHandle, SystemTable);
    EFI_STATUS status = GopInit();
    if (EFI_ERROR(status)) return status;
    FillDisplay(RGB(0,0,0));


    Init(ImageHandle);

    while(1){
        CHAR16* cmd = WaitForCommand();
        if(cmd)
            RunCMD(cmd);}
    return EFI_SUCCESS;
}