#include <efi.h>
#include <efilib.h>
#include "graphics.h"
#include "func.h"
#include "cmd.h"
#include "display.h"
#include "disk.h"

VOID PrintPrompt(CHAR16* Prompt, CHAR16* Path) {
    // Parcourir le prompt pour trouver le token "%s"
    for (UINTN i = 0; Prompt[i] != L'\0'; i++) {
        if (Prompt[i] == L'%' && Prompt[i+1] == L's') {
            // On a trouvé le token, on affiche le chemin à la place
            CPrint(ActualConfig.Theme.Prompt, L"%s", Path);
            i++; // Sauter le 's'
        } else {
            // Sinon, on affiche le caractère brut
            CPrint(ActualConfig.Theme.Prompt, L"%c", Prompt[i]);
        }
    }
}

CHAR16* WaitForCommand(){
    CHAR16* buffer = kmalloc(256 * sizeof(CHAR16));
    SetMem(buffer, 256 * sizeof(CHAR16), 0);
    if(!buffer) return NULL;
    UINT8 pos = 0;
    CHAR16 buff[256];
    GetCurrentPathString(buff, 256);
    //dezed();
    CPrint(ActualConfig.Theme.Prompt,ActualConfig.Prompt, buff); //Ici !
    
    while(1){
        CHAR16 Key = WaitForInput().UnicodeChar;

        
        if(!Key)continue;
        if(Key == L'\b' && pos>0){
            pos--;
            CPrint(ActualConfig.Theme.Info,L"\b \b");
        }
        else if(Key == L'\r' || Key == L'\n'){
            buffer[pos]=L'\0';
            CPrint(ActualConfig.Theme.Info,L"\r\n");
            return buffer;           
        }
        else if (pos < 255 && Key >= ' '){
            buffer[pos++]=Key;
            CPrint(ActualConfig.Theme.Info,L"%c",Key);

        }
        
    }

}

