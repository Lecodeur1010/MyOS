#include "cmd.h"
#include "func.h"
#include "display.h"
#include "disk.h"
#include "graphics.h"
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
    {L"cp",CMDcp,L"Copy file"},
    {L"nano",CMDnano,L"Edit text files"},
    {L"vol",CMDvol,L"List/ update volumes"},
    {L"test",CMDtest,L"Test the screen"},
    {L"checkargs",CMDcheckargs,L"Check arguments parsing (for debug purpose)"},
    {L"config",CMDconfig,L"Get current screen configuration"},
    {L"listres",CMDlistres,L"Get resolution list"},
    {L"setres",CMDsetres,L"Set resolution based on a mode ID"},
    {L"echo",CMDecho,L"Print the first arg if present"},
    {L"sh",CMDsh,L"Run a script file"},
    {L"img",CMDimg,L"Render a TGA image"},
    {L"loadcfg",CMDloadcfg,L"Load a configuration file"},
    {L"reloadcfg",CMDreloadcfg,L"Reload the boot configuration file"},
    {L"resetcfg",CMDresetcfg,L"Reset actual configuration to the default one"},
};

UINTN CMD_COUNT = sizeof(Commands) / sizeof(COMMAND);

EFI_STATUS CMDecho(UINTN argc, CHAR16** argv){
    if(argc<2) return EFI_SUCCESS;
    ShellPrint(ActualConfig.Theme.Info, L"%s", argv[1]);
    return EFI_SUCCESS;
}

EFI_STATUS CMDpower(UINTN argc, CHAR16** argv){
    if (!argc || !StrCmp(argv[1],L"help") ){ShellPrint(ActualConfig.Theme.Info,L"Usage : power off|reset\n");return EFI_INVALID_PARAMETER;}
    else if(!StrCmp(argv[1],L"off")) {uefi_call_wrapper(RT->ResetSystem,4,EfiResetShutdown,EFI_SUCCESS,0,NULL);}
    else if(!StrCmp(argv[1],L"reset")) {uefi_call_wrapper(RT->ResetSystem,4,EfiResetWarm,EFI_SUCCESS,0,NULL);}
    else {ShellPrint(ActualConfig.Theme.Error,L"Unknown parameter : %s\n",argv[1]);return EFI_INVALID_PARAMETER;}
    return EFI_SUCCESS;

}

EFI_STATUS CMDtime(UINTN argc, CHAR16** argv){
    EFI_TIME ActualTime;
    uefi_call_wrapper(gST->RuntimeServices->GetTime,2,&ActualTime,NULL);
    ShellPrint(ActualConfig.Theme.Info,L"Date : %u/%u/%u \n",ActualTime.Year,ActualTime.Month,ActualTime.Day);
    ShellPrint(ActualConfig.Theme.Info,L"Time : %02u:%02u:%02u\n",ActualTime.Hour,ActualTime.Minute,ActualTime.Second);
    if(ActualTime.TimeZone == EFI_UNSPECIFIED_TIMEZONE)
        ShellPrint(ActualConfig.Theme.Info,L"Timezone unspecified\n");
    else
        ShellPrint(ActualConfig.Theme.Info,L"Timezone : UTC%+d:%02d\n",ActualTime.TimeZone/60,ActualTime.TimeZone < 0 ? -(ActualTime.TimeZone % 60) : ActualTime.TimeZone % 60);
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
            ShellPrint(ActualConfig.Theme.Error,L"Command \"%s\" not found !\n",argv[1]);
            return EFI_INVALID_PARAMETER;
        }
        ShellPrint(ActualConfig.Theme.Info,L"%s - %s\n",Commands[CMDIndex].name,Commands[CMDIndex].description);
        return EFI_SUCCESS;
    }


    CPrintWait(TRUE);
    for (UINTN i = 0; i < CMD_COUNT; i++) {
        ShellPrint(ActualConfig.Theme.Info, L"%s - %s\n" , Commands[i].name, Commands[i].description);
    }
    CPrintWait(FALSE);
    return EFI_SUCCESS;
}

EFI_STATUS CMDloadcfg(UINTN argc, CHAR16** argv){
    if(argc<2){ShellPrint(ActualConfig.Theme.Info,L"Usage : loadcfg <path> \n");return EFI_INVALID_PARAMETER;}
    return LoadCFG(argv[1]);
}

