#include <efi.h>
#include <efilib.h>
#include "func.h"
#include "cmd.h"
#include "display.h"

CHAR16* WaitForCommand(){
    CHAR16* buffer = kmalloc(256*sizeof(CHAR16));
    if(!buffer) return NULL;
    uint8_t pos = 0;
    
    if(!buffer)return NULL;
    void *prompt = GetPrompt();
    CPrint(THEME_PROMPT,prompt);
    kfree(prompt);

    while(1){
        CHAR16 Key = WaitForInput().UnicodeChar;

        
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
    BOOLEAN InQuotes = FALSE;
    for(UINTN i = 1;OffsetedBuffer[i]!=L'\0';i++){//We skip the first char because it's from the name of the command
        if(OffsetedBuffer[i]==L'\"'){
            InQuotes=!InQuotes;
            continue;
        }
        if(OffsetedBuffer[i]==L' ' &&  !InQuotes){
            while(OffsetedBuffer[i]==L' ')i++;
            if(OffsetedBuffer[i]==L'\0')break;
            ArgCount++;
            i--;
        }
    }
    CHAR16* argv[ArgCount];
    argv[0] = OffsetedBuffer;
    InQuotes=FALSE;
    UINTN j = 1;
    for (UINTN i = 1; OffsetedBuffer[i] != L'\0'; i++) {
        if(OffsetedBuffer[i]==L'\"'){
            if(InQuotes)OffsetedBuffer[i]=L'\0';
            InQuotes=!InQuotes;
            continue;
        }
        if (OffsetedBuffer[i] == L' '  && !InQuotes) {
            OffsetedBuffer[i] = L'\0';
            i++;
            while (OffsetedBuffer[i] == L' ') i++;
            if (OffsetedBuffer[i] == L'\0') break;
            if (OffsetedBuffer[i] == L'\"')
                argv[j++] = OffsetedBuffer + i+1;
            else 
                argv[j++] = OffsetedBuffer + i;
            i--;
        }
    }
        
    for(UINTN i = 0;i<CMD_COUNT;i++){
        if(!StrCmp(argv[0],Commands[i].name)){
            Commands[i].func(ArgCount, argv);
            kfree(buffer);
            return EFI_SUCCESS;
        }
    }
    CPrint(THEME_ERROR,L"Error : CMD \"%s\" not recognized\n",OffsetedBuffer);
    kfree(buffer);
    return EFI_NOT_FOUND;

}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    EFI_STATUS status = GopInit();
    CPrint(THEME_SUCCESS,L"Kernel loading ... !\n");
    if (EFI_ERROR(status)) return status;
    FillDisplay(RGB(0,0,0));
    Init(ImageHandle);
    
    while(1){
        CHAR16* cmd = WaitForCommand();
        if(cmd)
            RunCMD(cmd);
    }
    return EFI_SUCCESS;
}