#include <efi.h>
#include <efilib.h>
#include "disk.h"
#include "func.h"
#include "display.h"

#define SET_STUB(node_func_ptr, stub_func) (node_func_ptr) = (__typeof__(node_func_ptr))(stub_func)
#define INCREMENT_REF_COUNT(node) do{FS_NODE* TempNode = (node);while(TempNode->Parent!=NULL){TempNode->RefCount++;TempNode=TempNode->Parent;}} while(0)



VOLUME* Volumes = NULL;
UINTN VolumesCount = 0;
UINT8* VolumesInfo = NULL;
FS_NODE* RootNode;
FS_NODE* ActualNode;


DEV_NODE DevListColor[] = {
    { L"bg",          UID_COLOR_BG,        NULL, ColorRead, ColorWrite ,30},
    { L"info",        UID_COLOR_INFO,      NULL, ColorRead, ColorWrite ,30},
    { L"error",       UID_COLOR_ERROR,     NULL, ColorRead, ColorWrite ,30},
    { L"warning",     UID_COLOR_WARNING,   NULL, ColorRead, ColorWrite ,30},
    { L"success",     UID_COLOR_SUCESS,    NULL, ColorRead, ColorWrite ,30},
    { L"prompt",      UID_COLOR_PROMPT,    NULL, ColorRead, ColorWrite ,30},
    { L"ls_file",     UID_COLOR_LS_FILE,   NULL, ColorRead, ColorWrite ,30},
    { L"ls_folder",   UID_COLOR_LS_FOLDER, NULL, ColorRead, ColorWrite ,30},
};

DEV_NODE DevListScreen[] ={
    {L"width",UID_SCREEN_WIDTH,NULL,ScreenRead,ScreenWrite, 10},//If it's over 9999 it's okay to have an overflow
    {L"height",UID_SCREEN_HEIGHT,NULL,ScreenRead,ScreenWrite, 10},
};

DEV_NODE DevListRoot[] ={
    {L"tty",UID_TTY,NULL,TTYRead,TTYWrite,4},
    {L"prompt",UID_PROMPT,NULL,PromptRead,PromptWrite,32},
    {L"screen",UID_SCREEN,DevOpen,NULL,NULL},
    {L"color",UID_COLOR,DevOpen,NULL,NULL},
};





FS_NODE* CreateFSNode(FS_NODE* Parent, CONST CHAR16* Name, BOOLEAN IsDirectory) {
    FS_NODE* Node = kmalloc(sizeof(FS_NODE));
    if (!Node) return NULL;
    
    SetMem(Node, sizeof(FS_NODE), 0);
    
    if (Name) StrCpy(Node->Name, Name);
    Node->Parent = Parent;
    Node->IsDirectory = IsDirectory;
    Node->RefCount = 0;
    Node->UID=0;
    Node->Size = 0;
    Node->Volume=NULL;
    Node->EfiFile=NULL;
    SET_STUB(Node->Open, Forbidden);
    SET_STUB(Node->Close, Forbidden);
    SET_STUB(Node->List, Forbidden);
    SET_STUB(Node->Read, Forbidden);
    SET_STUB(Node->Write, Forbidden);
    SET_STUB(Node->Delete, Forbidden);
    return Node;
}

