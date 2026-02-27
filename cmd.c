#include "cmd.h"
#include "func.h"
#include <efi.h>
#include <efilib.h>

COMMAND Commands[] = {
    {L"help",CMDhelp,L"General help"},
    {L"power",CMDpower,L"Reboot or shutdown"},
    {L"time",CMDtime,L"Get date and time"},
    {L"clear",CMDclear,L"Clear the screen"},
    {L"exit",CMDexit,L"Exit"},
    {L"ls",CMDls,L"List directory content"},
    {L"dir",CMDls,L"Alias for ls"},
    {L"cd",CMDcd,L"Change working dir"},
    {L"pwd",CMDpwd,L"Print working dir"},
    {L"mkdir",CMDmkdir,L"Create directory"},
    {L"rm",CMDrm,L"Remove file or directory"}
};
UINTN CMD_COUNT = sizeof(Commands) / sizeof(COMMAND);

EFI_STATUS CMDpower(CHAR16* Args) {
    if (!Args || !StrCmp(Args,L"help") ){CPrint(Info,L"Usage : power off|reset\n");return EFI_INVALID_PARAMETER;}
    else if(!StrCmp(Args,L"off")) {uefi_call_wrapper(RT->ResetSystem,4,EfiResetShutdown,EFI_SUCCESS,0,NULL);}
    else if(!StrCmp(Args,L"reset")) {uefi_call_wrapper(RT->ResetSystem,4,EfiResetWarm,EFI_SUCCESS,0,NULL);}
    else {CPrint(Error,L"Unknown parameter : %s",Args);return EFI_INVALID_PARAMETER;}
    return EFI_SUCCESS;

}
EFI_STATUS CMDtime(CHAR16* Args){
    EFI_TIME ActualTime;
    uefi_call_wrapper(gST->RuntimeServices->GetTime,2,&ActualTime,NULL);
    CPrint(Info,L"Date : %u/%u/%u \n",ActualTime.Year,ActualTime.Month,ActualTime.Day);
    CPrint(Info,L"Time : %02u:%02u:%02u\n",ActualTime.Hour,ActualTime.Minute,ActualTime.Second);
    if(ActualTime.TimeZone == EFI_UNSPECIFIED_TIMEZONE)
        CPrint(Info,L"Timezone unspecified\n");
    else
        CPrint(Info,L"Timezone : UTC%+d:%02d",ActualTime.TimeZone/60,ActualTime.TimeZone < 0 ? -(ActualTime.TimeZone % 60) : ActualTime.TimeZone % 60);
    return EFI_SUCCESS;
}
EFI_STATUS CMDhelp(CHAR16* Args){
    for(uint8_t i = 0; i<CMD_COUNT;i++){
        CPrint(Info,L"%s - %s\n",Commands[i].name,Commands[i].description);
    }
    return EFI_SUCCESS;
}

EFI_STATUS CMDclear(CHAR16* Args){
    uefi_call_wrapper(gST->ConOut->ClearScreen,1,gST->ConOut);
    return EFI_SUCCESS;
}

EFI_STATUS CMDexit(CHAR16* Args){
    Exit(EFI_SUCCESS,0,NULL);
}

EFI_FILE_PROTOCOL *ActualDir;
CHAR16* WorkingDir = NULL;
UINTN WorkingDirSize;

EFI_STATUS CMDls(CHAR16 *Args)
{
    uefi_call_wrapper(ActualDir->SetPosition,2,ActualDir,0);
    UINTN Size = 1024;
    void* buffer = AllocatePool(Size);
    while(1){
        Size = 1024;
        uefi_call_wrapper(ActualDir->Read,3,ActualDir,&Size,buffer);
        if(Size==0){Print(L"\n");return EFI_SUCCESS;} 
        EFI_FILE_INFO *info = (EFI_FILE_INFO*)buffer;
        CHAR16* name = info->FileName;
        if(info->Attribute & EFI_FILE_DIRECTORY){
            CPrint(EFI_LIGHTBLUE,L"%s    ",name);
        } else {
            CPrint(Warning,L"%s    ",name);
        }

    }
    FreePool(buffer);
    return EFI_SUCCESS;
}

void UpdateDir(CHAR16* Path){
    if (!Path) return;

    if (!StrCmp(Path,L".")) return;

    if (!StrCmp(Path,L"..")){
        if (WorkingDirSize <= 1) return; 
        INTN i = WorkingDirSize - 2;
        while(i >= 0 && WorkingDir[i] != L'\\') i--;
        WorkingDir[i+1] = L'\0';
        WorkingDirSize = i + 1;
    }
    else {
        UINTN newSize = (WorkingDirSize + StrLen(Path) + 2) * sizeof(CHAR16);
        CHAR16 *temp = AllocatePool(newSize);
        if(!temp) { CPrint(Error,L"Allocation error\n"); Exit(EFI_ABORTED,0,NULL);}
        StrCpy(temp, WorkingDir);
        if(WorkingDir[WorkingDirSize-1] != L'\\') StrCat(temp, L"\\");
        StrCat(temp, Path);

        FreePool(WorkingDir);
        WorkingDir = temp;
        WorkingDirSize = StrLen(WorkingDir);
    }
}

