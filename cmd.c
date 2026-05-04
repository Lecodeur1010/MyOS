#include <efi.h>
#include <efilib.h>
#include <math.h>
#include "cmd.h"
#include "func.h"
#include "display.h"
#include "memory.h"
#include "vars.h"
#include "fat.h"
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
    {L"vol",CMDvol,L"Change working volume volumes"},
    {L"test",CMDtest,L"Test the screen"},
    {L"checkargs",CMDcheckargs,L"Check arguments parsing (for debug purpose)"},
    {L"config",CMDconfig,L"Get current screen configuration"},
    {L"mem",CMDmem,L"Get current screen configuration"},
};

UINTN CMD_COUNT = sizeof(Commands) / sizeof(COMMAND);

UINTN ActualPartition = 0;
CHAR16* WorkingDir = NULL;
UINTN WorkingDirSize;
PARTITION* WorkingPartition = NULL;
UINT32 WorkingCluster = 0;

EFI_FILE *ActualDir;


CHAR16* GetPrompt(){
    CHAR16* Buffer = NULL;
    Buffer = kmalloc((WorkingDirSize+6)*sizeof(CHAR16));
    UnicodeSPrint(Buffer,(WorkingDirSize + 6) * sizeof(CHAR16),L"fs%u:%s>",ActualPartition,WorkingDir);
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
    if(!WorkingPartition){
        CPrint(THEME_ERROR,L"No drive implemented. All disk-related cmd are forbidden\n");
        return EFI_NOT_FOUND;
    }
    FAT_DIR_ENTRY* dir = NULL;
    UINTN entryCount = 0;
    EFI_STATUS status = ListDir(WorkingPartition,WorkingCluster,&dir,&entryCount);
    CheckError(status);
    CPrint(THEME_INFO,L"Type Name    \n");
    CPrint(THEME_INFO,L"-----------------\n");
    for(UINTN i = 0; i < entryCount; i++){
        CHAR16 PrettyLabel[13];
        for(UINTN j = 0; j < 12; j++) PrettyLabel[j]=L' ';
        PrettyLabel[12]=L'\0';

        CHAR16* type[2] = {L"[FILE]",L"[DIR] "};
        if(!strncmpa((CONST CHAR8*)". ",dir[i].name,2))continue;
        if(!strncmpa((CONST CHAR8*)"..",dir[i].name,2))continue;
        UINTN nameLen = 0,extLen = 0,index = 0;
        for(UINTN j = 0; j < 8; j++){
            if(dir[i].name[j]!=L' ')nameLen++;
            else break;
        }
        for(UINTN j = 8; j < 11; j++){
            if(dir[i].name[j]!=L' ')extLen++;
            else break;
        }
        for(index = 0; index < nameLen; index++){
            PrettyLabel[index]=(CHAR16)dir[i].name[index];
        }
        if(extLen){PrettyLabel[index]=L'.';index++;}
        for(UINTN j = 8; j < 8+extLen; j++){
            PrettyLabel[index]=dir[i].name[j];
        }
        CPrint(THEME_INFO,L"%s %s\n",type[(dir[i].attr & 0x10) ? 1 : 0],PrettyLabel);
    }
    return EFI_SUCCESS;
}

EFI_STATUS UpdateDir(CHAR16* Path){
    if (!Path) return EFI_INVALID_PARAMETER;

    if (!StrCmp(Path,L".")) return EFI_SUCCESS;

    if (!StrCmp(Path,L"..")){
        if (WorkingDirSize <= 1) return EFI_ABORTED; 
        INTN i = WorkingDirSize - 2;
        while(i >= 0 && WorkingDir[i] != L'\\') i--;
        WorkingDir[i+1] = L'\0';
        WorkingDirSize = i + 1;
    }
    else {
        UINTN newSize = (WorkingDirSize + StrLen(Path) + 2) * sizeof(CHAR16);
        CHAR16 *temp = kmalloc(newSize);
        CheckBuffer(temp);
        StrCpy(temp, WorkingDir);
        if(WorkingDir[WorkingDirSize-1] != L'\\') StrCat(temp, L"\\");
        StrCat(temp, Path);

        kfree(WorkingDir);
        WorkingDir = temp;
        WorkingDirSize = StrLen(WorkingDir);
    }
    return EFI_SUCCESS;
}

EFI_STATUS CMDcd(UINTN argc, CHAR16** argv){
    if(argc < 2 || StrLen(argv[1]) == 0){
        CPrint(THEME_INFO,L"Usage : cd <folder>\n");
        return EFI_INVALID_PARAMETER;
    }
    if(!WorkingPartition){
        CPrint(THEME_ERROR,L"No drive implemented. All disk-related cmd are forbidden\n");
        return EFI_NOT_FOUND;
    }
    CHAR16* path = argv[1];
    if(*path==L'\\'){
        WorkingDirSize = 1;
        if(WorkingDir) kfree(WorkingDir);
        WorkingDir = kmalloc((WorkingDirSize+1)*sizeof(CHAR16));
        CheckBuffer(WorkingDir);
        StrCpy(WorkingDir, L"\\");
        path++;
    }
    CHAR16* ptr = path;
    CHAR16* nextPtr = NULL;
    while(*ptr){
        if((*ptr==L'\\' || *ptr==L'/' )){
            *ptr = L'\0';
            if((*(ptr+1) != L'\0' && *(ptr+1) != L'\\' && *(ptr+1) != L'/' )){
                nextPtr = ptr+1;
                break;
            }
        }
        ptr++;
    }
    FAT_DIR_ENTRY* dir = NULL;
    UINTN entryCount = 0;
    EFI_STATUS status = ListDir(WorkingPartition,WorkingCluster,&dir,&entryCount);
    CheckError(status);
    CHAR16 Label[12];
    BOOLEAN Found = FALSE;
    for(UINTN i = 0; i<entryCount; i++){
        if(!StriCmp(L".",path)){ Found=TRUE; break;}
        for(UINTN j = 0; j<11;j++){
            Label[j]=(CHAR16)dir[i].name[j];
        }

        Label[11]=L'\0';
        for(INTN j = 10;j>=0; j--){
            if(Label[j]!=32) break;
            else Label[j] = L'\0';
        }
        if(!StriCmp(Label,path)){
            
            if(dir[i].attr & 0x10){
                Found = TRUE;
                WorkingCluster=dir[i].first_cluster_low + (dir[i].first_cluster_high << 16);
                if(WorkingCluster==0)WorkingCluster = ((FAT32_FS*)WorkingPartition->fs)->root_dir_cluster;
                UpdateDir(Label);break;
            } else {
                CPrint(THEME_ERROR,L"%s is not a dir\n",Label);
                kfree(dir);
                return EFI_INVALID_PARAMETER;
            }
        } 
    }
    kfree(dir);
    if(!Found){
        CPrint(THEME_ERROR,L"%s doesn't exist\n",path);
        return EFI_NOT_FOUND;
    }
    CHAR16* arg[2] = { argv[0], nextPtr };
    if(!nextPtr) status = EFI_SUCCESS;
    else status = CMDcd(2,arg);
    return status;
}