EFI_STATUS ListVolume(VOLUME** List, UINTN* Count) {
    EFI_STATUS status;
    EFI_HANDLE* HandleBuffer = NULL;
    status = uefi_call_wrapper(gBS->LocateHandleBuffer, 5, ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, Count, &HandleBuffer);
    if (EFI_ERROR(status)) return status;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;
    EFI_FILE_PROTOCOL* tmpRoot;
    UINTN TotalSize = 0;
    for (UINTN i = 0; i < *Count; i++) {
        status = uefi_call_wrapper(gBS->HandleProtocol, 3, HandleBuffer[i], &gEfiSimpleFileSystemProtocolGuid, (VOID*)&fs);
        if (EFI_ERROR(status)) continue;
        status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &tmpRoot);
        if (EFI_ERROR(status)) continue;
        UINTN Size = 0;
        status = uefi_call_wrapper(tmpRoot->GetInfo, 4, tmpRoot, &gEfiFileSystemInfoGuid, &Size, NULL);
        if (status == EFI_BUFFER_TOO_SMALL) {
            TotalSize += Size;
        }
        uefi_call_wrapper(tmpRoot->Close, 1, tmpRoot);
    }
    if (Volumes) kfree(Volumes);
    if (VolumesInfo) kfree(VolumesInfo);
    VolumesInfo = kmalloc(TotalSize);
    Volumes = kmalloc((*Count) * sizeof(VOLUME));
    if (!VolumesInfo || !Volumes) return EFI_OUT_OF_RESOURCES;
    UINT8* VolumesInfoPtr = VolumesInfo;
    INTN RootIndex = -1;
    EFI_HANDLE RootHandle;
    UINTN cursor = 0;
    status = GetBootVolumeHandle(&RootHandle);
    if(!EFI_ERROR(status)){
        for(UINTN i = 0; i < *Count; i++) {
            if(HandleBuffer[i]==RootHandle){;
                RootIndex=i;
                status = uefi_call_wrapper(gBS->HandleProtocol, 3, HandleBuffer[i], &gEfiSimpleFileSystemProtocolGuid, (VOID*)&fs);
                if (EFI_ERROR(status)) continue;
                status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &tmpRoot);
                if (EFI_ERROR(status)) continue;
                Volumes[cursor].Handle = HandleBuffer[i];
                UINTN CurrentSize = TotalSize - (VolumesInfoPtr - VolumesInfo);
                status = uefi_call_wrapper(tmpRoot->GetInfo, 4, tmpRoot, &gEfiFileSystemInfoGuid, &CurrentSize, VolumesInfoPtr);
                CHAR16* ptr = PoolPrint(L"fs%d", cursor);
                if(ptr){
                    StrCpy(Volumes[cursor].Tag,ptr);
                    FreePool(ptr);
                } 
                if (!EFI_ERROR(status)) {
                    Volumes[cursor].Info = (EFI_FILE_SYSTEM_INFO*)VolumesInfoPtr;
                    VolumesInfoPtr += CurrentSize;
                }
                uefi_call_wrapper(tmpRoot->Close, 1, tmpRoot);
                cursor++;
                break;
            }
        }
    }
    for(UINTN i = 0; i < *Count; i++) {
        if(i==RootIndex) continue;
        status = uefi_call_wrapper(gBS->HandleProtocol, 3, HandleBuffer[i], &gEfiSimpleFileSystemProtocolGuid, (VOID*)&fs);
        if (EFI_ERROR(status)) continue;
        status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &tmpRoot);
        if (EFI_ERROR(status)) continue;
        Volumes[cursor].Handle = HandleBuffer[i];
        UINTN CurrentSize = TotalSize - (VolumesInfoPtr - VolumesInfo);
        status = uefi_call_wrapper(tmpRoot->GetInfo, 4, tmpRoot, &gEfiFileSystemInfoGuid, &CurrentSize, VolumesInfoPtr);
        CHAR16* ptr = PoolPrint(L"fs%d", cursor);
        if(ptr){
            StrCpy(Volumes[cursor].Tag,ptr);
            FreePool(ptr);
        } 
        if (!EFI_ERROR(status)) {
            Volumes[cursor].Info = (EFI_FILE_SYSTEM_INFO*)VolumesInfoPtr;
            VolumesInfoPtr += CurrentSize;
        }
        uefi_call_wrapper(tmpRoot->Close, 1, tmpRoot);
        cursor++;
    }
    VolumesCount = *Count;
    uefi_call_wrapper(gBS->FreePool, 1, HandleBuffer); 
    if (List) *List = Volumes;
    return EFI_SUCCESS;
}

EFI_STATUS GetNextPart(CONST CHAR16* Path, CHAR16** Out) {
    if (!Path || !Out) return EFI_INVALID_PARAMETER;
    UINTN start = 0;
    while (Path[start] == L'\\') {
        start++;
    }
    UINTN len = 0;
    while (Path[start + len] != L'\0' && Path[start + len] != L'\\') {
        len++;
    }
    *Out = kmalloc((len + 1) * sizeof(CHAR16));
    if (!*Out) return EFI_OUT_OF_RESOURCES;
    CopyMem(*Out, &Path[start], len * sizeof(CHAR16));
    (*Out)[len] = L'\0'; 
    return EFI_SUCCESS;
}

CHAR16* GetRemainingPath(CONST CHAR16* Path) {
    if (!Path) return NULL;
    UINTN i = 0;
    while (Path[i] == L'\\') i++;
    while (Path[i] != L'\0' && Path[i] != L'\\') i++;
    while (Path[i] == L'\\') i++;
    return (CHAR16*)(Path + i);
}

