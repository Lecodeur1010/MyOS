#include "cmd.h"
#include "func.h"
#include "display.h"
#include <efi.h>
#include <efilib.h>
#include <math.h>
COMMAND Commands[] = {
    {L"help",CMDhelp,L"General help"},
    {L"power",CMDpower,L"Reboot or shutdown"},
    {L"time",CMDtime,L"Get date and time"},
    {L"clear",CMDclear,L"Clear the screen"},
    {L"exit",CMDexit,L"Exit"},
    {L"exc",CMDexc,L"Cause an exception. debugging purpose, please not call"},
    {L"ls",CMDls,L"List directory content"},
    {L"dir",CMDls,L"Alias for ls"},
    {L"cd",CMDcd,L"Change working dir"},
    {L"pwd",CMDpwd,L"Print working dir"},
    {L"mkdir",CMDmkdir,L"Create directory"},
    {L"rm",CMDrm,L"Remove file or directory (if empty)"},
    {L"cat",CMDcat,L"Print file's content"},
    {L"map",CMDmap,L"Map volumes"},
    {L"vol",CMDvol,L"Change working volume volumes"},
    {L"test",CMDtest,L"Test the screen"},
    {L"checkargs",CMDcheckargs,L"Check arguments parsing (for debug purpose)"},
    {L"config",CMDconfig,L"Get current screen configuration"},
    {L"listres",CMDlistres,L"Get resolution list"},
    {L"setres",CMDsetres,L"Set resolution based on a mode ID"},
};

UINTN CMD_COUNT = sizeof(Commands) / sizeof(COMMAND);

VOLUME *Volumes = NULL;
UINTN VolumesCount = 0;
UINTN ActualVolume = 0;



EFI_STATUS CMDpower(UINTN argc, CHAR16** argv){
    if (!argc || !StrCmp(argv[1],L"help") ){CPrint(THEME_INFO,L"Usage : power off|reset\n");return EFI_INVALID_PARAMETER;}
    else if(!StrCmp(argv[1],L"off")) {uefi_call_wrapper(RT->ResetSystem,4,EfiResetShutdown,EFI_SUCCESS,0,NULL);}
    else if(!StrCmp(argv[1],L"reset")) {uefi_call_wrapper(RT->ResetSystem,4,EfiResetWarm,EFI_SUCCESS,0,NULL);}
    else {CPrint(THEME_ERROR,L"Unknown parameter : %s",argv[1]);return EFI_INVALID_PARAMETER;}
    return EFI_SUCCESS;

}

EFI_STATUS CMDtime(UINTN argc, CHAR16** argv){
    EFI_TIME ActualTime;
    uefi_call_wrapper(gST->RuntimeServices->GetTime,2,&ActualTime,NULL);
    CPrint(THEME_INFO,L"Date : %u/%u/%u \n",ActualTime.Year,ActualTime.Month,ActualTime.Day);
    CPrint(THEME_INFO,L"Time : %02u:%02u:%02u\n",ActualTime.Hour,ActualTime.Minute,ActualTime.Second);
    if(ActualTime.TimeZone == EFI_UNSPECIFIED_TIMEZONE)
        CPrint(THEME_INFO,L"Timezone unspecified\n");
    else
        CPrint(THEME_INFO,L"Timezone : UTC%+d:%02d",ActualTime.TimeZone/60,ActualTime.TimeZone < 0 ? -(ActualTime.TimeZone % 60) : ActualTime.TimeZone % 60);
    return EFI_SUCCESS;
}

EFI_STATUS CMDhelp(UINTN argc, CHAR16** argv){
    UINTN size = 0;
    for (UINTN i = 0; i < CMD_COUNT; i++) {
        size += (StrLen(Commands[i].name) + StrLen(Commands[i].description) + 4) * sizeof(CHAR16);
    }
    CHAR16* buffer = kmalloc(size);
    if (buffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }
    UINTN index = 0;
    for (UINTN i = 0; i < CMD_COUNT; i++) {
        index += UnicodeSPrint(buffer + index, size - index, L"%s - %s\n", Commands[i].name, Commands[i].description);
    }
    CPrint(THEME_INFO,buffer);
    kfree(buffer);
    return EFI_SUCCESS;
}

