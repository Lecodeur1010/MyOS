#include "cmd.h"
#include "func.h"
#include "display.h"
#include "disk.h"
#include "graphics.h"
#include "memory.h"
#include "net.h"
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
    {L"hexdump",CMDhexdump,L"Dump fiel content in hex"},
    {L"cp",CMDcp,L"Copy file"},
    {L"dd",CMDdd,L"Copy defines size block from file"},
    {L"nano",CMDnano,L"Edit text files"},//Need to implement it asap
    {L"vol",CMDvol,L"List/ update volumes"},
    {L"test",CMDtest,L"Test the screen"},
    {L"checkargs",CMDcheckargs,L"Check arguments parsing (for debug purpose)"},
    {L"config",CMDconfig,L"Get current screen configuration"},
    {L"listres",CMDlistres,L"Get resolution list"},
    {L"setres",CMDsetres,L"Set resolution based on a mode ID"},
    {L"echo",CMDecho,L"Print the first arg if present"},
    {L"sh",CMDsh,L"Run a script file"},
    {L"img",CMDimg,L"Render a TGA image"},
    {L"raminfo",CMDraminfo,L"Fetch RAM info"},
    {L"loadcfg",CMDloadcfg,L"Load a configuration file"},
    {L"reloadcfg",CMDreloadcfg,L"Reload the boot configuration file"},
    {L"resetcfg",CMDresetcfg,L"Reset actual configuration to the default one"},
    {L"download",CMDdownload,L"Downlaod a file from Internet"},
    {L"netdetail",CMDnetdetails,L"Get network details"},
    {L"nslookup",CMDnslookup,L"Lookup an domain name"},
    {L"setfont",CMDsetfont,L"Set font"},
};

UINTN CMD_COUNT = sizeof(Commands) / sizeof(COMMAND);

EFI_STATUS CMDecho(UINTN argc, CHAR16** argv){
    if(argc<2) return EFI_SUCCESS;
    ShellPrint(ActualConfig.Theme.Info, L"%s\n", argv[1]);
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
                CMDIndex = (UINTN)i;
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
        CHECK_STATUS(status,L"Error while opening %s : %r\n",FALSE,NOP,argv[1],status);
    }
    if(!Node->IsDirectory){
        ShellPrint(ActualConfig.Theme.Error,L"%s is a file\n",argv[1]);
        Node->Close(Node);
        return EFI_UNSUPPORTED;
    }
    status = Node->List(Node,&FileInfo,&Count);
    CHECK_STATUS(status,L"Error while listing : %r\n",FALSE,{if(Node!=ActualNode)Node->Close(Node);},status);
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
    CHECK_STATUS(status,L"Error : %r\n",FALSE,NOP,status);
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
    CHECK_STATUS(status,L"Error while creating : %r\n",FALSE,NOP,status);
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
    CHECK_STATUS(status,L"Error while opening : %r\n",FALSE,NOP,status);
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
    
    CHECK_STATUS(status,L"Error while source : %r\n",FALSE,NOP,status);
    UINTN Size = 0;
    VOID* buff;
    status = VFSRead(src,&buff,&Size);
    src->Close(src);
    CHECK_STATUS(status,L"Error while reading : %r\n",FALSE,NOP,status);
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
        CHECK_STATUS(status,L"Error while creating %s : %r\n",FALSE,NOP,argv[2],status);
    }
    if(!dest)return EFI_ABORTED;
    if(dest->EfiFile==NULL){
        CHAR16 *tmp = NULL;
        Char8ToChar16(buff,&tmp);
        status = dest->Write(dest,tmp,Size*sizeof(CHAR16),FALSE);  
    }
    else status = dest->Write(dest,buff,Size,FALSE);
    dest->Close(dest);
    CHECK_STATUS(status,L"Error while writing : %r\n",FALSE,NOP,status);
    kfree(buff);
    return status;
}