EFI_STATUS VFSOpen(FS_NODE* Parent, CONST CHAR16* Path, FS_NODE** OutNode, UINT64 Mode, UINT64 Attributes) {
    if (!Path || !OutNode) return EFI_INVALID_PARAMETER;

    // 1. Détermination du point de départ (Absolu vs Relatif)
    FS_NODE* CurrentNode = (Path[0] == L'\\') ? RootNode : Parent;
    if (!CurrentNode) return EFI_INVALID_PARAMETER;

    // Si le chemin commence par un slash, on l'ignore pour le parsing
    if (Path[0] == L'\\') {
        Path++;
    }

    // Si le chemin est vide ou était juste "\", on retourne directement le point de départ
    if (StrLen(Path) == 0) {
        INCREMENT_REF_COUNT(CurrentNode);
        *OutNode = CurrentNode;
        return EFI_SUCCESS;
    }

    // 2. Allocation unique du buffer de travail
    CHAR16* PathCopy = StrDuplicate(Path);
    if (!PathCopy) return EFI_OUT_OF_RESOURCES;

    // 'Remaining' est un pointeur mobile qui va glisser le long de 'PathCopy'
    CHAR16* Remaining = PathCopy; 
    CHAR16* Element = NULL;
    EFI_STATUS status = EFI_SUCCESS;
    // 3. Boucle "Saute-Mouton"
    while (StrLen(Remaining) > 0) {
        // Extrait le morceau actuel (alloue probablement 'Element')
        status = GetNextPart(Remaining, &Element);
        if (EFI_ERROR(status)) {
            kfree(Element);
            break;
        }
        if (StrLen(Element) == 0) {
            kfree(Element);
            Remaining = GetRemainingPath(Remaining); // Glisse au suivant
            continue;
        }
        Remaining = GetRemainingPath(Remaining);
        if (StrCmp(Element, L".") == 0) {
            kfree(Element);
            continue;
        }
        else if (StrCmp(Element, L"..") == 0) {
            kfree(Element);
            if (CurrentNode->Parent != NULL) {
                CurrentNode = CurrentNode->Parent;
            }
            continue;
        } 
        
        else {
            if (!CurrentNode->IsDirectory) {
                kfree(Element);
                status = EFI_NOT_FOUND;
                break;
            }

            FS_NODE* NextNode = NULL;
            status = CurrentNode->Open(CurrentNode, Element, &NextNode, Mode, Attributes);
            kfree(Element); // Libération immédiate de la copie de l'élément

            if (EFI_ERROR(status)) {
                break; // Erreur d'ouverture (ex: fichier introuvable)
            }

            // On avance vers le nœud suivant
            CurrentNode = NextNode; 
        }
    }

    // 4. Nettoyage unique de la mémoire allouée au départ
    kfree(PathCopy);

    // 5. Conclusion
    if (EFI_ERROR(status)) {
        return status;
    }
    INCREMENT_REF_COUNT(CurrentNode);
    *OutNode = CurrentNode;
    return EFI_SUCCESS;
}

EFI_STATUS VFSList(FS_NODE* Node, EFI_FILE_INFO*** Content, UINTN* Count) {
    if (!Node || !Content || !Count) return EFI_INVALID_PARAMETER;
    if (!Node->IsDirectory) return EFI_UNSUPPORTED;
    if (Node->List == NULL || Node->List == (__typeof__(Node->List))Forbidden) return EFI_ACCESS_DENIED; 
    return Node->List(Node, Content, Count);
}

EFI_STATUS VFSRead(FS_NODE* Node, VOID** Buffer, UINTN* Size){
    if (!Node || !Buffer || !Size) return EFI_INVALID_PARAMETER;
    if (Node->IsDirectory) return EFI_UNSUPPORTED;
    if(*Size==0){
        if(Node->Size==(UINTN)(-1)) return EFI_UNSUPPORTED;
        *Size = Node->Size;
    };
    *Buffer=kmalloc(*Size+1);
    CHAR8* temp = (CHAR8*)*Buffer;
    temp[*Size] = 0;
    return Node->Read(Node,Buffer,Size);
}

EFI_STATUS MNTOpen(FS_NODE* Parent, CONST CHAR16* Path, FS_NODE** OutNode, UINT64 Mode, UINT64 Attributes){
    if(!Parent||!Path) return EFI_INVALID_PARAMETER;
    EFI_STATUS status;
    for(UINTN i = 0; i < VolumesCount; i++){
        if(StrCmp(Path,Volumes[i].Tag)==0){
            (*OutNode)=CreateFSNode(Parent,Volumes[i].Tag,TRUE);
            (*OutNode)->Open=FSOpen;
            (*OutNode)->Close=FSClose;
            (*OutNode)->List=FSList;
            (*OutNode)->Volume=&(Volumes[i]);
            EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* VolumeFS = NULL;
            status = uefi_call_wrapper(BS->HandleProtocol, 3, Volumes[i].Handle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&VolumeFS);
            status = uefi_call_wrapper(VolumeFS->OpenVolume, 2, VolumeFS, &((*OutNode)->EfiFile));
            CHECK_STATUS(status);
            return EFI_SUCCESS;
        };

    }
    return Attributes&EFI_FILE_MODE_CREATE ? EFI_ACCESS_DENIED : EFI_NOT_FOUND;
}

EFI_STATUS MNTList(FS_NODE* Node, EFI_FILE_INFO*** Content, UINTN* Count){
    *Content = kmalloc(VolumesCount*sizeof(EFI_FILE_INFO*));
    *Count = VolumesCount;
    for(UINTN i = 0; i < VolumesCount; i++){
        UINTN TagLen = StrLen(Volumes[i].Tag) + 1;
        (*Content)[i]=kmalloc(sizeof(EFI_FILE_INFO)-sizeof(CHAR16)+TagLen*sizeof(CHAR16));
        if(!Content[i]) return EFI_OUT_OF_RESOURCES;
        (*Content)[i]->Size = sizeof(EFI_FILE_INFO)-sizeof(CHAR16)+TagLen*sizeof(CHAR16);
        (*Content)[i]->FileSize=Volumes[i].Info->VolumeSize;
        StrCpy((*Content)[i]->FileName,Volumes[i].Tag);
    }
    return EFI_SUCCESS;
}

