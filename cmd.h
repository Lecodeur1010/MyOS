#include <efi.h>
#include <efilib.h>

#ifndef CMD
#define CMD

typedef struct {
    CHAR16 *name;
    EFI_STATUS (*func)(CHAR16 *args);
    CHAR16 *description;
} COMMAND;

typedef struct {
    EFI_HANDLE Handle;
    EFI_FILE_PROTOCOL *Root;
    CHAR16 Label[64];
} VOLUME;



extern COMMAND Commands[];
extern UINTN CMD_COUNT;
extern CHAR16* WorkingDir;

EFI_STATUS CMDhelp(CHAR16* Args);
EFI_STATUS CMDpower(CHAR16* Args);
EFI_STATUS CMDtime(CHAR16* Args);
EFI_STATUS CMDclear(CHAR16* Args);
EFI_STATUS CMDexit(CHAR16* Args);
EFI_STATUS CMDexc(CHAR16* Args);
CHAR16* GetPrompt();
EFI_STATUS CMDls(CHAR16* Args);
EFI_STATUS CMDcd(CHAR16* Args);
EFI_STATUS CMDpwd(CHAR16* Args);
EFI_STATUS CMDmkdir(CHAR16* Args);
EFI_STATUS CMDrm(CHAR16* Args);
EFI_STATUS CMDmap(CHAR16 *Args);
EFI_STATUS CMDvol(CHAR16* Args);
void Init(EFI_HANDLE ImageHandle);
#endif