EFI_STATUS CMDclear(UINTN argc, CHAR16** argv){
    FillDisplay(0);
    SetCursor(0,0);
    return EFI_SUCCESS;
}

EFI_STATUS CMDexit(UINTN argc, CHAR16** argv){
    Exit(EFI_SUCCESS,0,NULL);
}

EFI_STATUS CMDexc(UINTN argc, CHAR16** argv) {
    if (argc < 2 || StrLen(argv[1]) == 0) {
        CPrint(THEME_INFO, L"Usage : exc <vector> (0-31)\n");
        return EFI_INVALID_PARAMETER;
    }

    // 1. Conversion simple String vers Uint8 (Atoui manuel)
    UINTN vector = 0;
    for (UINTN i = 0; argv[1][i] != L'\0'; i++) {
        if (argv[1][i] < L'0' || argv[1][i] > L'9') {
            CPrint(THEME_ERROR, L"Erreur : l'argument doit être un nombre.\n");
            return EFI_INVALID_PARAMETER;
        }
        vector = vector * 10 + (argv[1][i] - L'0');
    }

    if (vector > 31) {
        CPrint(THEME_ERROR, L"Erreur : vecteur hors limites (0-31).\n");
        return EFI_INVALID_PARAMETER;
    }


    static uint8_t code[3];
    code[0] = 0xCD;           // Opcode INT
    code[1] = (uint8_t)vector; 
    code[2] = 0xC3;           // Opcode RET

    // 3. Appel
    void (*dispatch)() = (void(*)())code;
    dispatch();

    return EFI_SUCCESS;
}

EFI_FILE_PROTOCOL *ActualDir;
CHAR16* WorkingDir = NULL;
UINTN WorkingDirSize;

CHAR16* GetPrompt(){
    CHAR16* Buffer = NULL;
    Buffer = kmalloc((WorkingDirSize+6)*sizeof(CHAR16));
    UnicodeSPrint(Buffer,(WorkingDirSize + 6) * sizeof(CHAR16),L"fs%u:%s>",ActualVolume,WorkingDir);
    return Buffer;
}

EFI_STATUS CMDls(UINTN argc, CHAR16** argv)
{
    uefi_call_wrapper(ActualDir->SetPosition,2,ActualDir,0);
    UINTN Size = 1024;
    void* buffer = kmalloc(Size);
    while(1){
        Size = 1024;
        uefi_call_wrapper(ActualDir->Read,3,ActualDir,&Size,buffer);
        if(Size==0){break;} 
        EFI_FILE_INFO *info = (EFI_FILE_INFO*)buffer;
        CHAR16* name = info->FileName;
        if(info->Attribute & EFI_FILE_DIRECTORY){
            CPrint(THEME_FILE_FILE,L"%s    ",name);
        } else {
            CPrint(THEME_FILE_FOLDER,L"%s    ",name);
        }

    }
    CPrint(THEME_INFO,L"\n");
    kfree(buffer);
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
        CHAR16 *temp = kmalloc(newSize);
        if(!temp) { CPrint(THEME_ERROR,L"Allocation error\n"); Exit(EFI_ABORTED,0,NULL);}
        StrCpy(temp, WorkingDir);
        if(WorkingDir[WorkingDirSize-1] != L'\\') StrCat(temp, L"\\");
        StrCat(temp, Path);

        kfree(WorkingDir);
        WorkingDir = temp;
        WorkingDirSize = StrLen(WorkingDir);
    }
}