EFI_STATUS Close(FS_NODE* Node){
    if(!Node) return EFI_INVALID_PARAMETER;
    Node->RefCount--;
    FS_NODE* ParentToClose = Node->Parent;
    EFI_STATUS status = EFI_SUCCESS;
    if(ParentToClose != NULL) {
        ParentToClose->Close(ParentToClose);
    }
    if(Node->RefCount == 0 && Node->Parent != NULL){
        kfree(Node);
    }
    return status;
}

EFI_STATUS FSOpen(FS_NODE* Parent, CONST CHAR16* Path, FS_NODE** OutNode, UINT64 Mode, UINT64 Attributes){ 
    EFI_FILE_PROTOCOL* NewFile;
    EFI_STATUS status = uefi_call_wrapper(Parent->EfiFile->Open,5,Parent->EfiFile,&NewFile,Path, Mode,Attributes);
    CHECK_STATUS(status);
    FS_NODE* NewNode = CreateFSNode(Parent,Path,FALSE);
    if(!NewNode){
        uefi_call_wrapper(NewFile->Close,1,NewFile);
        return EFI_OUT_OF_RESOURCES;
    }
    NewNode->Volume=Parent->Volume;
    NewNode->Read=FSRead;
    NewNode->Write=FSWrite;
    NewNode->Open=FSOpen;
    NewNode->Close=FSClose;
    NewNode->Delete=FSDelete;
    NewNode->List=FSList;
    NewNode->EfiFile=NewFile;
    UINTN Size = 0;
    status = uefi_call_wrapper(NewFile->GetInfo,4,NewFile,&gEfiFileInfoGuid,&Size,NULL);
    if(status == EFI_BUFFER_TOO_SMALL){
        EFI_FILE_INFO* Info = kmalloc(Size);
        if(!Info){
            uefi_call_wrapper(NewFile->Close,1,NewFile);
            kfree(NewNode);
            return EFI_OUT_OF_RESOURCES;
        }
        status = uefi_call_wrapper(NewFile->GetInfo,4,NewFile,&gEfiFileInfoGuid,&Size,Info);
        if (EFI_ERROR(status)) {
            kfree(Info);
            uefi_call_wrapper(NewFile->Close, 1, NewFile);
            NewNode->Close(NewNode);
            return status;
        }
        if(Info->Attribute & EFI_FILE_DIRECTORY) NewNode->IsDirectory=TRUE;
        else NewNode->IsDirectory=FALSE;
        NewNode->Size=Info->FileSize;
        kfree(Info);
    }
    *OutNode = NewNode;
    return EFI_SUCCESS;
        
}

EFI_STATUS FSList(FS_NODE* Node, EFI_FILE_INFO*** Content, UINTN* Count)
{
    EFI_FILE_PROTOCOL* tmpDir = NULL;
    EFI_STATUS status;
    tmpDir = Node->EfiFile;
    status = uefi_call_wrapper(tmpDir->SetPosition,2,tmpDir,0);
    CHECK_STATUS(status);
    UINT8* ReadBuffer = kmalloc(1024);
    if(!ReadBuffer) return EFI_OUT_OF_RESOURCES;
    *Count = 0; UINTN Capacity = 16, Size = 1024;
    EFI_FILE_INFO** Array = kmalloc(Capacity*sizeof(EFI_FILE_INFO*));
    while(1){
        Size = 1024;
        status = uefi_call_wrapper(tmpDir->Read,3,tmpDir,&Size,ReadBuffer);
        if (EFI_ERROR(status) || Size == 0) {
            break;
        }
        EFI_FILE_INFO* CurrentEntry = (EFI_FILE_INFO*)ReadBuffer;
        if (StrCmp(CurrentEntry->FileName, L".") == 0 || StrCmp(CurrentEntry->FileName, L"..") == 0) {
            continue;
        }
        if(*Count>=Capacity){
            Capacity*=2;
            EFI_FILE_INFO** NewArray = kmalloc(Capacity * sizeof(EFI_FILE_INFO*));
            if(!NewArray){
                for (UINTN j = 0; j < *Count; j++) kfree(Array[j]);
                kfree(Array);
                kfree(ReadBuffer);
                return EFI_OUT_OF_RESOURCES;
            }
            CopyMem(NewArray, Array, *Count * sizeof(EFI_FILE_INFO*));
            kfree(Array);
            Array = NewArray;
        }
        UINTN EntrySize = CurrentEntry->Size; 
        Array[*Count] = kmalloc(EntrySize);
        if (!Array[*Count]) {
            for (UINTN j = 0; j < *Count; j++) kfree(Array[j]);
            kfree(Array);
            kfree(ReadBuffer);
            return EFI_OUT_OF_RESOURCES;
        }
        CopyMem(Array[*Count], CurrentEntry, EntrySize);
        (*Count)++;
    }
    kfree(ReadBuffer);
    if (*Count == 0) {
        kfree(Array);
        *Content = NULL;
    } else {
        *Content = Array;
    }
    return EFI_SUCCESS;
}

