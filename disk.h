#pragma once

#include <efilib.h>
#include <efi.h>

typedef struct {
    EFI_HANDLE Handle;
    EFI_FILE_SYSTEM_INFO* Info;
    CHAR16 Tag[16];
    UINTN RefCount;
} VOLUME;

typedef struct _FS_NODE {
    CHAR16 Name[256];
    EFI_FILE_PROTOCOL* EfiFile; // NULL si virtuel (\mnt)
    VOLUME* Volume;
    BOOLEAN IsDirectory;
    struct _FS_NODE* Parent;
    UINTN RefCount;
    UINTN Size;
    UINTN UID; //General purpose UID
    EFI_STATUS (*Open) (struct _FS_NODE* Parent, CONST CHAR16* Path, struct _FS_NODE** OutNode, UINT64 Mode, UINT64 Attributes);
    EFI_STATUS (*Read) (struct _FS_NODE* Node, VOID** Buffer, UINTN* Size);
    EFI_STATUS (*Write)(struct _FS_NODE* Node, CONST VOID* Buffer, UINTN Size, BOOLEAN Append);
    EFI_STATUS (*List) (struct _FS_NODE* Node, EFI_FILE_INFO*** Content, UINTN* Count);
    EFI_STATUS (*Close) (struct _FS_NODE* Node);
    EFI_STATUS (*Delete) (struct _FS_NODE* Node);
} FS_NODE;

typedef enum {
    UID_ROOT,
    UID_TTY,
    UID_PROMPT,
    UID_COLOR,
    UID_COLOR_BG,
    UID_COLOR_INFO,
    UID_COLOR_ERROR,
    UID_COLOR_WARNING,
    UID_COLOR_SUCESS,
    UID_COLOR_PROMPT,
    UID_COLOR_LS_FILE,
    UID_COLOR_LS_FOLDER,
    UID_SCREEN,
    UID_SCREEN_WIDTH,
    UID_SCREEN_HEIGHT
} FILE_UID;

typedef struct _DEV_NODE {
    CHAR16* Name;//Should be enough
    UINTN PersonalData;// ID reserved for the Read/Open
    EFI_STATUS (*Open) (struct _FS_NODE* Parent, CONST CHAR16* Path, struct _FS_NODE** OutNode, UINT64 Mode, UINT64 Attributes);
    EFI_STATUS (*Read) (struct _FS_NODE* Node, VOID** Buffer, UINTN* Size);
    EFI_STATUS (*Write)(struct _FS_NODE* Node, CONST VOID* Buffer, UINTN Size, BOOLEAN Append);
    //Close use the dafault Close (just need to properly delete the node)
    //Delete forbidden
    UINTN Size;
} DEV_NODE;

extern FS_NODE* ActualNode;
extern FS_NODE* RootNode;
extern VOLUME* Volumes;
extern UINTN VolumesCount;
extern EFI_HANDLE RootHandle;

FS_NODE *CreateFSNode(FS_NODE *Parent, const CHAR16 *Name, BOOLEAN IsDirectory);
EFI_STATUS ListVolume(VOLUME **List, UINTN *Count);
EFI_STATUS GetNextPart(const CHAR16 *Path, CHAR16 **Out);
CHAR16 *GetRemainingPath(const CHAR16 *Path);

EFI_STATUS VFSOpen(FS_NODE *Parent, const CHAR16 *Path, FS_NODE **OutNode, UINT64 Mode, UINT64 Attributes);
EFI_STATUS VFSList(FS_NODE *Node, EFI_FILE_INFO ***Content, UINTN *Count);
EFI_STATUS VFSRead(FS_NODE *Node, void **Buffer, UINTN *Size);

EFI_STATUS MNTOpen(FS_NODE *Parent, const CHAR16 *Path, FS_NODE **OutNode, UINT64 Mode, UINT64 Attributes);
EFI_STATUS MNTList(FS_NODE *Node, EFI_FILE_INFO ***Content, UINTN *Count);

EFI_STATUS Close(FS_NODE *Node);

EFI_STATUS FSOpen(FS_NODE *Parent, const CHAR16 *Path, FS_NODE **OutNode, UINT64 Mode, UINT64 Attributes);
EFI_STATUS FSList(FS_NODE *Node, EFI_FILE_INFO ***Content, UINTN *Count);
EFI_STATUS FSRead(FS_NODE *Node, void **Buffer, UINTN *Size);
EFI_STATUS FSWrite(FS_NODE *Node, const void *Buffer, UINTN Size, BOOLEAN Append);
EFI_STATUS FSClose(FS_NODE *Node);
EFI_STATUS FSDelete(FS_NODE *Node);

EFI_STATUS RootOpen(FS_NODE *Parent, const CHAR16 *Element, FS_NODE **OutNode, UINT64 Mode, UINT64 Attributes);
EFI_STATUS RootList(FS_NODE *Node, EFI_FILE_INFO ***Content, UINTN *Count);

EFI_STATUS DevOpen(FS_NODE *Parent, const CHAR16 *Path, FS_NODE **OutNode, UINT64 Mode, UINT64 Attributes);
EFI_STATUS DevList(FS_NODE *Node, EFI_FILE_INFO ***Content, UINTN *Count);

EFI_STATUS Forbidden(FS_NODE *Node, ...);

EFI_STATUS TTYRead(FS_NODE* Node, VOID** Buffer, UINTN* Size);
EFI_STATUS TTYWrite(FS_NODE* Node, CONST VOID* Buffer, UINTN Size, BOOLEAN Append);
EFI_STATUS PromptRead(FS_NODE* Node, VOID** Buffer, UINTN* Size);
EFI_STATUS PromptWrite(FS_NODE* Node, CONST VOID* Buffer, UINTN Size, BOOLEAN Append);
EFI_STATUS ColorRead(FS_NODE* Node, VOID** Buffer, UINTN* Size);
EFI_STATUS ColorWrite(FS_NODE* Node, CONST VOID* Buffer, UINTN Size, BOOLEAN Append);
EFI_STATUS ScreenRead(FS_NODE* Node, VOID** Buffer, UINTN* Size);
EFI_STATUS ScreenWrite(FS_NODE* Node, CONST VOID* Buffer, UINTN Size, BOOLEAN Append);

EFI_STATUS GetBootVolumeHandle(EFI_HANDLE *OutDeviceHandle);
EFI_STATUS DiskInit(void);