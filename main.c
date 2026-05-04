#include <efi.h>
#include <efilib.h>
#include "func.h"
#include "cmd.h"
#include "display.h"
#include "memory.h"
#include "pci.h"
#include "disk.h"
#include "fat.h"

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
            EFI_STATUS status = Commands[i].func(ArgCount, argv);
            if(EFI_ERROR(status)){
                CPrint(THEME_ERROR,L"%s exited with error code %u : %r\n",Commands[i].name,status,status);
            }
            kfree(buffer);
            return status;
        }
    }
    CPrint(THEME_ERROR,L"Error : CMD \"%s\" not recognized\n",OffsetedBuffer);
    kfree(buffer);
    return EFI_NOT_FOUND;

}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    
    EFI_STATUS status = GopInit();
    FillDisplay(0);
    status =  ExitBootServices(ImageHandle);
    SerialInit();
    ResetBackBuffer();
    CheckError(status);
    CPrint(THEME_SUCCESS,L"Kernel loaded !\n");
    EnumeratePci();
    CPrintWait(TRUE);
    for(UINTN i = 0; i<PciCount;i++){
        PCI_DEVICE device = PciList[i];
        CPrint(THEME_INFO,L"Bus %3u device %3u function %3u : classcode %2x subclass %2x prog_if %2x(%s)\n",device.bus,device.device,device.func,device.class_code,device.subclass,device.prog_if,PciClassDescrption[device.class_code>17?0:device.class_code]);
    }
    CPrintWait(FALSE);
    CPrint(THEME_INFO,L"Initializing IDE ...\n");
    status = IDEInit();
    if(status == EFI_SUCCESS) CPrint(THEME_SUCCESS,L"IDE initialized with success (%u drives) !\n",IDEDiskCount);
    else  CPrint(THEME_WARNING,L"No IDE drives found !\n");
    CPrint(THEME_INFO,L"Initializing AHCI ...\n");
    status = AHCIInit();
    if(EFI_ERROR(status)) CPrint(THEME_WARNING,L"Error while init AHCI : %r !\n");
    if(AHCIDiskCount == 0) CPrint(THEME_WARNING,L"No AHCI drives found !\n");
    else CPrint(THEME_SUCCESS,L"AHCI initialized with success (%u drives) !\n",AHCIDiskCount);
    CHAR16 Label[12];
    CPrint(THEME_ERROR,L"%u partition\n",PartitionCount);
    for(UINTN i = 0; i < PartitionCount; i++){
        if(PartitionList[i].fsType==0x0C)
            Char8ToChar16(((FAT32_FS*)PartitionList[i].fs)->bpb.volume_label,Label,11);
        else
            StrCpy(Label,L"<NOT FAT32>");
        Label[11]=L'\0';
        for(INTN j = 10;j>=0; j--){
            if(Label[j]==L' ') Label[j] = L'\0';
            else break;
        }
        CHAR16 Size[12];
        UINTN SizeSize = sizeof(Size);
        UINTN TotalSector = PartitionList[i].sectorCount;
        UINTN SectorSize = PartitionList[i].disk->sector_size;
        FormatWithUnit(TotalSector*SectorSize,Size,&SizeSize);
        CPrint(THEME_INFO,L"Partition fs%u: : ID internal: %u; Label : %s ; Size : %s ; Type : %u (%s)\n",PartitionList[i].OSUID,PartitionList[i].disk->handle,Label,Size,PartitionList[i].fsType,GetFSnameMBR(PartitionList[i].fsType));
    
    }

    Init();

    while(1){
        CHAR16* cmd = WaitForCommand();
        if(cmd)
            RunCMD(cmd);
    }
    return EFI_SUCCESS;
}