EFI_STATUS FSRead(FS_NODE* Node, VOID** Buffer, UINTN* Size)
{
    if (!Node->EfiFile)
        return EFI_INVALID_PARAMETER;
    EFI_FILE_PROTOCOL* file = Node->EfiFile;
    EFI_STATUS status;
    if(!(*Buffer)) return EFI_OUT_OF_RESOURCES;
    status = uefi_call_wrapper(file->Read,3,file,Size,*Buffer);
    return status;
}

EFI_STATUS FSWrite(FS_NODE* Node, CONST VOID* Buffer, UINTN Size, BOOLEAN Append) {
    if (!Node || !Buffer || !Node->EfiFile) return EFI_INVALID_PARAMETER;
    EFI_FILE_PROTOCOL* file = Node->EfiFile;
    
    // 1. Positionnement
    uefi_call_wrapper(file->SetPosition, 2, file, (Append ? 0xFFFFFFFFFFFFFFFF : 0));
    
    // 2. Écriture
    UINTN WrittenSize = Size;
    EFI_STATUS status = uefi_call_wrapper(file->Write, 3, file, &WrittenSize, (VOID*)Buffer);
    if (EFI_ERROR(status)) return status;

    // 3. SEULEMENT SI ON N'EST PAS EN APPEND, ON TRONQUE
    if (!Append) {
        UINTN InfoSize = SIZE_OF_EFI_FILE_INFO + 256;
        EFI_FILE_INFO* Info = kmalloc(InfoSize);
        if (Info) {
            status = uefi_call_wrapper(file->GetInfo, 4, file, &gEfiFileInfoGuid, &InfoSize, Info);
            if (!EFI_ERROR(status)) {
                Info->FileSize = WrittenSize; // On force la taille à la taille écrite
                uefi_call_wrapper(file->SetInfo, 4, file, &gEfiFileInfoGuid, InfoSize, Info);
            }
            kfree(Info);
        }
    }
    // Si c'est Append, on ne touche à rien, UEFI gère la croissance tout seul !

    uefi_call_wrapper(file->Flush, 1, file);
    return status;
}

EFI_STATUS FSClose(FS_NODE* Node){
    if(!Node) return EFI_INVALID_PARAMETER;
    if(Node->Parent)Node->RefCount--;
    FS_NODE* ParentToClose = Node->Parent;
    EFI_STATUS status = EFI_SUCCESS;
    if(ParentToClose != NULL && ParentToClose->Parent != NULL) {
        ParentToClose->Close(ParentToClose);
    }
    if(Node->RefCount == 0 && Node->Parent != NULL){
        if(Node->EfiFile) {
            status = uefi_call_wrapper(Node->EfiFile->Close, 1, Node->EfiFile);
        }
        kfree(Node);
    }
    return status;
}

EFI_STATUS FSDelete (FS_NODE* Node){
    if(!Node||!Node->EfiFile) return EFI_INVALID_PARAMETER;
    FS_NODE* Parent = Node->Parent;
    EFI_STATUS status = uefi_call_wrapper(Parent->EfiFile->Delete, 1, Node->EfiFile);
    kfree(Node);
    return status; 
}

EFI_STATUS RootOpen(FS_NODE* Parent, CONST CHAR16* Element, FS_NODE** OutNode, UINT64 Mode, UINT64 Attributes) {
    if (!Parent || !Element || !OutNode) return EFI_INVALID_PARAMETER;

    if (StrCmp(Element, L"mnt") == 0) {
        *OutNode = CreateFSNode(Parent, L"mnt", TRUE);
        if (!*OutNode) return EFI_OUT_OF_RESOURCES;
        
        (*OutNode)->Open  = MNTOpen;
        (*OutNode)->Close = Close;
        (*OutNode)->List  = MNTList;
        return EFI_SUCCESS;
    } else if (StrCmp(Element, L"dev") == 0) {
        *OutNode = CreateFSNode(Parent, L"dev", TRUE);
        if (!*OutNode) return EFI_OUT_OF_RESOURCES;
        
        (*OutNode)->Open  = DevOpen;
        (*OutNode)->Close = Close;
        (*OutNode)->List  = DevList;
        return EFI_SUCCESS;
    }

    return Attributes&EFI_FILE_MODE_CREATE ? EFI_ACCESS_DENIED : EFI_NOT_FOUND;
}