EFI_STATUS CMDpwd(UINTN argc, CHAR16** argv){
    CPrint(THEME_INFO,L"%s\n",WorkingDir);
    return EFI_SUCCESS;
}

EFI_STATUS CMDmkdir(UINTN argc, CHAR16** argv){
    if(!InBS){
        CPrint(THEME_WARNING,L"Not available anymore ! (writing in FAT is a pain)\n");
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
        CheckError(status);
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
    return EFI_SUCCESS;
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
    CheckBuffer(buffer);
    //Read
    status = uefi_call_wrapper(FileSrc->Read,3,FileSrc,&FileSize,buffer);
    CheckError(status);
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
    CheckBuffer(FileInfoDest);
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
    CheckError(status);
    UINTN InfoSize = 0;
    EFI_FILE_INFO *FileInfo = NULL;
    uefi_call_wrapper(File->GetInfo, 4, File, &gEfiFileInfoGuid, &InfoSize, NULL);
    FileInfo = kmalloc(InfoSize);
    CheckBuffer(FileInfo);
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
    CheckBuffer(buffer);
    
    
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
    if(argc != 2){
        CPrint(THEME_ERROR,L"Usage : vol <volume> \n<vol> should be in fsX: format");
        return EFI_INVALID_PARAMETER;
    }
    if (argv[1][0] != L'f' || argv[1][1] != L's'){
        CPrint(THEME_ERROR,L"Usage : vol <volume> \n<vol> should be in fsX: format");
        return EFI_INVALID_PARAMETER;
    }
    
    UINTN i = 2;

    if (argv[1][i] < L'0' || argv[1][i] > L'9'){
        CPrint(THEME_ERROR,L"Usage : vol <volume> \n<vol> should be in fsX: format");
        return EFI_INVALID_PARAMETER;
    }
    uint32_t index = 0;

    while (argv[1][i] >= L'0' && argv[1][i] <= L'9') {
        index = index * 10 + ((CHAR8)argv[1][i] - L'0');
        i++;
    }

    if (argv[1][i] != L';' || argv[1][i + 1] != L'\0'){
        CPrint(THEME_ERROR,L"Usage : vol <volume> \n<vol> should be in fsX: format");
        return EFI_INVALID_PARAMETER;
    }
    if(index>PartitionCount-1){
        CPrint(THEME_ERROR,L"Partition fs%u; doesn't exist\n",index);
        return EFI_INVALID_PARAMETER;
    }
    for(UINTN i = 0; i < PartitionCount;i++){
        if(PartitionList[i].OSUID==index){
            WorkingPartition=&PartitionList[i];
            WorkingCluster=((FAT32_FS*)WorkingPartition->fs)->root_dir_cluster;
            WorkingDirSize = 1;
            ActualPartition = i;
            kfree(WorkingDir);
            WorkingDir = kmalloc((WorkingDirSize+1)*sizeof(CHAR16));
            StrCpy(WorkingDir, L"\\");
        }
        return EFI_SUCCESS;
    }
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
    return EFI_SUCCESS;
}

EFI_STATUS CMDmem(UINTN argc, CHAR16** argv){
    CHAR16 buf[64];
    CPrintWait(TRUE);
    UINTN size = sizeof(buf);
    FormatWithUnit(MemSize,buf,&size);
    CPrint(THEME_INFO,L"Memory size         : %s\n",buf);
    size = sizeof(buf);
    FormatWithUnit(UsableMemSize,buf,&size);
    CPrint(THEME_INFO,L"Total usable memory : %s\n",buf);
    size = sizeof(buf);
    FormatWithUnit(HeapSize,buf,&size);
    CPrint(THEME_INFO,L"Heap size           : %s\n",buf);
    size = sizeof(buf);
    FormatWithUnit(UsedMem,buf,&size);
    CPrint(THEME_INFO,L"Used heap           : %s\n",buf);
    CPrintWait(FALSE);
    return EFI_SUCCESS;
}

VOID Init(){
    WorkingDirSize = 1;
    WorkingDir = kmalloc((WorkingDirSize+1)*sizeof(CHAR16));
    StrCpy(WorkingDir, L"\\");
    if(PartitionCount != 0){ //Only if drives
        ActualPartition = 0;
        WorkingPartition=PartitionList;

        WorkingCluster = ((FAT32_FS*)WorkingPartition->fs)->root_dir_cluster;
    }
}