EFI_STATUS CMDcd(CHAR16 *Args)
{
    if(!Args){
        CPrint(Info,L"Usage : cd <folder>\n");
        return EFI_INVALID_PARAMETER;
    }
    EFI_FILE_PROTOCOL *temp;
    EFI_STATUS status = uefi_call_wrapper(ActualDir->Open,5,ActualDir,&temp,Args,EFI_FILE_MODE_READ,0);
    if(status == EFI_NOT_FOUND){
        CPrint(Error,L"Folder %s not found\n",Args);
        return status;
    }
    if(status == EFI_SUCCESS){
        UINTN BufferSize = 1024;
        EFI_FILE_INFO *FileInfo;
        uefi_call_wrapper(gBS->AllocatePool,3,EfiLoaderData,BufferSize,(VOID**)&FileInfo);
        uefi_call_wrapper(temp->GetInfo,4,temp,&gEfiFileInfoGuid,&BufferSize,FileInfo);
        if (FileInfo->Attribute & EFI_FILE_DIRECTORY) {
            if(*Args==L'\\'){
                void* newDir = AllocatePool(sizeof(CHAR16)*(StrLen(Args)+1));
                if(!newDir)return EFI_ABORTED;
                StrCpy(newDir,Args);
                FreePool(WorkingDir);
                WorkingDir = newDir;
                WorkingDirSize = StrLen(newDir);
            }
            uefi_call_wrapper(ActualDir->Close, 1, ActualDir);
            ActualDir = temp;
            if(*Args!=L'\\')UpdateDir(Args);
            FreePool(FileInfo);
        } else {
            CPrint(Error,L"Folder %s not found\n",Args);
            FreePool(FileInfo);
            temp->Close(temp);
            return EFI_NOT_FOUND;
        }
    }
    return status;
}

EFI_STATUS CMDpwd(CHAR16 *Args){
    CPrint(Info,L"%s\n",WorkingDir);
    return EFI_SUCCESS;
}

EFI_STATUS CMDmkdir(CHAR16 *Args){
    if(!Args){
        CPrint(Info,L"Usage : mkdir <folder>\n");
        return EFI_INVALID_PARAMETER;
    }
    EFI_FILE_PROTOCOL *temp;
    EFI_STATUS status = uefi_call_wrapper(ActualDir->Open,5,ActualDir,&temp,Args,EFI_FILE_MODE_READ,0);
    if(status == EFI_SUCCESS){
        CPrint(Warning,L"%s already exist \n",Args);
        uefi_call_wrapper(temp->Close,1,temp);
        return EFI_ABORTED;
    }
    else if(status==EFI_NOT_FOUND){
        status = uefi_call_wrapper(ActualDir->Open,5,ActualDir,&temp,Args,EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,EFI_FILE_DIRECTORY);
        if(EFI_ERROR(status))return status;
        CPrint(Info,L"%s created successfuly !\n",Args);
        uefi_call_wrapper(temp->Close,1,temp);
        
    }else Print(L"Error : %u",status);

    return EFI_SUCCESS;
}

EFI_STATUS CMDrm(CHAR16 *Args)
{
    if(!Args){
        CPrint(Info,L"Usage : rm <file/folder>\n");
        return EFI_INVALID_PARAMETER;
    }
    EFI_FILE_PROTOCOL *temp;
    EFI_STATUS status = uefi_call_wrapper(ActualDir->Open,5,ActualDir,&temp,Args,EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,0);
    if(status == EFI_NOT_FOUND){
        CPrint(Error,L"%s not found\n",Args);
        return status;
    }
    if(status == EFI_SUCCESS){
        status = uefi_call_wrapper(temp->Delete,1,temp);
        if(status == EFI_WARN_DELETE_FAILURE){
            CPrint(Warning,L"Failed to delete %s\n",Args);
            return status;
        }
        
    }
    return EFI_SUCCESS;
}

void Init(EFI_HANDLE ImageHandle){
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    uefi_call_wrapper(gBS->HandleProtocol,3,ImageHandle,&gEfiLoadedImageProtocolGuid,(VOID**)&LoadedImage);
    uefi_call_wrapper(gBS->HandleProtocol,3,LoadedImage->DeviceHandle,&gEfiSimpleFileSystemProtocolGuid,(VOID**)&fs);
    uefi_call_wrapper(fs->OpenVolume,2,fs, &ActualDir);
    WorkingDirSize = 1;
    WorkingDir = AllocatePool((WorkingDirSize+1)*sizeof(CHAR16));
    StrCpy(WorkingDir, L"\\");
}