EFI_STATUS RootList(FS_NODE* Node, EFI_FILE_INFO*** Content, UINTN* Count) {
    if (!Node || !Content || !Count) return EFI_INVALID_PARAMETER;
    
    CHAR16* Entries[] = {L"mnt",L"dev"};
    *Count=sizeof(Entries)/sizeof(Entries[0]);
    *Content = kmalloc(*Count*sizeof(EFI_FILE_INFO*));
    if (!*Content) return EFI_OUT_OF_RESOURCES;
    
    for (UINTN i = 0; i < *Count; i++) {
        UINTN NameLen = StrLen(Entries[i]) + 1;
        UINTN EntrySize = sizeof(EFI_FILE_INFO) - sizeof(CHAR16) + (NameLen * sizeof(CHAR16));
        
        (*Content)[i] = kmalloc(EntrySize);
        if (!(*Content)[i]) {
            for (UINTN j = 0; j < i; j++) {
                kfree((*Content)[j]);
            }
            kfree(*Content);
            *Content = NULL;
            *Count = 0;
            return EFI_OUT_OF_RESOURCES;
        }
        SetMem((*Content)[i], EntrySize, 0);
        (*Content)[i]->Size = EntrySize;
        (*Content)[i]->Attribute = EFI_FILE_DIRECTORY;
        (*Content)[i]->FileSize = 0;
        StrCpy((*Content)[i]->FileName, Entries[i]);
    }
    
    return EFI_SUCCESS;
}




EFI_STATUS DevOpen(FS_NODE* Parent, CONST CHAR16* Path, FS_NODE** OutNode, UINT64 Mode, UINT64 Attributes) {
    if (!Parent || !Path || !OutNode) return EFI_INVALID_PARAMETER;
    DEV_NODE* TargetList = NULL;
    UINTN TargetCount = 0;
    if (Parent->UID == UID_COLOR) {
        TargetList = DevListColor;
        TargetCount = sizeof(DevListColor) / sizeof(DevListColor[0]);
    } else if (Parent->UID == UID_SCREEN) {
        TargetList = DevListScreen;
        TargetCount = sizeof(DevListScreen) / sizeof(DevListScreen[0]);
    } else {
        TargetList = DevListRoot;
        TargetCount = sizeof(DevListRoot) / sizeof(DevListRoot[0]);
    }
    for (UINTN i = 0; i < TargetCount; i++) {
        if (StrCmp(Path, TargetList[i].Name) == 0) {
            BOOLEAN IsDir = (TargetList[i].Open != NULL);
            
            FS_NODE* NewNode = CreateFSNode(Parent, TargetList[i].Name, IsDir);
            if (!NewNode) return EFI_OUT_OF_RESOURCES;

            NewNode->UID = TargetList[i].PersonalData;
            NewNode->Size= TargetList[i].Size;
            if (IsDir) {
                NewNode->Open  = TargetList[i].Open; // DevOpen
                NewNode->List  = DevList;
                NewNode->Close = Close;
            } else {
                // C'est un fichier/noeud terminal de périphérique
                NewNode->Read  = TargetList[i].Read;
                NewNode->Write = TargetList[i].Write;
                NewNode->Close = Close;
            }

            *OutNode = NewNode;
            return EFI_SUCCESS;
        }
    }

    return Attributes & EFI_FILE_MODE_CREATE ? EFI_ACCESS_DENIED : EFI_NOT_FOUND;
}

EFI_STATUS DevList(FS_NODE* Node, EFI_FILE_INFO*** Content, UINTN* Count) {
    if (!Node || !Content || !Count) return EFI_INVALID_PARAMETER;
    DEV_NODE* TargetList = NULL;
    UINTN TargetCount = 0;
    if (Node->UID == UID_COLOR) {
        TargetList = DevListColor;
        TargetCount = sizeof(DevListColor) / sizeof(DevListColor[0]);
    } else if (Node->UID == UID_SCREEN) {
        TargetList = DevListScreen;
        TargetCount = sizeof(DevListScreen) / sizeof(DevListScreen[0]);
    } else {
        TargetList = DevListRoot;
        TargetCount = sizeof(DevListRoot) / sizeof(DevListRoot[0]);
    }
    *Count = TargetCount;
    *Content = kmalloc(TargetCount * sizeof(EFI_FILE_INFO*));
    if (!*Content) return EFI_OUT_OF_RESOURCES;

    for (UINTN i = 0; i < TargetCount; i++) {
        UINTN NameLen = StrLen(TargetList[i].Name) + 1;
        UINTN EntrySize = sizeof(EFI_FILE_INFO) - sizeof(CHAR16) + (NameLen * sizeof(CHAR16));
        (*Content)[i] = kmalloc(EntrySize);
        if (!(*Content)[i]) {
            for (UINTN j = 0; j < i; j++) kfree((*Content)[j]);
            kfree(*Content);
            *Content = NULL;
            *Count = 0;
            return EFI_OUT_OF_RESOURCES;
        }
        SetMem((*Content)[i], EntrySize, 0);
        (*Content)[i]->Size = EntrySize;
        // Si Open existe, c'est un sous-dossier, sinon c'est un fichier spécial de périphérique
        (*Content)[i]->Attribute = (TargetList[i].Open != NULL) ? EFI_FILE_DIRECTORY : 0;
        (*Content)[i]->FileSize = 0;
        StrCpy((*Content)[i]->FileName, TargetList[i].Name);
    }

    return EFI_SUCCESS;
}