EFI_STATUS CMDcd(UINTN argc, CHAR16** argv){
    if(argc < 2 || StrLen(argv[1]) == 0){
        CPrint(THEME_INFO,L"Usage : cd <folder>\n");
        return EFI_INVALID_PARAMETER;
    }
    EFI_FILE_PROTOCOL *temp;
    EFI_STATUS status = uefi_call_wrapper(ActualDir->Open,5,ActualDir,&temp,argv[1],EFI_FILE_MODE_READ,0);
    if(status == EFI_NOT_FOUND){
        CPrint(THEME_ERROR,L"Folder %s not found\n",argv[1]);
        return status;
    }
    if(status == EFI_SUCCESS){
        UINTN BufferSize = 1024;
        EFI_FILE_INFO *FileInfo = (EFI_FILE_INFO*)kmalloc(BufferSize);
        uefi_call_wrapper(temp->GetInfo,4,temp,&gEfiFileInfoGuid,&BufferSize,FileInfo);
        if (FileInfo->Attribute & EFI_FILE_DIRECTORY) {
            if(*argv[1]==L'\\'){
                void* newDir = kmalloc(sizeof(CHAR16)*(StrLen(argv[1])+1));
                if(!newDir)return EFI_ABORTED;
                StrCpy(newDir,argv[1]);
                kfree(WorkingDir);
                WorkingDir = newDir;
                WorkingDirSize = StrLen(newDir);
            }
            uefi_call_wrapper(ActualDir->Close, 1, ActualDir);
            ActualDir = temp;
            if(*argv[1]!=L'\\')UpdateDir(argv[1]);
            kfree(FileInfo);
        } else {
            CPrint(THEME_ERROR,L"Folder %s not found\n",argv[1]);
            kfree(FileInfo);
            temp->Close(temp);
            return EFI_NOT_FOUND;
        }
    }
    return status;
}

EFI_STATUS CMDpwd(UINTN argc, CHAR16** argv){
    CPrint(THEME_INFO,L"%s\n",WorkingDir);
    return EFI_SUCCESS;
}

EFI_STATUS CMDmkdir(UINTN argc, CHAR16** argv){
    if(argc < 2 || StrLen(argv[1]) == 0){
        CPrint(THEME_INFO,L"Usage : mkdir <folder>\n");
        return EFI_INVALID_PARAMETER;
    }
    EFI_FILE_PROTOCOL *temp;
    EFI_STATUS status = uefi_call_wrapper(ActualDir->Open,5,ActualDir,&temp,argv[1],EFI_FILE_MODE_READ,0);
    if(status == EFI_SUCCESS){
        CPrint(THEME_WARNING,L"%s already exist \n",argv[1]);
        uefi_call_wrapper(temp->Close,1,temp);
        return EFI_ABORTED;
    }
    else if(status==EFI_NOT_FOUND){
        status = uefi_call_wrapper(ActualDir->Open,5,ActualDir,&temp,argv[1],EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,EFI_FILE_DIRECTORY);
        if(EFI_ERROR(status))return status;
        CPrint(THEME_INFO,L"%s created successfuly !\n",argv[1]);
        uefi_call_wrapper(temp->Close,1,temp);
        
    }else Print(L"Error : %u",status);

    return EFI_SUCCESS;
}

EFI_STATUS CMDrm(UINTN argc, CHAR16** argv){

    if(argc < 2 || StrLen(argv[1]) == 0){
        CPrint(THEME_INFO,L"Usage : rm <file/folder>\n");
        return EFI_INVALID_PARAMETER;
    }
    EFI_FILE_PROTOCOL *temp;
    EFI_STATUS status = uefi_call_wrapper(ActualDir->Open,5,ActualDir,&temp,argv[1],EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,0);
    if(status == EFI_NOT_FOUND){
        CPrint(THEME_ERROR,L"%s not found\n",argv[1]);
        return status;
    }
    if(status == EFI_SUCCESS){
        status = uefi_call_wrapper(temp->Delete,1,temp);
        if(status == EFI_WARN_DELETE_FAILURE){
            CPrint(THEME_WARNING,L"Failed to delete %s\n",argv[1]);
            return status;
        }
        
    }
    return EFI_SUCCESS;
}