EFI_STATUS CMDreloadcfg(UINTN argc, CHAR16** argv){
    CopyMem(&ActualConfig.Theme,&BackupConfig,sizeof(CONFIG));
    EFI_STATUS status = LoadCFG(L"\\mnt\\fs0\\boot.ini");
    CMDclear(0,NULL);
    return status;
}

EFI_STATUS CMDresetcfg(UINTN argc, CHAR16** argv){
    CopyMem(&ActualConfig.Theme,&BackupConfig,sizeof(CONFIG));
    return CMDclear(0,NULL);
}

EFI_STATUS CMDclear(UINTN argc, CHAR16** argv){
    FillDisplay(ActualConfig.Theme.Background);
    SetCursor(0,0);
    return EFI_SUCCESS;
}

EFI_STATUS CMDexit(UINTN argc, CHAR16** argv){
    Exit(EFI_SUCCESS,0,NULL);
    return EFI_SUCCESS;
}

EFI_STATUS CMDexc(UINTN argc, CHAR16** argv) {
    if (argc < 2 || StrLen(argv[1]) == 0) {
        ShellPrint(ActualConfig.Theme.Info, L"Usage : exc <vector> (0-31)\n");
        return EFI_INVALID_PARAMETER;
    }

    // 1. Conversion simple String vers Uint8 (Atoui manuel)
    UINTN vector = 0;
    for (UINTN i = 0; argv[1][i] != L'\0'; i++) {
        if (argv[1][i] < L'0' || argv[1][i] > L'9') {
            ShellPrint(ActualConfig.Theme.Error, L"Erreur : l'argument doit être un nombre.\n");
            return EFI_INVALID_PARAMETER;
        }
        vector = vector * 10 + (argv[1][i] - L'0');
    }

    if (vector > 31) {
        ShellPrint(ActualConfig.Theme.Error, L"Erreur : vecteur hors limites (0-31).\n");
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

EFI_STATUS GetCurrentPathString(CHAR16* OutBuffer, UINTN MaxLength) {
    if (!OutBuffer || MaxLength == 0) return EFI_INVALID_PARAMETER;

    // Buffer temporaire pour construire la chaîne à l'envers
    CHAR16 TempBuffer[256];
    UINTN Pos = 255;
    TempBuffer[Pos] = L'\0'; // Fin de chaîne

    FS_NODE* Current = ActualNode;

    // Si on est déjà à la racine
    if (Current == RootNode || Current->Parent == NULL) {
        if (MaxLength < 2) return EFI_BUFFER_TOO_SMALL;
        StrCpy(OutBuffer, L"\\");
        return EFI_SUCCESS;
    }

    // Remontée de l'arbre
    while (Current != NULL && Current != RootNode) {
        UINTN Len = StrLen(Current->Name);
        
        // Vérification de sécurité pour ne pas déborder du TempBuffer
        if (Pos < Len + 1) { 
            return EFI_OUT_OF_RESOURCES; 
        }

        // 1. Reculer le curseur et copier le nom du dossier
        Pos -= Len;
        CopyMem(&TempBuffer[Pos], Current->Name, Len * sizeof(CHAR16));

        // 2. Reculer le curseur et ajouter le séparateur '\'
        Pos--;
        TempBuffer[Pos] = L'\\';

        // 3. Monter au parent
        Current = Current->Parent;
    }

    // Vérification de la taille du buffer de sortie demandé par l'utilisateur
    UINTN FinalLen = StrLen(&TempBuffer[Pos]);
    if (FinalLen >= MaxLength) {
        return EFI_BUFFER_TOO_SMALL;
    }

    // Copie finale vers la sortie
    StrCpy(OutBuffer, &TempBuffer[Pos]);
    return EFI_SUCCESS;
}

EFI_STATUS CMDls(UINTN argc, CHAR16** argv){
    EFI_FILE_INFO** FileInfo;
    UINTN Count = 0;
    FS_NODE* Node = ActualNode;
    EFI_STATUS status;
    if(argc > 1){
        status = VFSOpen(ActualNode,argv[1],&Node,EFI_FILE_MODE_READ,0);
        if(EFI_ERROR(status)){
            ShellPrint(ActualConfig.Theme.Error,L"Error1 : %r\n",status);
            return status;
        }
    }
    if(!Node->IsDirectory){
        ShellPrint(ActualConfig.Theme.Error,L"%s is a file\n",argv[1]);
        Node->Close(Node);
        return EFI_UNSUPPORTED;
    }
    status = Node->List(Node,&FileInfo,&Count);
    if(EFI_ERROR(status)){
        ShellPrint(ActualConfig.Theme.Error,L"Error : %r\n",status);
        return status;
    }
    for(UINTN i = 0; i<Count; i++){
        UINT32 color = FileInfo[i]->Attribute&EFI_FILE_DIRECTORY ? ActualConfig.Theme.Folder : ActualConfig.Theme.File;
        ShellPrint(color,L"%s   ",FileInfo[i]->FileName);
        kfree(FileInfo[i]);
    }
    if(Node!=ActualNode)Node->Close(Node);
    ShellPrint(ActualConfig.Theme.Info,L"\n");
    kfree(FileInfo);
    return EFI_SUCCESS;
}

EFI_STATUS CMDcd(UINTN argc, CHAR16** argv){
    if(argc < 2 || StrLen(argv[1]) == 0){
        ShellPrint(ActualConfig.Theme.Info,L"Usage : cd <folder>\n");
        return EFI_INVALID_PARAMETER;
    }
    FS_NODE* Node;
    
    EFI_STATUS status = VFSOpen(ActualNode,argv[1],&Node,EFI_FILE_MODE_READ,0);
    if(EFI_ERROR(status)){
        ShellPrint(ActualConfig.Theme.Error,L"Error : %r\n",status);
        return status;
    }
    if(!Node->IsDirectory){
        Node->Close(Node);
        ShellPrint(ActualConfig.Theme.Error,L"%s isn't a directory\n",argv[1]);
        return EFI_UNSUPPORTED;
    }
    ActualNode->Close(ActualNode);
    ActualNode = Node;
    return EFI_SUCCESS;
}

EFI_STATUS CMDpwd(UINTN argc, CHAR16** argv){
    CHAR16 buff[256];
    GetCurrentPathString(buff,256);
    ShellPrint(ActualConfig.Theme.Info,L"%s\n",buff);
    return EFI_SUCCESS;
}

EFI_STATUS CMDmkdir(UINTN argc, CHAR16** argv){
    if(argc < 2 || StrLen(argv[1]) == 0){
        ShellPrint(ActualConfig.Theme.Info,L"Usage : mkdir <folder>\n");
        return EFI_INVALID_PARAMETER;
    }
    FS_NODE* Node;
    EFI_STATUS status = VFSOpen(ActualNode,argv[1],&Node,EFI_FILE_MODE_READ,EFI_FILE_DIRECTORY);
    if(!EFI_ERROR(status)){
        ShellPrint(ActualConfig.Theme.Warning,L"%s already exist !\n",argv[1]);
        Node->Close(Node);
        return EFI_WRITE_PROTECTED;
    }
    status = VFSOpen(ActualNode,argv[1],&Node,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,EFI_FILE_DIRECTORY);
    if(EFI_ERROR(status)){
        ShellPrint(ActualConfig.Theme.Error,L"Error while creating: %r\n",status);
        return status;
    }
    Node->Close(Node);
    ShellPrint(ActualConfig.Theme.Sucess,L"%s created successfully !\n",argv[1]);
    return EFI_SUCCESS;
}

EFI_STATUS CMDrm(UINTN argc, CHAR16** argv){

    if(argc < 2 || StrLen(argv[1]) == 0){
        ShellPrint(ActualConfig.Theme.Info,L"Usage : rm <file/folder>\n");
        return EFI_INVALID_PARAMETER;
    }
    FS_NODE* Node;
    EFI_STATUS status = VFSOpen(ActualNode,argv[1],&Node,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,0);
    if(EFI_ERROR(status)){
        ShellPrint(ActualConfig.Theme.Error,L"Error while opening : %r\n",status);
        return status;
    }
    status = Node->Delete(Node);
    if(status==EFI_WARN_DELETE_FAILURE)
        ShellPrint(ActualConfig.Theme.Error,L"Error while deleting : %r\n",status);
    return status;
}

EFI_STATUS CMDcp(UINTN argc, CHAR16** argv){
    if(argc < 3){
        ShellPrint(ActualConfig.Theme.Warning,L"Usage : cp <src> <dest> [overwrite]\n");
        return EFI_INVALID_PARAMETER;
    }
    BOOLEAN overwrite = (argc >= 4 && StrCmp(argv[3], L"overwrite") == 0);
    FS_NODE *src = NULL, *dest = NULL;
    EFI_STATUS status = VFSOpen(ActualNode,argv[1],&src,EFI_FILE_MODE_READ,0);
    
    if(EFI_ERROR(status)){
        ShellPrint(ActualConfig.Theme.Error,L"Error while opening source : %r\n",status);
        return status;
    }
    UINTN Size = 0;
    VOID* buff;
    status = VFSRead(src,&buff,&Size);
    src->Close(src);
    if(EFI_ERROR(status)){
        ShellPrint(ActualConfig.Theme.Error,L"Error while reading : %r\n",status);
        return status;
    }
    status = VFSOpen(ActualNode,argv[2],&dest,EFI_FILE_MODE_READ,0);
    
    if(!EFI_ERROR(status)){
        if(dest->IsDirectory){
            FS_NODE* tmp = NULL;;
            status = VFSOpen(dest,argv[1],&tmp,EFI_FILE_MODE_READ,0);
            if(!EFI_ERROR(status)){
                if(overwrite){
                    dest->Close(dest);
                } else {
                    ShellPrint(ActualConfig.Theme.Error,L"Destination already exist. Abort. Add at the end \"overwrite\" to allow overwrite (return %r)\n",status);
                    dest->Close(dest);
                    tmp->Close(tmp);
                    kfree(buff);
                    return EFI_ABORTED;
                }
            } 
            status = VFSOpen(dest,argv[1],&tmp,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
            dest->Close(dest);
            dest = tmp;
        } else {
            if(overwrite){
                dest->Close(dest);
                status = VFSOpen(ActualNode,argv[2],&dest,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
            } else {
                ShellPrint(ActualConfig.Theme.Error,L"Destination already exist. Abort. Add at the end \"overwrite\" to allow overwrite\n");
                dest->Close(dest);
                kfree(buff);
                return EFI_ABORTED;
            }
            
        }
    } else {
        status = VFSOpen(ActualNode,argv[2],&dest,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
        if(EFI_ERROR(status)){
            kfree(buff);
            ShellPrint(ActualConfig.Theme.Error,L"Error while creating file : %r\n",status);
            return status;
        }
    }
    if(!dest)return EFI_ABORTED;
    if(dest->EfiFile==NULL){
        CHAR16 *tmp = kmalloc(Size*sizeof(CHAR16));
        Char8ToChar16(buff,tmp,Size);
        status = dest->Write(dest,tmp,Size*sizeof(CHAR16),FALSE);  
    }
    else status = dest->Write(dest,buff,Size,FALSE);
    if(EFI_ERROR(status))
        ShellPrint(ActualConfig.Theme.Error,L"Error while writing : %r\n",status);
    dest->Close(dest);
    kfree(buff);
    return status;
}

EFI_STATUS CMDcat(UINTN argc, CHAR16** argv) {
    if (argc < 2 || StrLen(argv[1]) == 0) {
        ShellPrint(ActualConfig.Theme.Warning, L"Usage : cat <filename>\n");
        return EFI_INVALID_PARAMETER;
    }
    FS_NODE* Node;
    EFI_STATUS status = VFSOpen(ActualNode,argv[1],&Node,EFI_FILE_MODE_READ,0);
    if(EFI_ERROR(status)){
        ShellPrint(ActualConfig.Theme.Error,L"Error : %r\n",status);
        return status;
    }
    if(Node->IsDirectory){
        ShellPrint(ActualConfig.Theme.Error,L"%s is a directory\n",argv[1]);
        Node->Close(Node);
        return status;
    }
    CHAR8* RawBuff;
    UINTN Size = 0;
    VFSRead(Node,(VOID**)&RawBuff,&Size);
    CHAR16* RefinedBuff = kmalloc((Size+1)*sizeof(CHAR16));
    if(!RefinedBuff){
        Node->Close(Node);
        kfree(RawBuff);
        ShellPrint(ActualConfig.Theme.Error,L"Ran out of ressources\n");
        return EFI_OUT_OF_RESOURCES;
    }
    for(UINTN i = 0; i < Size; i++){
        RefinedBuff[i]=(CHAR16)RawBuff[i]; //I know bad idea
    }
    RefinedBuff[Size]=L'\0';
    ShellPrint(ActualConfig.Theme.Info,L"%s\n",RefinedBuff);
    kfree(RawBuff); kfree(RefinedBuff);Node->Close(Node);
    return EFI_SUCCESS;
}

EFI_STATUS CMDvol(UINTN argc, CHAR16** argv){
    if(argc>1&&StrCmp(argv[1],L"help")==0){
        ShellPrint(ActualConfig.Theme.Info,L"Usage : listdrives <help|update>\n");
        return EFI_SUCCESS;
    }
    if(argc>1&&StrCmp(argv[1],L"update")){
        ShellPrint(ActualConfig.Theme.Error,L"Unknown argument :%s\nUsage : listdrives <help|update>\n",argv[1]);
        return EFI_INVALID_PARAMETER;
    }
    if(argc>1&&StrCmp(argv[1],L"update")==0){
        EFI_STATUS status = ListVolume(&Volumes,&VolumesCount);
        if(EFI_ERROR(status)){ShellPrint(ActualConfig.Theme.Error,L"Error while actualizing volumes : %r\n",status);return status;}
    }
    for(UINTN i = 0; i < VolumesCount; i++){
        ShellPrint(ActualConfig.Theme.Info,L"%s : %s\n",Volumes[i].Tag,StrLen(Volumes[i].Info->VolumeLabel)?Volumes[i].Info->VolumeLabel:L"NO LABEL");
    }
    return EFI_SUCCESS;
}


EFI_STATUS CMDnano(UINTN argc, CHAR16** argv){  
    if(argc < 2){
        ShellPrint(ActualConfig.Theme.Error, L"Usage : nano <file>\n");
        return EFI_INVALID_PARAMETER;
    }
    ShellPrint(ActualConfig.Theme.Warning,L"Not implemented yet (it's damn hard !)\n");
    return EFI_SUCCESS;

}

EFI_STATUS CMDtest(UINTN argc, CHAR16** argv){
    
    ShellPrint(RGB(255,0,0),L"Red   : abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890□\uFFFD\u16A0\n");
    ShellPrint(RGB(0,255,0),L"Green : abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890□\uFFFD\u16A0\n");
    ShellPrint(RGB(0,0,255),L"Blue  : abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890□\uFFFD\u16A0\n");
    ShellPrint(ActualConfig.Theme.Info,L"Turning on leds ...");
    EFI_STATUS status = SetKeyboardLeds(0x07);
    if(!EFI_ERROR(status)) ShellPrint(ActualConfig.Theme.Sucess,L"Turned on !\n"); else ShellPrint(ActualConfig.Theme.Error,L"Error : %r\n",status);
    return EFI_SUCCESS;
    
}

EFI_STATUS CMDcheckargs(UINTN argc, CHAR16** argv){
    ShellPrint(ActualConfig.Theme.Info,L"Argument count : %u\n",argc);
    for(UINTN i = 0; i<argc; i++){
        ShellPrint(ActualConfig.Theme.Info,L"Argument %u : %s\n",i,argv[i]);
    }
    return EFI_SUCCESS;
}

EFI_STATUS CMDconfig(UINTN argc, CHAR16** argv){
    ShellPrint(ActualConfig.Theme.Info,L"Horizontal resolution : %u\n",GopInfo->HorizontalResolution);
    ShellPrint(ActualConfig.Theme.Info,L"Vertical resolution   : %u\n",GopInfo->VerticalResolution);
    ShellPrint(ActualConfig.Theme.Info,L"Scanline size         : %u\n",GopInfo->PixelsPerScanLine);
    return EFI_SUCCESS;
}

EFI_STATUS CMDlistres(UINTN argc, CHAR16** argv){
    GopModeList* liste = GetModeList();
    for(UINTN i = 0; i<ModeCount; i++){
        if (liste[i].SizeX==(UINTN)(-1)){
            ShellPrint(ActualConfig.Theme.Warning,L"Mode %u not available\n",i);
            continue;
        }
        ShellPrint(ActualConfig.Theme.Info,L"Mode %u : %ux%u\n",i,liste[i].SizeX,liste[i].SizeY);
    }
    kfree(liste);
    return EFI_SUCCESS;
}

EFI_STATUS CMDsetres(UINTN argc, CHAR16** argv){
    if(ModeCount==(UINTN)(-1)){
        ShellPrint(ActualConfig.Theme.Warning,L"Resolution not enumerated - please use reslist\n");
        return EFI_NOT_READY;
    }
    UINTN ID = 0;
    while (*argv[1] != L'\0') {
        if (*argv[1] < L'0' || *argv[1] > L'9') {
            ShellPrint(ActualConfig.Theme.Warning,L"Usage : setres <ID> (ID is always an integrer)\n");
            return EFI_INVALID_PARAMETER;
        }
        ID = ID * 10 + (*argv[1] - L'0');  // Conversion en entier
        argv[1]++;
    }
    if(ID>=ModeCount){
        ShellPrint(ActualConfig.Theme.Warning,L"Resolution ID out of bound\n");
        return EFI_INVALID_PARAMETER;
    }
    EFI_STATUS status = SetMode(ID);
    if(EFI_ERROR(status)) ShellPrint(ActualConfig.Theme.Error,L"Error : %r\n",status);
    else ShellPrint(ActualConfig.Theme.Info,L"Mode %u set successfuly !\n",ID);
    return status;
    
}

EFI_STATUS RunCMD(CHAR16* buffer);
EFI_STATUS CMDsh(UINTN argc, CHAR16** argv){
    if(argc < 2){
        ShellPrint(ActualConfig.Theme.Error, L"Usage : sh <file>\n");
        return EFI_INVALID_PARAMETER;
    }
    FS_NODE* Node;
    EFI_STATUS status;
    status = VFSOpen(ActualNode,argv[1],&Node,EFI_FILE_MODE_READ,0);
    CHECK_STATUS(status);
    
    CHAR8* RawBuff;
    UINTN Size = 0;
    status = VFSRead(Node,(VOID**)&RawBuff,&Size);
    if(EFI_ERROR(status)){
        Node->Close(Node);
        return status; 
    }
    Node->Close(Node);

    CHAR16* Buff = kmalloc((Size + 1) * sizeof(CHAR16));
    if(!Buff) {
        kfree(RawBuff);
        return EFI_OUT_OF_RESOURCES;
    }

    for(UINTN i = 0; i < Size; i++){
        Buff[i]=(CHAR16)RawBuff[i]; 
    }
    Buff[Size] = L'\0';
    kfree(RawBuff);

    CHAR16* ptr = Buff;
    UINTN CMDSize = 0;

    while(ptr[CMDSize] != L'\0'){
        if(ptr[CMDSize] == L'\n' || ptr[CMDSize] == L'\r'){
            CHAR16 separator = ptr[CMDSize];
            ptr[CMDSize] = L'\0';
            
            if(StrLen(ptr) > 0){
                status = RunCMD(ptr);
                if(EFI_ERROR(status)){
                    kfree(Buff);
                    return status;
                }
            }
            ptr += CMDSize + 1;
            if(separator == L'\r' && *ptr == L'\n') {
                ptr++;
            }
            
            CMDSize = 0;
            continue; 
        }
        CMDSize++;
    }
    if(StrLen(ptr) > 0) {
        status = RunCMD(ptr);
    }
    
    kfree(Buff);
    return EFI_SUCCESS;
}

EFI_STATUS CMDimg(UINTN argc, CHAR16** argv){
    if(argc < 2){
        ShellPrint(ActualConfig.Theme.Error, L"Usage : img <file>\n");
        return EFI_INVALID_PARAMETER;
    }
    TemporaryBuffer(TRUE);
    EFI_STATUS status = LoadTGA(argv[1],0,0);
    Actualize();
    if(EFI_ERROR(status)){
        TemporaryBuffer(FALSE);
        ShellPrint(ActualConfig.Theme.Error,L"Error while loading %s : %r\n",argv[1],status);
        return status;
    }
    WaitForInput();
    TemporaryBuffer(FALSE); 
    Actualize();
    return EFI_SUCCESS;
}