EFI_STATUS TTYRead(FS_NODE* Node, VOID** Buffer, UINTN* Size){
    CHAR16* buf = (CHAR16*)(*Buffer);   
    buf[1]=L'\0';
    while (TRUE){
        EFI_INPUT_KEY key = WaitForInput();
        if(key.ScanCode==0x17){
            buf[0]=L'\0';
            return EFI_ABORTED;
        }
        if(key.UnicodeChar){
            buf[0]=key.UnicodeChar;
            return EFI_SUCCESS;
        }
    }
    
}
EFI_STATUS TTYWrite(FS_NODE* Node, CONST VOID* Buffer, UINTN Size, BOOLEAN Append){
    // Buffer est en CHAR16 (2 octets par caractère)
    CHAR16* Text = (CHAR16*)Buffer;
    UINTN NumChars = Size / sizeof(CHAR16);

    for(UINTN i = 0; i < NumChars; i++) {
        // On n'affiche que les caractères imprimables (au-dessus de 0x20)
        // ou les retours à la ligne/chariot.
        CHAR16* tmp =L" ";
        if (Text[i] >= 0x20 || Text[i] == L'\n' || Text[i] == L'\r') {
            tmp[0]=Text[i];
            CPrint(ActualConfig.Theme.Info,tmp);
        }
    }
    return EFI_SUCCESS;
}


EFI_STATUS ColorRead(FS_NODE* Node, VOID** Buffer, UINTN* Size){
    UINT32 GopColor;
    switch(Node->UID){
        case UID_COLOR_BG :
            GopColor=ActualConfig.Theme.Background;
            break;
        case UID_COLOR_INFO :
            GopColor=ActualConfig.Theme.Info;
            break;
        case UID_COLOR_ERROR :
            GopColor=ActualConfig.Theme.Error;
            break;
        case UID_COLOR_WARNING :
            GopColor=ActualConfig.Theme.Warning;
            break;
        case UID_COLOR_SUCESS :
            GopColor=ActualConfig.Theme.Sucess;
            break;
        case UID_COLOR_PROMPT :
            GopColor=ActualConfig.Theme.Prompt;
            break;
        case UID_COLOR_LS_FILE :
            GopColor=ActualConfig.Theme.File;
            break;
        case UID_COLOR_LS_FOLDER :
            GopColor=ActualConfig.Theme.Folder;
            break;
        default :
            return EFI_NOT_FOUND;
    }
    UINT8 Red = 0, Green = 0, Blue = 0;
    if (GopInfo->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
        // Le format est R | (G << 8) | (B << 16)
        Red   = (GopColor)       & 0xFF;
        Green = (GopColor >> 8)  & 0xFF;
        Blue  = (GopColor >> 16) & 0xFF;
    } 
    else if (GopInfo->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
        // Le format est B | (G << 8) | (R << 16)
        Blue  = (GopColor)       & 0xFF;
        Green = (GopColor >> 8)  & 0xFF;
        Red   = (GopColor >> 16) & 0xFF;
    }
    // On formate TOUJOURS en RRGGBB pour l'humain (ex: 0xFF0000 pour rouge)
    UnicodeSPrint(*Buffer, *Size, L"0x%02x%02x%02x", Red, Green, Blue);
    return EFI_SUCCESS;
}

BOOLEAN IsValidHexColor(CHAR16* str) {
    // 1. Vérifier la longueur totale : "0x" (2) + 6 chiffres = 8 caractères
    // On vérifie aussi que la chaîne ne soit pas trop longue (ex: si le fichier contient des retours à la ligne)
    if (StrLen(str) < 8) return FALSE;

    // 2. Vérifier le préfixe "0x"
    if (str[0] != L'0' || (str[1] != L'x' && str[1] != L'X')) return FALSE;

    // 3. Vérifier les 6 caractères suivants
    for (UINTN i = 2; i < 8; i++) {
        CHAR16 c = str[i];
        if (!((c >= L'0' && c <= L'9') || 
              (c >= L'A' && c <= L'F') || 
              (c >= L'a' && c <= L'f'))) {
            return FALSE;
        }
    }
    
    return TRUE;
}

