#include <efi.h>
#include <efilib.h>
#include <math.h>
#include "cmd.h"
#include "func.h"
#include "display.h"
#include "memory.h"
#include "vars.h"
COMMAND Commands[] = {
    {L"help",CMDhelp,L"General help"},
    {L"power",CMDpower,L"Reboot or shutdown"},
    {L"time",CMDtime,L"Get date and time"},
    {L"clear",CMDclear,L"Clear the screen"},
    {L"exc",CMDexc,L"Cause an exception. debugging purpose, please not call"},
    {L"ls",CMDls,L"List directory content"},
    {L"dir",CMDls,L"Alias for ls"},
    {L"cd",CMDcd,L"Change working dir"},
    {L"pwd",CMDpwd,L"Print working dir"},
    {L"mkdir",CMDmkdir,L"Create directory"},
    {L"rm",CMDrm,L"Remove file or directory (if empty)"},
    {L"cp",CMDcp,L"Copy file"},
    {L"cat",CMDcat,L"Print file's content"},
    {L"nano",CMDnano,L"Edit text files"},
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
EFI_FILE_PROTOCOL *ActualDir;
CHAR16* WorkingDir = NULL;
UINTN WorkingDirSize;
UINTN ModeCount=(UINTN)(-1);


CHAR16* GetPrompt(){
    CHAR16* Buffer = NULL;
    Buffer = kmalloc((WorkingDirSize+6)*sizeof(CHAR16));
    UnicodeSPrint(Buffer,(WorkingDirSize + 6) * sizeof(CHAR16),L"fs%u:%s>",ActualVolume,WorkingDir);
    return Buffer;
}

EFI_STATUS CMDpower(UINTN argc, CHAR16** argv){
    if (argc < 2 || !StrCmp(argv[1],L"help") ){CPrint(THEME_INFO,L"Usage : power off|reset\n");return EFI_INVALID_PARAMETER;}
    else if(!StrCmp(argv[1],L"off")) {uefi_call_wrapper(RT->ResetSystem,4,EfiResetShutdown,EFI_SUCCESS,0,NULL);}

    else if(!StrCmp(argv[1],L"reset")) {uefi_call_wrapper(RT->ResetSystem,4,EfiResetWarm,EFI_SUCCESS,0,NULL);}
    else {CPrint(THEME_ERROR,L"Unknown parameter : %s",argv[1]);return EFI_INVALID_PARAMETER;}
    return EFI_SUCCESS;

}

EFI_STATUS CMDtime(UINTN argc, CHAR16** argv){
    EFI_TIME ActualTime;
    uefi_call_wrapper(RT->GetTime,2,&ActualTime,NULL);
    CPrint(THEME_INFO,L"Date : %u/%u/%u \n",ActualTime.Year,ActualTime.Month,ActualTime.Day);
    CPrint(THEME_INFO,L"Time : %02u:%02u:%02u\n",ActualTime.Hour,ActualTime.Minute,ActualTime.Second);
    if(ActualTime.TimeZone == EFI_UNSPECIFIED_TIMEZONE)
        CPrint(THEME_INFO,L"Timezone unspecified\n");
    else
        CPrint(THEME_INFO,L"Timezone : UTC%+d:%02d\n",ActualTime.TimeZone/60,ActualTime.TimeZone < 0 ? -(ActualTime.TimeZone % 60) : ActualTime.TimeZone % 60);
    return EFI_SUCCESS;
}

EFI_STATUS CMDhelp(UINTN argc, CHAR16** argv){
    if(argc == 2){
        INTN CMDIndex = -1;
        for(UINTN i = 0; i < CMD_COUNT; i++){
            if(!StrCmp(argv[1],Commands[i].name))
                CMDIndex = i;
        }
        if(CMDIndex == -1){
            CPrint(THEME_ERROR,L"Command \"%s\" not found !\n",argv[1]);
            return EFI_INVALID_PARAMETER;
        }
        CPrint(THEME_INFO,L"%s - %s\n",Commands[CMDIndex].name,Commands[CMDIndex].description);
        return EFI_SUCCESS;
    }


    UINTN index = 0;
    CPrintWait(TRUE);
    for (UINTN i = 0; i < CMD_COUNT; i++) {
        CPrint(THEME_INFO, L"%s - %s\n" , Commands[i].name, Commands[i].description);
    }
    CPrintWait(FALSE);
    return EFI_SUCCESS;
}

EFI_STATUS CMDclear(UINTN argc, CHAR16** argv){
    FillDisplay(0);
    SetCursor(0,0);
    return EFI_SUCCESS;
}

EFI_STATUS CMDexc(UINTN argc, CHAR16** argv) {
    if (argc < 2 || StrLen(argv[1]) == 0) {
        CPrint(THEME_INFO, L"Usage : exc <vector> (0-31)\n");
        return EFI_INVALID_PARAMETER;
    }
    UINTN vector = 0;
    for (UINTN i = 0; argv[1][i] != L'\0'; i++) {
        if (argv[1][i] < L'0' || argv[1][i] > L'9') {
            CPrint(THEME_ERROR, L"Erreur : l'argument doit être un nombre\n");
            return EFI_INVALID_PARAMETER;
        }
        vector = vector * 10 + (argv[1][i] - L'0');
    }

    if (vector > 31) {
        CPrint(THEME_ERROR, L"Erreur : vecteur hors limites (0-31)\n");
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

EFI_STATUS CMDls(UINTN argc, CHAR16** argv)
{
    if(!InBS){
        CPrint(THEME_WARNING,L"Not available anymore !\n");
        return EFI_ABORTED;
    }
    uefi_call_wrapper(ActualDir->SetPosition,2,ActualDir,0);
    UINTN Size = 1024;
    void* buffer = kmalloc(Size);
    CPrintWait(TRUE);
    BOOLEAN IsSomething = FALSE;
    while(1){
        Size = 1024;
        uefi_call_wrapper(ActualDir->Read,3,ActualDir,&Size,buffer);
        if(Size==0)break;
        EFI_FILE_INFO *info = (EFI_FILE_INFO*)buffer;
        CHAR16* name = info->FileName;
        if(!StrCmp(name,L".") || !StrCmp(name,L"..") ) continue;
        if(info->Attribute & EFI_FILE_DIRECTORY){
            CPrint(THEME_FILE_FOLDER,L"%s    ",name);
        } else {
            CPrint(THEME_FILE_FILE,L"%s    ",name);
        }
        IsSomething=TRUE; 

    }
    if(!IsSomething)CPrint(THEME_INFO,L"Empty directory!");
    CPrintWait(FALSE);
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
    if(!InBS){
        CPrint(THEME_WARNING,L"Not available anymore !\n");
        return EFI_ABORTED;
    }
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
    if(!InBS){
        CPrint(THEME_WARNING,L"Not available anymore !\n");
        return EFI_ABORTED;
    }
    CPrint(THEME_INFO,L"%s\n",WorkingDir);
    return EFI_SUCCESS;
}

EFI_STATUS CMDmkdir(UINTN argc, CHAR16** argv){
    if(!InBS){
        CPrint(THEME_WARNING,L"Not available anymore !\n");
        return EFI_ABORTED;
    }
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
        
    }else CPrint(THEME_ERROR,L"Error : %u\n",status);

    return EFI_SUCCESS;
}

EFI_STATUS CMDrm(UINTN argc, CHAR16** argv){
    if(!InBS){
        CPrint(THEME_WARNING,L"Not available anymore !\n");
        return EFI_ABORTED;
    }
    if(argc < 2 || StrLen(argv[1]) == 0){
        CPrint(THEME_INFO,L"Usage : rm <file/folder>\n");
        return EFI_INVALID_PARAMETER;
    }
    for(UINTN i = 1; i < argc; i++){
        EFI_FILE_PROTOCOL *temp;
        EFI_STATUS status = uefi_call_wrapper(ActualDir->Open,5,ActualDir,&temp,argv[i],EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,0);
        if(status == EFI_NOT_FOUND){
            CPrint(THEME_ERROR,L"%s not found\n",argv[i]);
            return status;
        }
        if(status == EFI_SUCCESS){
            status = uefi_call_wrapper(temp->Delete,1,temp);
            if(status == EFI_WARN_DELETE_FAILURE){
                CPrint(THEME_WARNING,L"Failed to delete %s\n",argv[i]);
                return status;
            }
        
        }
    }
}

EFI_STATUS CMDcp(UINTN argc, CHAR16** argv){
    if(!InBS){
        CPrint(THEME_WARNING,L"Not available anymore !\n");
        return EFI_ABORTED;
    }
    EFI_STATUS status;
    if(argc < 3){
        CPrint(THEME_WARNING,L"usage : cp <src> <dest>\n");
        return EFI_INVALID_PARAMETER;
    }
    //Open file
    EFI_FILE_PROTOCOL *FileSrc = NULL;
    status = uefi_call_wrapper(ActualDir->Open,5,ActualDir,&FileSrc,argv[1],EFI_FILE_MODE_READ,0);
    if(EFI_ERROR(status)){
        CPrint(THEME_ERROR,L"Error while opening %s : %r\n",argv[1],status);
        return status;
    }
    //Get info
    UINTN InfoSizeSrc = 0;
    EFI_FILE_INFO *FileInfoSrc = NULL;
    uefi_call_wrapper(FileSrc->GetInfo, 4, FileSrc, &gEfiFileInfoGuid, &InfoSizeSrc, NULL);
    FileInfoSrc = kmalloc(InfoSizeSrc);
    if(!FileInfoSrc){
        CPrint(THEME_ERROR,L"Error while allocating buffer 1\n");
        return EFI_OUT_OF_RESOURCES;
    }
    status = uefi_call_wrapper(FileSrc->GetInfo, 4, FileSrc, &gEfiFileInfoGuid, &InfoSizeSrc, FileInfoSrc);
    if(EFI_ERROR(status)){
        CPrint(THEME_ERROR,L"Error while fetching %s info : %r\n",argv[1],status);
        kfree(FileInfoSrc);
        uefi_call_wrapper(FileSrc->Close,1,FileSrc);
    }
    //Allocate buffer for data
    UINTN FileSize = FileInfoSrc->FileSize;
    kfree(FileInfoSrc);
    void* buffer = kmalloc(FileSize);
    if(!buffer){
        CPrint(THEME_ERROR,L"Error while allocating buffer 2\n");
        uefi_call_wrapper(FileSrc->Close,1,FileSrc);
        return EFI_OUT_OF_RESOURCES;
    }
    //Read
    status = uefi_call_wrapper(FileSrc->Read,3,FileSrc,&FileSize,buffer);
    if(EFI_ERROR(status)){
        CPrint(THEME_ERROR,L"Error while opening %s : %r\n",argv[1],status);
        uefi_call_wrapper(FileSrc->Close,1,FileSrc);
        kfree(buffer);
        return status;
    }
    //Create new file
    EFI_FILE_PROTOCOL *FileDest = NULL;
    status = uefi_call_wrapper(ActualDir->Open,5,ActualDir,&FileDest,argv[2],EFI_FILE_MODE_READ|EFI_FILE_MODE_CREATE|EFI_FILE_MODE_WRITE,EFI_FILE_ARCHIVE);
    if(EFI_ERROR(status)){
        CPrint(THEME_ERROR,L"Error while creating %s : %r\n",argv[2],status);
        kfree(buffer);
        uefi_call_wrapper(FileSrc->Close,1,FileSrc);
        return status;
    }
    //Set info
    UINTN InfoSizeDest=0;
    uefi_call_wrapper(FileDest->GetInfo, 4, FileDest, &gEfiFileInfoGuid, &InfoSizeDest, NULL);
    EFI_FILE_INFO *FileInfoDest = NULL;
    FileInfoDest = kmalloc(InfoSizeDest);
    if(!InfoSizeDest){
        CPrint(THEME_ERROR,L"Error while allocating buffer 3\n");
        return EFI_OUT_OF_RESOURCES;
    }
    uefi_call_wrapper(FileDest->GetInfo, 4, FileDest, &gEfiFileInfoGuid, &InfoSizeDest, FileInfoDest);
    FileInfoDest->FileSize=FileSize;
    status = uefi_call_wrapper(FileDest->SetInfo, 4, FileDest ,&gEfiFileInfoGuid, &InfoSizeDest, FileInfoDest);
    kfree(FileInfoDest);
    if(EFI_ERROR(status)){
        CPrint(THEME_ERROR,L"Error while setting %s info : %r\n",argv[2],status);
    }
    //Write
    status = uefi_call_wrapper(FileDest->Write,3,FileDest,&FileSize,buffer);
    kfree(buffer);
    if(EFI_ERROR(status)){
        CPrint(THEME_ERROR,L"Error while writing %s : %r\n",argv[2],status);
        uefi_call_wrapper(FileSrc->Close,1,FileSrc);
        uefi_call_wrapper(FileDest->Close,1,FileDest);
        return status;
    }
    //Flush
    status = uefi_call_wrapper(FileDest->Flush,1,FileDest);
    if(EFI_ERROR(status)){
        CPrint(THEME_ERROR,L"Error while flushing %s : %r\n",argv[2],status);
        uefi_call_wrapper(FileSrc->Close,1,FileSrc);
        uefi_call_wrapper(FileDest->Close,1,FileDest);
        return status;
    }
    CPrint(THEME_INFO,L"%s copied successfuly !\n",argv[1]);
    uefi_call_wrapper(FileSrc->Close,1,FileSrc);
    uefi_call_wrapper(FileDest->Close,1,FileDest);
    return EFI_SUCCESS;
}

EFI_STATUS CMDcat(UINTN argc, CHAR16** argv) {
    if(!InBS){
        CPrint(THEME_WARNING,L"Not available anymore !\n");
        return EFI_ABORTED;
    }
    if (argc < 2 || StrLen(argv[1]) == 0) {
        CPrint(THEME_WARNING,L"Usage: cat <filename>\n");
        return EFI_INVALID_PARAMETER;
    }

    EFI_FILE_PROTOCOL *File = NULL;
    EFI_STATUS status;

    status = uefi_call_wrapper(ActualDir->Open, 5, ActualDir, &File, argv[1], EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        CPrint(THEME_ERROR,L"Error: Could not open file %s (%r)\n", argv[1], status);
        return status;
    }
    UINTN InfoSize = 0;
    EFI_FILE_INFO *FileInfo = NULL;
    uefi_call_wrapper(File->GetInfo, 4, File, &gEfiFileInfoGuid, &InfoSize, NULL);
    FileInfo = kmalloc(InfoSize);
    status = uefi_call_wrapper(File->GetInfo, 4, File, &gEfiFileInfoGuid, &InfoSize, FileInfo);
    if (EFI_ERROR(status)) {
        CPrint(THEME_ERROR,L"Error: Could not get file info\n");
        kfree(FileInfo);
        uefi_call_wrapper(File->Close, 1, File);
        return status;
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
    status = uefi_call_wrapper(File->Read, 3, File, &FileSize, RawBuffer);

    if (!EFI_ERROR(status)) {
        CHAR16* WideBuffer = kmalloc((FileSize + 1) * sizeof(CHAR16));
        
        for (UINTN i = 0; i < FileSize; i++) {
            WideBuffer[i] = (CHAR16)RawBuffer[i];
        }
        WideBuffer[FileSize] = L'\0';

        CPrint(THEME_INFO,L"%s\n", WideBuffer);
        kfree(WideBuffer);
    }

    kfree(RawBuffer);
    uefi_call_wrapper(File->Close, 1, File);

    return status;
}

EFI_STATUS CMDmap(UINTN argc, CHAR16** argv){
    if(!InBS){
        CPrint(THEME_WARNING,L"Not available anymore !\n");
        return EFI_ABORTED;
    }
    EFI_STATUS status;
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
        status = uefi_call_wrapper(Root->GetInfo,4,Root,&gEfiFileSystemInfoGuid,&Size,NULL);
        EFI_FILE_SYSTEM_INFO *info = kmalloc(Size);
        uefi_call_wrapper(Root->GetInfo,4,Root,&gEfiFileSystemInfoGuid,&Size,info);
        StrCpy(Volumes[i].Label, info->VolumeLabel);
        
        kfree(info);
    }
    for(UINTN i = 0; i < VolumesCount; i++)
        CPrint(THEME_INFO,L"fs%u: %s\n",i,Volumes[i].Label);
    return EFI_SUCCESS;

}

EFI_STATUS CMDnano(UINTN argc, CHAR16** argv){
    if(!InBS){
        CPrint(THEME_WARNING,L"Not available anymore !\n");
        return EFI_ABORTED;
    }
    if(argc<2){
        CPrint(THEME_WARNING,L"usage : nano <file>\n");
        return EFI_INVALID_PARAMETER;
    }
    EFI_FILE_PROTOCOL *File = NULL;
    EFI_STATUS status;
    status = uefi_call_wrapper(ActualDir->Open,5,ActualDir,&File,argv[1],EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,0);
    if(EFI_ERROR(status)){
        CPrint(THEME_ERROR,L"Error while opening %s : %r\n",argv[1],status);
        return status;
    }
    UINTN InfoSize = 0;
    EFI_FILE_INFO *FileInfo = NULL;
    uefi_call_wrapper(File->GetInfo, 4, File, &gEfiFileInfoGuid, &InfoSize, NULL);
    FileInfo = kmalloc(InfoSize);
    if(!FileInfo)return EFI_OUT_OF_RESOURCES;
    status = uefi_call_wrapper(File->GetInfo, 4, File, &gEfiFileInfoGuid, &InfoSize, FileInfo);
    if(EFI_ERROR(status)){
        CPrint(THEME_ERROR,L"Error : can't read file info\n");
        kfree(FileInfo);
        uefi_call_wrapper(File->Close,1,File);
        return status;
    }
    if(FileInfo->Attribute & EFI_FILE_DIRECTORY){
        CPrint(THEME_ERROR,L"Error : %s is a directory\n",argv[1]);
        kfree(FileInfo);
        uefi_call_wrapper(File->Close,1,File);
        return EFI_INVALID_PARAMETER;
    }
    CHAR16* buffer = kmalloc(65536);
    if(!buffer)return EFI_OUT_OF_RESOURCES;
    
    
    UINTN FileSize = FileInfo->FileSize;
    if (FileSize>65536){
        CPrint(THEME_ERROR,L"File too big (%u). Maximum : 65536 bytes\n",FileSize);
        return EFI_ABORTED;
    }
    if(FileSize != 0){
        CHAR8* tbuffer = kmalloc(FileSize);
        status = uefi_call_wrapper(File->Read,3,File,&FileSize,tbuffer);
        if(EFI_ERROR(status)){
            CPrint(THEME_ERROR,L"Error while reading content : %r\n",status);
            return status;
        }
        for(UINTN i = 0; i< FileSize; i++){
            buffer[i]=tbuffer[i];
        }
        buffer[FileSize]=L'\0';
    }
    
    CMDclear(0,NULL);
    CPrint(THEME_INFO,L"NANO : %s - \"f1\" to save and exit; \"esc\" to discard\n",argv[1]);
    UINTN pos = 0;
    if(FileSize!=0){
        CPrint(THEME_INFO, buffer);
        pos = StrLen(buffer);
    }
    uefi_call_wrapper(File->SetPosition, 2, File, 0);
    while(1){
        EFI_INPUT_KEY Key = WaitForInput();
        if (Key.UnicodeChar == L'\b' && pos > 0) {
            UINTN X, Y;
            GetCursor(&X, &Y);
            if (X > 0) {
                buffer[pos]=L'\0';
                pos--;
                CPrint(THEME_INFO, L"\b \b");
            } else if (Y > 0) {
                if (pos >= 2 ) {
                    pos -= 2;
                    UINTN LastLineLen = 0;
                    for (INTN i = (INTN)pos - 1; i >= 0; i--) {
                        if (buffer[i] == L'\n') break;
                        LastLineLen++;
                    }
                    SetCursor(LastLineLen+1, Y - 1);
                    pos++;
                    CPrint(THEME_INFO,L" \b");
                }  
            }
        }
        else if (Key.UnicodeChar == L'\n' || Key.UnicodeChar == L'\r'){
            buffer[pos++]=L'\n';
            CPrint(THEME_INFO,L"\n");
        }
        else if (pos < 65536 && Key.UnicodeChar >= ' '){
            buffer[pos++]=Key.UnicodeChar;
            CPrint(THEME_INFO,L"%c",Key.UnicodeChar);
        } else if (Key.ScanCode == 0x17){//Abort
            uefi_call_wrapper(File->Close,1,File);
            buffer[pos] = L'\0';
            kfree(buffer);
            CPrint(THEME_WARNING,L"Aborted\n");
            return EFI_ABORTED;
        } else if (Key.ScanCode == 0x0b){//Save and exit
            buffer[pos] = L'\0';
            break;
        }
    }
    CMDclear(0,NULL);
    CHAR8* char8buffer = kmalloc(pos);
    for(UINTN i = 0; i<pos; i++)
        char8buffer[i] = buffer[i];
    FileInfo->FileSize = pos;
    UINTN Size = FileInfo->Size;
    status = uefi_call_wrapper(File->SetInfo,4,File,&gEfiFileInfoGuid,&Size,FileInfo);
    
    if(EFI_ERROR(status)) Print(L"%r : P %u E %u",status,Size,FileInfo->Size);
    status = uefi_call_wrapper(File->Write,3,File,&pos,char8buffer);
    kfree(char8buffer);
    if(EFI_ERROR(status)){
        CPrint(THEME_ERROR,L"Error while writing %s : %r\n",argv[1],status);
        kfree(buffer);
        kfree(FileInfo);
        return status;
    }
    status = uefi_call_wrapper(File->Flush,1,File);
    if(EFI_ERROR(status)){
        CPrint(THEME_ERROR,L"Error while flushing %s : %r\n",argv[1],status);
        kfree(buffer);
        kfree(FileInfo);
        return status;
    }
    CPrint(THEME_INFO,L"%s written successfuly!\n",argv[1]);
    uefi_call_wrapper(File->Close,1,File);
    kfree(buffer);
    kfree(FileInfo);
    return EFI_SUCCESS;
}

EFI_STATUS CMDvol(UINTN argc, CHAR16** argv){
    if(!InBS){
        CPrint(THEME_WARNING,L"Not available anymore !\n");
        return EFI_ABORTED;
    }
    if(VolumesCount==0){
        CPrint(THEME_ERROR,L"Either no volumes or not mapped - use \"map\"\n");
        return EFI_NOT_READY;
    }

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
    return EFI_SUCCESS;
}

EFI_STATUS CMDcheckargs(UINTN argc, CHAR16** argv){
    CPrint(THEME_INFO,L"Argument count : %u\n",argc);
    for(UINTN i = 0; i<argc; i++){
        CPrint(THEME_INFO,L"Argument %u : %s\n",i,argv[i]);
    }
    return EFI_SUCCESS;
}

EFI_STATUS CMDconfig(UINTN argc, CHAR16** argv){
    if(!GopInfo){
        CPrint(THEME_ERROR,L"Error : GOP info not available\n");
        return EFI_NOT_READY;
    }
    
    CPrint(THEME_INFO,L"Horizontal resolution : %u\n",GopInfo->HorizontalResolution);
    CPrint(THEME_INFO,L"Vertical resolution   : %u\n",GopInfo->VerticalResolution);
    CPrint(THEME_INFO,L"Scanline size         : %u\n",GopInfo->PixelsPerScanLine);
}

EFI_STATUS CMDlistres(UINTN argc, CHAR16** argv){
    if(!InBS){
        CPrint(THEME_WARNING,L"Not available anymore !\n");
        return EFI_ABORTED;
    }
    GOP_MODE_LIST* liste = GetModeList(&ModeCount);
    CPrintWait(TRUE);
    for(UINTN i = 0; i<ModeCount; i++){
        if (liste[i].SizeX==(UINTN)(-1)){
            CPrint(THEME_WARNING,L"Mode %u not available\n");
            continue;
        }
        CPrint(THEME_INFO,L"Mode %u : %ux%u\n",i,liste[i].SizeX,liste[i].SizeY);
    }
    kfree(liste);
    CPrintWait(FALSE);
    return EFI_SUCCESS;
}

EFI_STATUS CMDsetres(UINTN argc, CHAR16** argv){
    if(!InBS){
        CPrint(THEME_WARNING,L"Not available anymore !\n");
        return EFI_ABORTED;
    }
    if(ModeCount==(UINTN)(-1)){
        CPrint(THEME_WARNING,L"Resolution not enumerated - please use reslist\n");
        return EFI_NOT_READY;
    }
    UINTN ID = 0;
    if(argc < 2){
        CPrint(THEME_WARNING,L"usage : setres <ID> (ID is always an integrer)\n");
        return EFI_INVALID_PARAMETER;
    }
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