#define IO_BUFFER_SIZE 8192
EFI_STATUS CMDdd(UINTN argc, CHAR16** argv){
    CHAR16 *InputFile = NULL, *OutputFile = NULL;
    INTN BlockCount = -1, BlockSize = 1;
    EFI_STATUS status;
    for(UINTN i = 1; i < argc; i++){ //argv[0] is the cmd name
        if(StrnCmp(argv[i],L"if=",3) == 0)    InputFile=argv[i]+3;
        if(StrnCmp(argv[i],L"of=",3) == 0)    OutputFile=argv[i]+3;
        if(StrnCmp(argv[i],L"count=",6) == 0) BlockCount=(INTN)Atoi(argv[i]+6);
        if(StrnCmp(argv[i],L"bs=",3) == 0)    BlockSize=(INTN)Atoi(argv[i]+3);
    }
    if(!InputFile||StrLen(InputFile) == 0||!OutputFile||StrLen(OutputFile) == 0||BlockCount == -1){
        ShellPrint(ActualConfig.Theme.Error,L"Error : Missing argument : ");
        if(!InputFile||StrLen(InputFile) == 0) ShellPrint(ActualConfig.Theme.Error,L"Input File;");
        if(!OutputFile||StrLen(OutputFile) == 0) ShellPrint(ActualConfig.Theme.Error,L"Output File;");
        if(BlockCount==-1) ShellPrint(ActualConfig.Theme.Error,L"Block Count;");
        ShellPrint(ActualConfig.Theme.Info,L"\nUsage : dd if=<src> of=<dest> count=<nb of block to copy> [bs=<size of block, default : 1>]\n");
        return EFI_INVALID_PARAMETER;
    }
    FS_NODE *InputNode, *OutputNode;
    status = VFSOpen(ActualNode,InputFile,&InputNode,EFI_FILE_MODE_READ,0);
    CHECK_STATUS(status,L"Error while opening %s : %r\n",FALSE,NOP,InputFile,status);
    status = VFSOpen(ActualNode,OutputFile,&OutputNode,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
    CHECK_STATUS(status,L"Error while opening %s : %r\n",FALSE,NOP,OutputFile,status);
    OutputNode->Reset(OutputNode);
    VOID* IoBuffer = kmalloc(IO_BUFFER_SIZE); 
    UINTN BufferOffset = 0;
    for(UINTN i = 0; i < BlockCount; i++) {
        UINTN SizeToRead = (UINTN)BlockSize;
        VOID* TempBlock = NULL;
        status = VFSRead(InputNode, &TempBlock, &SizeToRead);
        if (EFI_ERROR(status) || SizeToRead == 0) break; // Fin de fichier ou erreur

        // Si le bloc est plus grand que notre buffer, on flush et on écrit directement
        if (BufferOffset + SizeToRead > IO_BUFFER_SIZE) {
            OutputNode->Write(OutputNode, IoBuffer, BufferOffset, TRUE);
            BufferOffset = 0;
        }

        CopyMem((UINT8*)IoBuffer + BufferOffset, TempBlock, SizeToRead);
        BufferOffset += SizeToRead;
        
        kfree(TempBlock);
    }
    if (BufferOffset > 0) {
        OutputNode->Write(OutputNode, IoBuffer, BufferOffset, TRUE);
    }
    kfree(IoBuffer);
    InputNode->Close(InputNode);
    OutputNode->Close(OutputNode);
    return EFI_SUCCESS;
}


EFI_STATUS CMDcat(UINTN argc, CHAR16** argv) {
    if (argc < 2 || StrLen(argv[1]) == 0) {
        ShellPrint(ActualConfig.Theme.Warning, L"Usage : cat <filename>\n");
        return EFI_INVALID_PARAMETER;
    }
    FS_NODE* Node;
    EFI_STATUS status = VFSOpen(ActualNode,argv[1],&Node,EFI_FILE_MODE_READ,0);
    CHECK_STATUS(status,L"Error : %r\n",FALSE,NOP,status);
    if(Node->IsDirectory){
        ShellPrint(ActualConfig.Theme.Error,L"%s is a directory\n",argv[1]);
        Node->Close(Node);
        return status;
    }
    CHAR8* RawBuff;
    UINTN Size = 0;
    status = VFSRead(Node,(VOID**)&RawBuff,&Size);
    CHECK_STATUS(status,NULL,FALSE,NOP);
    CHAR16* RefinedBuff = NULL;
    Char8ToChar16(RawBuff,&RefinedBuff);
    ShellPrint(ActualConfig.Theme.Info,L"%s\n",RefinedBuff);
    kfree(RawBuff); kfree(RefinedBuff);Node->Close(Node);
    return EFI_SUCCESS;
}


EFI_STATUS CMDhexdump(UINTN argc, CHAR16** argv) {
    if (argc < 2 || StrLen(argv[1]) == 0) {
        ShellPrint(ActualConfig.Theme.Warning, L"Usage : hexdump <filename> [size]\n");
        return EFI_INVALID_PARAMETER;
    }
    FS_NODE* Node;
    EFI_STATUS status = VFSOpen(ActualNode,argv[1],&Node,EFI_FILE_MODE_READ,0);
    CHECK_STATUS(status,L"Error while opening %s : %r\n",FALSE,NOP,argv[1],status);
    UINT8* Buff = NULL;
    UINTN Size = 0;
    if(argc>2){
        Size=StrToHex(argv[2]);
    }
    status = VFSRead(Node,(VOID**)&Buff,&Size);
    CHECK_STATUS(status,L"Error while reading %s : %r",FALSE,Node->Close(Node),argv[1],status);
    Node->Close(Node);
    for(UINTN i = 0; i < Size; i++){
        CHAR16 Str[3];
        HexToStr(Buff[i],Str,3,FALSE,2);
        ShellPrint(ActualConfig.Theme.Info,L"%s ",Str);
    }
    ShellPrint(ActualConfig.Theme.Info,L"\n");
    return EFI_SUCCESS;
}

EFI_STATUS CMDvol(UINTN argc, CHAR16** argv){
    if(argc>1&&StrCmp(argv[1],L"help")==0){
        ShellPrint(ActualConfig.Theme.Info,L"Usage : vol <help|update>\n");
        return EFI_SUCCESS;
    }
    if(argc>1&&StrCmp(argv[1],L"update")){
        ShellPrint(ActualConfig.Theme.Error,L"Unknown argument :%s\nUsage : listdrives <help|update>\n",argv[1]);
        return EFI_INVALID_PARAMETER;
    }
    if(argc>1&&StrCmp(argv[1],L"update")==0){
        EFI_STATUS status = ListVolume(&Volumes,&VolumesCount);
        CHECK_STATUS(status,L"Error while actualizing volumes: %r\n",FALSE,NOP,status);
    }
    for(UINTN i = 0; i < VolumesCount; i++){
        ShellPrint(ActualConfig.Theme.Info,L"%s : %s\n",Volumes[i].Tag,StrLen(Volumes[i].Info->VolumeLabel)?Volumes[i].Info->VolumeLabel:L"NO LABEL");
    }
    return EFI_SUCCESS;
}


EFI_STATUS CMDnano(UINTN argc, CHAR16** argv) {  
    if (argc < 2) {
        ShellPrint(ActualConfig.Theme.Error, L"Usage : nano <file>\n");
        return EFI_INVALID_PARAMETER;
    }

    FS_NODE* Node = NULL;
    EFI_STATUS status = VFSOpen(ActualNode, argv[1], &Node, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
    if (status == EFI_NOT_FOUND) {
        status = VFSOpen(ActualNode, argv[1], &Node, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
        CHECK_STATUS(status, L"Error while creating %s : %r\n", FALSE, NOP, argv[1], status);
    }
    CHECK_STATUS(status, L"Error while opening %s : %r\n", FALSE, NOP, argv[1], status);

    UINT8* RawBuff = NULL;
    UINTN Size = 0;
    status = VFSRead(Node, (VOID**)&RawBuff, &Size);

    // --- SÉCURITÉ FICHIER VIDE / RAWBUFF NULL ---
    UINTN StrLenRaw = (RawBuff != NULL && Size > 0) ? strlena((CHAR8*)RawBuff) : 0;
    
    CHAR16* Buff = NULL;
    if (StrLenRaw > 0) {
        Char8ToChar16(RawBuff, &Buff);
    } else {
        Buff[0] = L'\0';
    }

    UINTN LineCount = 1;
    for (UINTN i = 0; i < StrLenRaw; i++) {
        if (Buff[i] == L'\n') LineCount++;
    }

    NANO_LINE* Document = kmalloc(sizeof(NANO_LINE) * LineCount);
    UINTN StartIdx = 0;
    UINTN CurrentLine = 0;
    UINTN CurrentChar = 0;

    // --- DECOUPAGE EN LIGNES ---
    for (UINTN i = 0; i <= StrLenRaw; i++) {
        if (Buff[i] == L'\n' || Buff[i] == L'\0') {
            UINTN LineLen = i - StartIdx;
            CHAR16* tmp = kmalloc((LineLen + 1) * sizeof(CHAR16));
            if (LineLen > 0) {
                CopyMem(tmp, &Buff[StartIdx], LineLen * sizeof(CHAR16));
            }
            tmp[LineLen] = L'\0';  
            Document[CurrentLine].Buffer = tmp;
            Document[CurrentLine].UsedSize = LineLen * sizeof(CHAR16);
            Document[CurrentLine].CurrentSize = (LineLen + 1) * sizeof(CHAR16);
            CurrentLine++;     
            StartIdx = i + 1;
        }
    }
    kfree(Buff); // Libération du buffer de conversion temporaire

    CurrentLine = 0;
    CurrentChar = 0;

    // --- INITIALISATION DE L'ÉCRAN ---
    TemporaryBuffer(TRUE);
    FillDisplay(ActualConfig.Theme.Background);
    SetCursor(0, 0);
    ShellPrint(ActualConfig.Theme.Warning, L"NANO - EXPERIMENTAL - ESC TO ABORT - F1 TO EXIT AND SAVE\n");
    
    for (UINTN i = 0; i < LineCount; i++) {
        ShellPrint(ActualConfig.Theme.Info, L"%s\n", Document[i].Buffer);
    }

    // --- BOUCLE D'ÉDITION ---
    while (1) {
        // Replacement systématique du curseur à sa position effective
        SetCursor((INTN)CurrentChar, (INTN)CurrentLine + 1);

        EFI_INPUT_KEY Key = WaitForInput();

        // --- ESC : ABORT ---
        if (Key.ScanCode == 0x17) {
            Node->Close(Node);
            for (UINTN i = 0; i < LineCount; i++) {
                kfree(Document[i].Buffer);
            }
            kfree(Document);
            TemporaryBuffer(FALSE);
            return EFI_SUCCESS;
        } 
        // --- F1 : SAVE & EXIT ---
        else if (Key.ScanCode == 0x0b) {
            Node->Reset(Node);
            for (UINTN i = 0; i < LineCount; i++) {           
                CHAR8* tmp = NULL;
                UINTN l = Char16ToChar8(Document[i].Buffer, &tmp);
                tmp[l] = '\n';
                Node->Write(Node, tmp, l + 1, TRUE);
                
                kfree(tmp);
            }
            Node->Close(Node);
            for (UINTN i = 0; i < LineCount; i++) {
                kfree(Document[i].Buffer);
            }
            kfree(Document);
            TemporaryBuffer(FALSE);
            return EFI_SUCCESS; 
        }
        // --- FLECHE HAUT ---
        else if (Key.ScanCode == 0x01) {
            if (CurrentLine > 0) {
                CurrentLine--;
                UINTN MaxChars = Document[CurrentLine].UsedSize / sizeof(CHAR16);
                if (CurrentChar > MaxChars) CurrentChar = MaxChars;
            }
        } 
        // --- FLECHE BAS ---
        else if (Key.ScanCode == 0x02) {
            if (CurrentLine + 1 < LineCount) {
                CurrentLine++;
                UINTN MaxChars = Document[CurrentLine].UsedSize / sizeof(CHAR16);
                if (CurrentChar > MaxChars) CurrentChar = MaxChars;
            }
        } 
        // --- FLECHE DROITE ---
        else if (Key.ScanCode == 0x03) {
            UINTN MaxChars = Document[CurrentLine].UsedSize / sizeof(CHAR16);
            if (CurrentChar < MaxChars) {
                CurrentChar++;
            } else if (CurrentLine + 1 < LineCount) {
                CurrentLine++;
                CurrentChar = 0;
            }
        } 
        // --- FLECHE GAUCHE ---
        else if (Key.ScanCode == 0x04) {
            if (CurrentChar > 0) {
                CurrentChar--;
            } else if (CurrentLine > 0) {
                CurrentLine--;
                CurrentChar = Document[CurrentLine].UsedSize / sizeof(CHAR16);
            }
        } 
        else if (Key.UnicodeChar == L'\r' || Key.UnicodeChar == L'\n') {
            
            // 1. Allouer un nouveau tableau de lignes (+1 ligne)
            NANO_LINE* NewDocument = kmalloc(sizeof(NANO_LINE) * (LineCount + 1));

            // 2. Copier les lignes intactes situées AVANT la coupure (y compris la ligne courante)
            for (UINTN i = 0; i <= CurrentLine; i++) {
                NewDocument[i] = Document[i];
            }

            // 3. Décaler les lignes situées APRÈS la coupure d'un index vers le bas
            for (UINTN i = CurrentLine + 1; i < LineCount; i++) {
                NewDocument[i + 1] = Document[i];
            }

            // 4. Calculer combien de caractères doivent être basculés sur la nouvelle ligne
            UINTN MaxChars = NewDocument[CurrentLine].UsedSize / sizeof(CHAR16);
            UINTN CharsToMove = MaxChars - CurrentChar;

            // 5. Initialiser la nouvelle ligne (CurrentLine + 1)
            NewDocument[CurrentLine + 1].Buffer = kmalloc((CharsToMove + 1) * sizeof(CHAR16));
            
            if (CharsToMove > 0) {
                CopyMem(
                    NewDocument[CurrentLine + 1].Buffer,
                    &NewDocument[CurrentLine].Buffer[CurrentChar],
                    CharsToMove * sizeof(CHAR16)
                );
            }
            NewDocument[CurrentLine + 1].Buffer[CharsToMove] = L'\0';
            NewDocument[CurrentLine + 1].UsedSize = CharsToMove * sizeof(CHAR16);
            NewDocument[CurrentLine + 1].CurrentSize = (CharsToMove + 1) * sizeof(CHAR16);

            // 6. Tronquer la ligne d'origine exactement là où se trouvait le curseur
            NewDocument[CurrentLine].Buffer[CurrentChar] = L'\0';
            NewDocument[CurrentLine].UsedSize = CurrentChar * sizeof(CHAR16);

            // 7. Remplacer l'ancien tableau par le nouveau et mettre à jour les compteurs
            kfree(Document);
            Document = NewDocument;
            LineCount++;
            CurrentLine++;    // Le curseur descend sur la nouvelle ligne
            CurrentChar = 0;  // Et se place tout à gauche

            // 8. Redessin complet (indispensable car toutes les lignes du dessous ont été décalées)
            FillDisplay(ActualConfig.Theme.Background);
            SetCursor(0, 0);
            ShellPrint(ActualConfig.Theme.Warning, L"NANO - EXPERIMENTAL - ESC TO ABORT - F1 TO EXIT AND SAVE\n");
            
            for (UINTN i = 0; i < LineCount; i++) {
                SetCursor(0,(INTN) i + 1);
                ShellPrint(ActualConfig.Theme.Info, L"%s", Document[i].Buffer);
            }
        }
        // --- BACKSPACE ---
        else if (Key.UnicodeChar == L'\b') {
            if (CurrentChar > 0) {
                UINTN MaxChars = Document[CurrentLine].UsedSize / sizeof(CHAR16);
                UINTN CharsToCopy = MaxChars - CurrentChar;
                
                if (CharsToCopy > 0) {
                    CopyMem(
                        &Document[CurrentLine].Buffer[CurrentChar - 1], 
                        &Document[CurrentLine].Buffer[CurrentChar], 
                        CharsToCopy * sizeof(CHAR16)
                    );
                }
                CurrentChar--;
                Document[CurrentLine].UsedSize -= sizeof(CHAR16);
                Document[CurrentLine].Buffer[Document[CurrentLine].UsedSize / sizeof(CHAR16)] = L'\0';

                SetCursor(0, (INTN)CurrentLine + 1);
                ShellPrint(ActualConfig.Theme.Info, L"%s ", Document[CurrentLine].Buffer);
            } 
            else if (CurrentLine > 0) {
                UINTN PrevLine = CurrentLine - 1;
                
                UINTN PrevLen = Document[PrevLine].UsedSize / sizeof(CHAR16);
                UINTN CurrLen = Document[CurrentLine].UsedSize / sizeof(CHAR16);
                
                // 1. Allouer un nouveau buffer pour contenir les deux lignes
                CHAR16* MergedBuffer = kmalloc((PrevLen + CurrLen + 1) * sizeof(CHAR16));
                
                // 2. Copier le contenu de la ligne précédente
                if (PrevLen > 0) {
                    CopyMem(MergedBuffer, Document[PrevLine].Buffer, PrevLen * sizeof(CHAR16));
                }
                
                // 3. Ajouter le contenu de la ligne courante
                if (CurrLen > 0) {
                    CopyMem(&MergedBuffer[PrevLen], Document[CurrentLine].Buffer, CurrLen * sizeof(CHAR16));
                }
                MergedBuffer[PrevLen + CurrLen] = L'\0';
                
                // 4. Nettoyer les anciens buffers de texte
                kfree(Document[PrevLine].Buffer);
                kfree(Document[CurrentLine].Buffer);
                
                // 5. Assigner le buffer fusionné à la ligne précédente
                Document[PrevLine].Buffer = MergedBuffer;
                Document[PrevLine].UsedSize = (PrevLen + CurrLen) * sizeof(CHAR16);
                Document[PrevLine].CurrentSize = (PrevLen + CurrLen + 1) * sizeof(CHAR16);
                
                // 6. Décaler le tableau de structures avec CopyMem (plutôt qu'une boucle 'for')
                UINTN LinesToShift = LineCount - 1 - CurrentLine;
                if (LinesToShift > 0) {
                    CopyMem(
                        &Document[CurrentLine],
                        &Document[CurrentLine + 1],
                        LinesToShift * sizeof(NANO_LINE)
                    );
                }
                
                // 7. Mettre à jour les compteurs et la position du curseur
                LineCount--;
                CurrentLine = PrevLine;
                CurrentChar = PrevLen;
                
                // 8. Redessin complet
                FillDisplay(ActualConfig.Theme.Background);
                SetCursor(0, 0);
                ShellPrint(ActualConfig.Theme.Warning, L"NANO - EXPERIMENTAL - ESC TO ABORT - F1 TO EXIT AND SAVE\n");
                
                for (UINTN i = 0; i < LineCount; i++) {
                    SetCursor(0, (INTN) i + 1);
                    ShellPrint(ActualConfig.Theme.Info, L"%s", Document[i].Buffer);
                }
            }
        }
        else if (Key.UnicodeChar >= 0x20) {
            UINTN MaxChars = Document[CurrentLine].UsedSize / sizeof(CHAR16);
            CHAR16* newBuf = kmalloc((MaxChars + 2) * sizeof(CHAR16));
            for (UINTN i = 0; i < CurrentChar; i++) {
                newBuf[i] = Document[CurrentLine].Buffer[i];
            }
            newBuf[CurrentChar] = Key.UnicodeChar;
            for (UINTN i = CurrentChar; i < MaxChars; i++) {
                newBuf[i + 1] = Document[CurrentLine].Buffer[i];
            }
            newBuf[MaxChars + 1] = L'\0';
            kfree(Document[CurrentLine].Buffer);
            Document[CurrentLine].Buffer = newBuf;
            Document[CurrentLine].UsedSize += sizeof(CHAR16);
            Document[CurrentLine].CurrentSize = (MaxChars + 2) * sizeof(CHAR16);
            CurrentChar++;
            SetCursor(0, (INTN)CurrentLine + 1);
            ShellPrint(ActualConfig.Theme.Info, L"%s", Document[CurrentLine].Buffer);
        }
    }

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
    CHECK_STATUS(status,L"Error : %r\n",FALSE,;,status);
    ShellPrint(ActualConfig.Theme.Info,L"Mode %u set successfuly !\n",ID);
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
    CHECK_STATUS(status,L"Couln't open the file\n",FALSE,NOP);
    
    CHAR8* RawBuff;
    UINTN Size = 0;
    status = VFSRead(Node,(VOID**)&RawBuff,&Size);
    Node->Close(Node);
    CHECK_STATUS(status,L"Couln't read the file\n",FALSE,NOP);
    CHAR16* Buff = NULL;

    Char8ToChar16(RawBuff,&Buff);
    kfree(RawBuff);

    CHAR16* ptr = Buff;
    UINTN CMDSize = 0;

    while(ptr[CMDSize] != L'\0'){
        if(ptr[CMDSize] == L'\n' || ptr[CMDSize] == L'\r'){
            CHAR16 separator = ptr[CMDSize];
            ptr[CMDSize] = L'\0';
            
            if(StrLen(ptr) > 0){
                status = RunCMD(ptr);
                CHECK_STATUS(status,NULL,FALSE,kfree(Buff););
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
    CHECK_STATUS(status,L"Error while loading %s: %r\n",FALSE,TemporaryBuffer(FALSE);,argv[1],status);
    WaitForInput();
    TemporaryBuffer(FALSE); 
    Actualize();
    return EFI_SUCCESS;
}

EFI_STATUS CMDraminfo(UINTN argc, CHAR16** argv){
    VOID *Data;
    UINTN DataSize, UsedSize, IncludeHeaders;
    CHAR16 UsableStr[64], UsedStr[64], IncludeHeadersStr[64];
    GetMemoryDetails(&Data,&DataSize,&UsedSize,&IncludeHeaders);
    FormatBytes(IncludeHeaders,IncludeHeadersStr);
    FormatBytes(DataSize,UsableStr);
    FormatBytes(UsedSize,UsedStr);
    ShellPrint(ActualConfig.Theme.Info,L"Usable RAM adress : %016llx ; Size : %s (Used : %s (Including headers : %s))\n",Data,UsableStr,UsedStr,IncludeHeadersStr);
    return EFI_SUCCESS;
}

VOID CleanCRLF(CHAR8 *str) {
    CHAR8 *src = str;
    CHAR8 *dst = str;
    while (*src != '\0') {
        if (src[0] == '\r' && src[1] == '\r') {
            src++; // On saute le \r en trop
            continue;
        }
        *dst++ = *src++;
    }
    *dst = '\0';
}

EFI_STATUS CMDdownload(UINTN argc, CHAR16** argv) {
    if (argc < 4) {
        ShellPrint(ActualConfig.Theme.Error, L"Usage : download <server IP> <remote path> <local filename>\n");
        return EFI_INVALID_PARAMETER;
    }
    // 1. Validation IP
    EFI_IPv4_ADDRESS TargetIp;
    if (!ParseIPv4(argv[1], TargetIp.Addr)) {
        EFI_STATUS status = ResolveHostName(argv[1],&TargetIp);
        EFI_IPv4_ADDRESS Null = {{0,0,0,0}};
        if(EFI_ERROR(status)||CompareMem(&TargetIp,&Null,sizeof(EFI_IPv4_ADDRESS))==0){
            CPrint(ActualConfig.Theme.Error,L"Couldn't resolve %s\n",argv[1]);
            return status;
        } else {
            CPrint(ActualConfig.Theme.Info,L"%s resolved as %u.%u.%u.%u\n",argv[1],TargetIp.Addr[0],TargetIp.Addr[1],TargetIp.Addr[2],TargetIp.Addr[3]);
        }
    }

    // 2. Nettoyage du chemin distant
    CONST CHAR16 *RemotePath = argv[2];
    if (RemotePath[0] == L'/') RemotePath++;

    // 3. Payload HTTP
    // 3. Payload HTTP avec retours CRLF (\r\n) obligatoires
    CONST CHAR16 *Format = 
        L"GET /%s HTTP/1.1\n"
        L"Host: %s\n"
        L"User-Agent: Mozilla/5.0 (UEFI; x64)\n"
        L"Accept: */*\n"
        L"Connection: close\n\n";

    // Allocation sécurisée avec une marge généreuse pour la requête
    UINTN NeededSize = (StrLen(Format) + StrLen(RemotePath) + StrLen(argv[1]) + 64)*sizeof(CHAR16);
    CHAR16* Payload = kmalloc(NeededSize);
    if (!Payload) {
        return EFI_OUT_OF_RESOURCES;
    }

    UnicodeSPrint(Payload, NeededSize, Format, 
                RemotePath, 
                argv[1]);

    
    CHAR8* PayloadAscii = NULL;
    Char16ToChar8(Payload,&PayloadAscii);
    kfree(Payload);
    UINTN PayloadSize=strlena(PayloadAscii);
    Print(L"NeededSize : %u; Size : %u\n",NeededSize,PayloadSize);
    // 4. Envoi / Réception TCP
    UINTN ResponseSize = 0;
    CHAR8* Content = NULL;
    UINTN Time = 10000; // Timeout augmenté pour les gros fichiers (10s)
    EFI_STATUS status = SendTCPRequest(&Time, &TargetIp, 80, PayloadAscii, PayloadSize, (VOID**)&Content, &ResponseSize);
    kfree(PayloadAscii);

    CHECK_STATUS(status, L"Error while downloading content : %r\n", FALSE, NOP, status);

    // 5. Extraction du corps binaire (TGA) en ignorant les en-têtes HTTP
    VOID *FileData = Content;
    UINTN FileSize = ResponseSize;

    // Chercher la fin des headers HTTP ("\r\n\r\n")
    for (UINTN i = 0; i < ResponseSize - 4; i++) {
        if (Content[i] == '\r' && Content[i+1] == '\n' && 
            Content[i+2] == '\r' && Content[i+3] == '\n') {
            FileData = (VOID*)&Content[i + 4];
            FileSize = ResponseSize - (i + 4);
            break;
        }
    }

    ShellPrint(ActualConfig.Theme.Info, L"Reçu : %d octets au total (Fichier binaire : %d octets)\n", ResponseSize, FileSize);

    // 6. Écriture sur le disque UEFI
    FS_NODE* Node = NULL;
    status = VFSOpen(ActualNode, argv[3], &Node, EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE | EFI_FILE_MODE_WRITE, 0);
    CHECK_STATUS(status, L"Error while creating the file : %r\n", FALSE, kfree(Content), status);

    Node->Reset(Node);
    status = Node->Write(Node, FileData, FileSize, FALSE);
    
    kfree(Content);
    Node->Close(Node);

    return status;
}

EFI_STATUS CMDnetdetails(UINTN argc, CHAR16** argv){
    EFI_IPv4_ADDRESS Device,Mask,Gateway,*DNS;
    UINTN DNSServerCount = 0;
    CHECK_STATUS(GetAdresses(&Device,&Mask,&Gateway,&DNS,&DNSServerCount),L"Couldn't fetch network details : %r\n",FALSE,;,_s);
        ShellPrint(ActualConfig.Theme.Info,L"Device Adress  : %u.%u.%u.%u\n",   Device.Addr[0], Device.Addr[1], Device.Addr[2], Device.Addr[3] );
        ShellPrint(ActualConfig.Theme.Info,L"Subnet Mask    : %u.%u.%u.%u\n",   Mask.Addr[0],   Mask.Addr[1],   Mask.Addr[2],   Mask.Addr[3]   );
        ShellPrint(ActualConfig.Theme.Info,L"Gateway Adress : %u.%u.%u.%u\n",   Gateway.Addr[0],Gateway.Addr[1],Gateway.Addr[2],Gateway.Addr[3]);
    for(UINTN i = 0; i < DNSServerCount; i++){
        ShellPrint(ActualConfig.Theme.Info,L"DNS Adress [%u] : %u.%u.%u.%u\n",i,DNS[i].Addr[0], DNS[i].Addr[1], DNS[i].Addr[2], DNS[i].Addr[3] );
    }
    return EFI_SUCCESS;
}

EFI_STATUS CMDnslookup(UINTN argc, CHAR16** argv){
    if (argc < 2) {
        ShellPrint(ActualConfig.Theme.Error, L"Usage : nslookup <domain name>\n");
        return EFI_INVALID_PARAMETER;
    }
    EFI_IPv4_ADDRESS Addr;
    EFI_IPv4_ADDRESS Null = {{0,0,0,0}};
    EFI_STATUS status = ResolveHostName(argv[1],&Addr);
    if(EFI_ERROR(status)||CompareMem(&Addr,&Null,sizeof(EFI_IPv4_ADDRESS))==0){
        ShellPrint(ActualConfig.Theme.Error,L"Couldn't resolve %s\n",argv[1]);
        return EFI_ERROR(status)?status:EFI_NOT_FOUND;
    }
    else 
        ShellPrint(ActualConfig.Theme.Info,L"%u.%u.%u.%u\n",Addr.Addr[0],Addr.Addr[1],Addr.Addr[2],Addr.Addr[3]);
    return status;
}

EFI_STATUS CMDsetfont(UINTN argc, CHAR16** argv){
    if (argc < 2) {
        ShellPrint(ActualConfig.Theme.Error, L"Usage : setfont <font file>\n");
        return EFI_INVALID_PARAMETER;
    }
    FS_NODE* Node = NULL;
    UINTN ContentSize = 0;
    VOID* buff = NULL;
    CHECK_STATUS(VFSOpen(ActualNode, argv[1], &Node, EFI_FILE_MODE_READ, 0),L"Couldn't open font file : %r\n", FALSE, NOP, _s);            
    CHECK_STATUS(VFSRead(Node, &buff, &ContentSize),L"Couldn't read font file : %r\n", FALSE, Node->Close(Node), _s);
    EFI_STATUS status = InitFont(buff);
    Node->Close(Node);
    if (buff) kfree(buff);
    if (EFI_ERROR(status)) {
        ShellPrint(ActualConfig.Theme.Error, L"Couldn't load font : %r\n", status);
        return status;
    }
    CMDclear(0, NULL);
    return EFI_SUCCESS;
}