EFI_STATUS ColorWrite(FS_NODE* Node, CONST VOID* Buffer, UINTN Size, BOOLEAN Append) {
    if(Size==0) return EFI_SUCCESS;
    UINT32* GopColor;
    switch(Node->UID){
        case UID_COLOR_BG :
            GopColor=&ActualConfig.Theme.Background;
            break;
        case UID_COLOR_INFO :
            GopColor=&ActualConfig.Theme.Info;
            break;
        case UID_COLOR_ERROR :
            GopColor=&ActualConfig.Theme.Error;
            break;
        case UID_COLOR_WARNING :
            GopColor=&ActualConfig.Theme.Warning;
            break;
        case UID_COLOR_SUCESS :
            GopColor=&ActualConfig.Theme.Sucess;
            break;
        case UID_COLOR_PROMPT :
            GopColor=&ActualConfig.Theme.Prompt;
            break;
        case UID_COLOR_LS_FILE :
            GopColor=&ActualConfig.Theme.File;
            break;
        case UID_COLOR_LS_FOLDER :
            GopColor=&ActualConfig.Theme.Folder;
            break;
        default :
            return EFI_NOT_FOUND;
    }
    
    CHAR16* SafeBuffer = kmalloc(Size + sizeof(CHAR16));
    CHAR16* tmp = (CHAR16*)Buffer;
    while(*tmp==L'\r'||*tmp==L'\n'||*tmp==L' ')tmp++;
    if(*tmp == L'\0'){kfree(SafeBuffer); return EFI_SUCCESS;};
    CopyMem(SafeBuffer, Buffer, Size);
    SafeBuffer[Size / sizeof(CHAR16)] = L'\0'; // Garantit la fin de chaîne !
    
    UINTN RawHex = StrToHex(SafeBuffer);

    if (!IsValidHexColor(SafeBuffer)) {
        Print(L"Format invalide. Utilise 0xRRGGBB. reçu : %s\n",SafeBuffer);
        kfree(SafeBuffer);
        return EFI_INVALID_PARAMETER;
    }

    UINT8 Red   = (RawHex >> 16) & 0xFF;
    UINT8 Green = (RawHex >> 8)  & 0xFF;
    UINT8 Blue  = (RawHex)       & 0xFF; 
    *GopColor = RGB(Red, Green, Blue);
    
    kfree(SafeBuffer);
    return EFI_SUCCESS; // N'oublie pas de retourner un statut !
}

EFI_STATUS PromptRead(FS_NODE* Node, VOID** Buffer, UINTN* Size){
    Char16ToChar8(ActualConfig.Prompt,*Buffer,0);
    *Size = 32;
    return EFI_SUCCESS;
}

EFI_STATUS PromptWrite(FS_NODE* Node, CONST VOID* Buffer, UINTN Size, BOOLEAN Append){
    if(Size==0) return EFI_SUCCESS;
    SetMem(ActualConfig.Prompt, 32 * sizeof(CHAR16), 0);
    UINTN WriteSize = (Size / sizeof(CHAR16)) > 31 ? 31 : (Size / sizeof(CHAR16));
    CopyMem(ActualConfig.Prompt,Buffer,WriteSize * sizeof(CHAR16));
    Node->Size=WriteSize * sizeof(CHAR16);

    return EFI_SUCCESS;
}



EFI_STATUS Forbidden(FS_NODE* Node, ...) {return EFI_UNSUPPORTED;}

EFI_STATUS GetBootVolumeHandle(EFI_HANDLE* OutDeviceHandle) {
    EFI_STATUS status;
    EFI_LOADED_IMAGE* LoadedImage;

    // 1. On demande à l'UEFI les infos sur l'image (notre binaire) en cours d'exécution
    status = uefi_call_wrapper(BS->OpenProtocol, 6,
        gImageHandle,
        &LoadedImageProtocol,
        (VOID**)&LoadedImage,
        gImageHandle,
        NULL,
        EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
    );
    if (EFI_ERROR(status)) return status;

    // 2. On extrait le DeviceHandle d'où provient notre binaire .efi
    *OutDeviceHandle = LoadedImage->DeviceHandle;

    // 3. On referme proprement le protocole
    uefi_call_wrapper(BS->CloseProtocol, 4, gImageHandle, &LoadedImageProtocol, gImageHandle, NULL);

    return EFI_SUCCESS;
}

EFI_STATUS DiskInit(){
    RootNode=CreateFSNode(NULL,L"",TRUE);
    if(!RootNode) return EFI_OUT_OF_RESOURCES;
    EFI_LOADED_IMAGE_PROTOCOL* LoadedImage = NULL;
    EFI_STATUS status = uefi_call_wrapper(BS->HandleProtocol, 3,gImageHandle, &gEfiLoadedImageProtocolGuid, (VOID**)&LoadedImage);
    status = ListVolume(&Volumes,&VolumesCount);
    if(VolumesCount==0||EFI_ERROR(status)||!Volumes) return EFI_ABORTED;
    RootNode->Volume=&(Volumes[0]);
    RootNode->List=RootList;
    RootNode->Open=RootOpen;
    RootNode->RefCount=1;
    ActualNode=RootNode;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* VolumeFS = NULL;
    status = uefi_call_wrapper(BS->HandleProtocol, 3, Volumes[0].Handle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&VolumeFS);
    status = uefi_call_wrapper(VolumeFS->OpenVolume, 2, VolumeFS, &RootNode->EfiFile);
    CHECK_STATUS(status);
    
    return EFI_SUCCESS;
}