EFI_STATUS RunCMD(CHAR16* buffer) {
    CHAR16* OffsetedBuffer = buffer;
    while (*OffsetedBuffer == L' ') OffsetedBuffer++; // Skip leading spaces
    if (*OffsetedBuffer == L'\0') {
        return EFI_SUCCESS;
    }

    // Phase 1: Backward Redirection Extraction
    CHAR16* redirect_file = NULL;
    BOOLEAN Append = FALSE;
    BOOLEAN InQuotes = FALSE;
    UINTN buffer_len = StrLen(OffsetedBuffer);

    for (INTN i = (INTN)buffer_len - 1; i >= 0; i--) {
        if (OffsetedBuffer[i] == L'\"') {
            InQuotes = !InQuotes;
            continue;
        }

        if (!InQuotes) {
            // Check for Append Mode (>>)
            if (i > 0 && OffsetedBuffer[i] == L'>' && OffsetedBuffer[i - 1] == L'>') {
                Append = TRUE;
                OffsetedBuffer[i - 1] = L'\0'; // Cleanly truncate command here
                redirect_file = &OffsetedBuffer[i + 1];
                break;
            }
            // Check for Write Mode (>)
            else if (OffsetedBuffer[i] == L'>') {
                OffsetedBuffer[i] = L'\0'; // Cleanly truncate command here
                redirect_file = &OffsetedBuffer[i + 1];
                break;
            }
        }
    }

    // Phase 2: Open Redirection Target if detected
    if (redirect_file) {
        while (*redirect_file == L' ') redirect_file++; // Skip spaces after operator
        
        // Strip quotes around file name if present
        UINTN file_len = StrLen(redirect_file);
        if (file_len >= 2 && redirect_file[0] == L'\"' && redirect_file[file_len - 1] == L'\"') {
            redirect_file[file_len - 1] = L'\0';
            redirect_file++;
        }

        UINT64 open_mode = EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE;
        EFI_STATUS status = VFSOpen(ActualNode, redirect_file, &ShellNode, open_mode, 0);
        if (EFI_ERROR(status)) {
            ShellNode = NULL;
            CPrint(ActualConfig.Theme.Warning, L"Couldn't open %s (%r); redirect to \\dev\\tty\n", redirect_file, status);
        } else {
            // Empty write to initialize/advance file cursor appropriately
            ShellNode->Write(ShellNode, L"", 0, Append);
        }
    }

    // Phase 3: Tokenize Arguments (Safe now because OffsetedBuffer is explicitly null-terminated)
    UINTN ArgCount = 1;
    InQuotes = FALSE;
    for (UINTN i = 1; OffsetedBuffer[i] != L'\0'; i++) {
        if (OffsetedBuffer[i] == L'\"') {
            InQuotes = !InQuotes;
            continue;
        }
        if (OffsetedBuffer[i] == L' ' && !InQuotes) {
            while (OffsetedBuffer[i] == L' ') i++;
            if (OffsetedBuffer[i] == L'\0') break;
            ArgCount++;
            i--;
        }
    }

    CHAR16* argv[ArgCount];
    argv[0] = OffsetedBuffer;
    InQuotes = FALSE;
    UINTN j = 1;
    for (UINTN i = 1; OffsetedBuffer[i] != L'\0'; i++) {
        if (OffsetedBuffer[i] == L'\"') {
            if (InQuotes) OffsetedBuffer[i] = L'\0';
            InQuotes = !InQuotes;
            continue;
        }
        if (OffsetedBuffer[i] == L' ' && !InQuotes) {
            OffsetedBuffer[i] = L'\0';
            i++;
            while (OffsetedBuffer[i] == L' ') i++;
            if (OffsetedBuffer[i] == L'\0') break;
            
            if (OffsetedBuffer[i] == L'\"')
                argv[j++] = OffsetedBuffer + i + 1;
            else 
                argv[j++] = OffsetedBuffer + i;
            i--;
        }
    }
    for (UINTN i = 0; i < CMD_COUNT; i++) {
        if (!StrCmp(argv[0], Commands[i].name)) {
            EFI_STATUS status = Commands[i].func(ArgCount, argv);
            if (ShellNode) { ShellNode->Close(ShellNode); ShellNode = NULL; }
            if (EFI_ERROR(status)) CPrint(ActualConfig.Theme.Error, L"%s raised : %r\n", argv[0], status);
            return status;
        }
    }

    CPrint(ActualConfig.Theme.Error, L"Error : CMD \"%s\" not recognized\n", OffsetedBuffer);
    if (ShellNode) { ShellNode->Close(ShellNode); ShellNode = NULL; }
    return EFI_NOT_FOUND;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    gImageHandle=ImageHandle;
    EFI_STATUS status[5];
    status[0] = GopInit();
    if (EFI_ERROR(status[0])) return status[0];
    status[1] = uefi_call_wrapper(BS->SetWatchdogTimer,4,0,0,0,NULL);
    status[2] = DiskInit();
    GeneralInit();
    status[3] = LoadCFG(L"\\mnt\\fs0\\boot.ini");
    
    FillDisplay(ActualConfig.Theme.Background);//Before this : loop in the BSOD; after : normal BSOD
    CPrint(ActualConfig.Theme.Info, L"Loading graphics ... ");
    CPrint(ActualConfig.Theme.Sucess, L"Graphics loaded !\n");
    CPrint(ActualConfig.Theme.Info, L"Disabling watchdog ... ");
    if(EFI_ERROR(status[1]))
        CPrint(ActualConfig.Theme.Warning,L"Could not disable watchdog timer : %r\n",status[1]);
    else
        CPrint(ActualConfig.Theme.Sucess,L"Watchdog diabled !\n");
    CPrint(ActualConfig.Theme.Info,L"Initializing drive FS ... ");
    if(EFI_ERROR(status[2]))
        CPrint(ActualConfig.Theme.Error,L"Error while loading FS : %r\n");
    else 
        CPrint(ActualConfig.Theme.Sucess,L"Drive FS initialized successfuly !\n");
    CPrint(ActualConfig.Theme.Info,L"Loading configuration file ... ");
    if(status[3] == EFI_NOT_FOUND)
        CPrint(ActualConfig.Theme.Warning,L"Configuration file not found \n");
    else if(EFI_ERROR(status[3]))
        CPrint(ActualConfig.Theme.Error,L"Error while loading configuration file : %r\n",status[3]);
    else 
        CPrint(ActualConfig.Theme.Sucess,L"Configuration file loaded successfuly !\n");
    CPrint(ActualConfig.Theme.Info, L"Loading boot script ... ");
    CHAR16* args[] = { L"sh", L"\\mnt\\fs0\\boot.sh"};
    status[4] = CMDsh(2,args);
    if(status[4]==EFI_NOT_FOUND)
        CPrint(ActualConfig.Theme.Warning,L"Boot script not found \n");
    else if(EFI_ERROR(status[4]))
        CPrint(ActualConfig.Theme.Error,L"Error while loading boot script : %r\n",status[4]);
    else 
        CPrint(ActualConfig.Theme.Sucess,L"Boot script loaded successfuly !\n");
    while(1){
        CHAR16* cmd = WaitForCommand();
        if(cmd){
            RunCMD(cmd);
            kfree(cmd);
        }
    }
    return EFI_SUCCESS;
}
