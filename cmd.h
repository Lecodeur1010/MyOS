#include <efi.h>
#include <efilib.h>

#ifndef CMD
#define CMD

typedef struct {
    CHAR16 *name;
    EFI_STATUS (*func)(UINTN argc, CHAR16** argv);
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

CHAR16* GetPrompt();

EFI_STATUS CMDhelp(UINTN argc, CHAR16** argv);
EFI_STATUS CMDpower(UINTN argc, CHAR16** argv);
EFI_STATUS CMDtime(UINTN argc, CHAR16** argv);
EFI_STATUS CMDclear(UINTN argc, CHAR16** argv);
EFI_STATUS CMDexit(UINTN argc, CHAR16** argv);
EFI_STATUS CMDexc(UINTN argc, CHAR16** argv);
EFI_STATUS CMDls(UINTN argc, CHAR16** argv);
EFI_STATUS CMDcd(UINTN argc, CHAR16** argv);
EFI_STATUS CMDpwd(UINTN argc, CHAR16** argv);
EFI_STATUS CMDmkdir(UINTN argc, CHAR16** argv);
EFI_STATUS CMDrm(UINTN argc, CHAR16** argv);
EFI_STATUS CMDcp(UINTN argc, CHAR16** argv);
EFI_STATUS CMDcat(UINTN argc, CHAR16** argv);
EFI_STATUS CMDnano(UINTN argc, CHAR16** argv);
EFI_STATUS CMDmap(UINTN argc, CHAR16** argv);
EFI_STATUS CMDvol(UINTN argc, CHAR16** argv);
EFI_STATUS CMDtest(UINTN argc, CHAR16** argv);
EFI_STATUS CMDcheckargs(UINTN argc, CHAR16** argv);
EFI_STATUS CMDconfig(UINTN argc, CHAR16** argv);
EFI_STATUS CMDlistres(UINTN argc, CHAR16** argv);
EFI_STATUS CMDsetres(UINTN argc, CHAR16** argv);
void Init(EFI_HANDLE ImageHandle);
#endif