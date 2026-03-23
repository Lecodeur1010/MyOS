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
    CHAR16* OffsetedBuffer = buffer;
    while(*OffsetedBuffer == L' ')OffsetedBuffer++;
    if(*OffsetedBuffer==L'\0') {
        kfree(buffer);
        return EFI_SUCCESS;
    }
    UINTN ArgCount = 1;
    for(UINTN i = 1;OffsetedBuffer[i]!=L'\0';i++){//We skip the first char because it's from the name of the command
        if(OffsetedBuffer[i]==L' ' && OffsetedBuffer[i-1]!=L' '){
            while(OffsetedBuffer[i]==L' ')i++;
            if(OffsetedBuffer[i]==L'\0')break;
            ArgCount++;
        }
    }
    CHAR16* argv[ArgCount];
    argv[0] = OffsetedBuffer;
    UINTN j = 1;

    for (UINTN i = 1; OffsetedBuffer[i] != L'\0'; i++) {
        if (j == ArgCount) break;
        if (OffsetedBuffer[i] == L' ' && OffsetedBuffer[i-1] != L' ') {
            OffsetedBuffer[i] = L'\0';
            i++;
            while (OffsetedBuffer[i] == L' ') i++;
            if (OffsetedBuffer[i] == L'\0') break;
            argv[j++] = OffsetedBuffer + i;
        }
    }
        
    for(UINTN i = 0;i<CMD_COUNT;i++){
        if(!StrCmp(argv[0],Commands[i].name)){
            Commands[i].func(ArgCount, argv);
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