EFI_STATUS CMDcp(UINTN argc, CHAR16** argv){
    if(argc < 3){
        CPrint(THEME_WARNING,L"usage : cp <src> <dest>");
        return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
}

EFI_STATUS CMDcat(UINTN argc, CHAR16** argv) {
    if (argc < 2 || StrLen(argv[1]) == 0) {
        Print(L"Usage: cat <filename>\n");
        return EFI_INVALID_PARAMETER;
    }

    EFI_FILE_PROTOCOL *File = NULL;
    EFI_STATUS Status;

    Status = uefi_call_wrapper(ActualDir->Open, 5, ActualDir, &File, argv[1], EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        CPrint(THEME_ERROR,L"Error: Could not open file %s (%r)\n", argv[1], Status);
        return Status;
    }
    UINTN InfoSize = 0;
    EFI_FILE_INFO *FileInfo = NULL;
    uefi_call_wrapper(File->GetInfo, 4, File, &gEfiFileInfoGuid, &InfoSize, NULL);
    FileInfo = kmalloc(InfoSize);
    Status = uefi_call_wrapper(File->GetInfo, 4, File, &gEfiFileInfoGuid, &InfoSize, FileInfo);
    if (EFI_ERROR(Status)) {
        CPrint(THEME_ERROR,L"Error: Could not get file info\n");
        kfree(FileInfo);
        uefi_call_wrapper(File->Close, 1, File);
        return Status;
    }

    if (FileInfo->Attribute & EFI_FILE_DIRECTORY) {
        CPrint(THEME_ERROR,L"Error: %s is a directory\n", argv[1]);
        kfree(FileInfo);
        uefi_call_wrapper(File->Close, 1, File);
        return EFI_UNSUPPORTED;
    }

    UINTN FileSize = FileInfo->FileSize;
    kfree(FileInfo);

    CHAR8* RawBuffer = kmalloc(FileSize);
    Status = uefi_call_wrapper(File->Read, 3, File, &FileSize, RawBuffer);

    if (!EFI_ERROR(Status)) {
        CHAR16* WideBuffer = AllocateZeroPool((FileSize + 1) * sizeof(CHAR16));
        
        for (UINTN i = 0; i < FileSize; i++) {
            WideBuffer[i] = (CHAR16)RawBuffer[i];
        }

        CPrint(THEME_INFO,L"%s\n", WideBuffer);
        kfree(WideBuffer);
    }

    kfree(RawBuffer);
    uefi_call_wrapper(File->Close, 1, File);

    return Status;
}

EFI_STATUS CMDmap(UINTN argc, CHAR16** argv){
    EFI_HANDLE* HandleBuffer;
    uefi_call_wrapper(BS->LocateHandleBuffer,5,ByProtocol,&gEfiSimpleFileSystemProtocolGuid,NULL,&VolumesCount,&HandleBuffer);
    if(Volumes)kfree(Volumes);
    Volumes = kmalloc(sizeof(VOLUME)*VolumesCount);
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    EFI_FILE_PROTOCOL *Root;
    for(UINTN i = 0; i < VolumesCount; i++){
        
        uefi_call_wrapper(gBS->HandleProtocol, 3, HandleBuffer[i], &gEfiSimpleFileSystemProtocolGuid, (VOID*)&fs);
        uefi_call_wrapper(fs->OpenVolume,2,fs, &Root);

        Volumes[i].Handle = HandleBuffer[i];
        Volumes[i].Root = Root;
        UINTN Size = 1;
        uefi_call_wrapper(Root->GetInfo,4,Root,&gEfiFileSystemInfoGuid,&Size,NULL);
        EFI_FILE_SYSTEM_INFO *info = kmalloc(Size);
        uefi_call_wrapper(Root->GetInfo,4,Root,&gEfiFileSystemInfoGuid,&Size,info);
        StrCpy(Volumes[i].Label, info->VolumeLabel);
        
        kfree(info);
    }
    for(UINTN i = 0; i < VolumesCount; i++)
        CPrint(THEME_INFO,L"fs%u: %s\n",i,Volumes[i].Label);

}

EFI_STATUS CMDvol(UINTN argc, CHAR16** argv){
    if (argc < 2 || StrLen(argv[1]) == 0) {
        CPrint(THEME_INFO,L"Usage : vol <volume> (<volume> will always be fsX: format)\n");
        return EFI_INVALID_PARAMETER;
    }

    uint8_t val=255 ; 
    if(argv[1][2] >= L'0' && argv[1][2]  <= L'9')
        val = argv[1][2]  - L'0'; 
     

    if (argv[1][0]!=L'f'||argv[1][1]!=L's'||val>=VolumesCount||argv[1][3]!=L':'){
        CPrint(THEME_ERROR,L"Wrong volume identifier. Should be in fsX: format\n");
        return EFI_INVALID_PARAMETER;
    }

    ActualDir=Volumes[val].Root;
    WorkingDirSize = 1;
    StrCpy(WorkingDir,L"\\");
    ActualVolume=val;
    return EFI_SUCCESS;
}

EFI_STATUS CMDtest(UINTN argc, CHAR16** argv){
    
    CPrint(RGB(255,0,0),L"Red   : abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890□\n");
    CPrint(RGB(0,255,0),L"Green : abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890□\n");
    CPrint(RGB(0,0,255),L"Blue  : abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890□\n");
}

EFI_STATUS CMDcheckargs(UINTN argc, CHAR16** argv){
    CPrint(THEME_INFO,L"Argument count : %u\n",argc);
    for(UINTN i = 0; i<argc; i++){
        CPrint(THEME_INFO,L"Argument %u : %s\n",i,argv[i]);
    }
    return EFI_SUCCESS;
}

EFI_STATUS CMDconfig(UINTN argc, CHAR16** argv){
    CPrint(THEME_INFO,L"Horizontal resolution : %u\n",GopInfo->HorizontalResolution);
    CPrint(THEME_INFO,L"Vertical resolution   : %u\n",GopInfo->VerticalResolution);
    CPrint(THEME_INFO,L"Scanline size         : %u\n",GopInfo->PixelsPerScanLine);
}

UINTN ModeCount=(UINTN)(-1);

EFI_STATUS CMDlistres(UINTN argc, CHAR16** argv){
    GopModeList* liste = GetModeList(&ModeCount);
    for(UINTN i = 0; i<ModeCount; i++){
        if (liste[i].SizeX==(UINTN)(-1)){
            CPrint(THEME_WARNING,L"Mode %u not available\n");
            continue;
        }
        CPrint(THEME_INFO,L"Mode %u : %ux%u\n",i,liste[i].SizeX,liste[i].SizeY);
    }
    kfree(liste);
    return EFI_SUCCESS;
}

EFI_STATUS CMDsetres(UINTN argc, CHAR16** argv){
    if(ModeCount==(UINTN)(-1)){
        CPrint(THEME_WARNING,L"Resolution not enumerated - please use reslist\n");
        return EFI_NOT_READY;
    }
    UINTN ID = 0;
    while (*argv[1] != L'\0') {
        if (*argv[1] < L'0' || *argv[1] > L'9') {
            CPrint(THEME_WARNING,L"usage : setres <ID> (ID is always an integrer)\n");
            return EFI_INVALID_PARAMETER;
        }
        ID = ID * 10 + (*argv[1] - L'0');  // Conversion en entier
        argv[1]++;
    }
    if(ID>=ModeCount){
        CPrint(THEME_WARNING,L"Resolution ID out of bound\n");
        return EFI_INVALID_PARAMETER;
    }
    EFI_STATUS status = SetMode(ID);
    if(EFI_ERROR(status)) CPrint(THEME_ERROR,L"Error : %r\n",status);
    else CPrint(THEME_INFO,L"Mode %u set successfuly !\n",ID);
    return status;
    
}

void Init(EFI_HANDLE ImageHandle){
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    uefi_call_wrapper(gBS->HandleProtocol,3,ImageHandle,&gEfiLoadedImageProtocolGuid,(VOID**)&LoadedImage);
    uefi_call_wrapper(gBS->HandleProtocol,3,LoadedImage->DeviceHandle,&gEfiSimpleFileSystemProtocolGuid,(VOID**)&fs);
    uefi_call_wrapper(fs->OpenVolume,2,fs, &ActualDir);
    WorkingDirSize = 1;
    WorkingDir = kmalloc((WorkingDirSize+1)*sizeof(CHAR16));
    StrCpy(WorkingDir, L"\\");
}
