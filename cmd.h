#pragma once

#include <efi.h>
#include <efilib.h>

typedef struct {
    CHAR16 *name;
    EFI_STATUS (*func)(UINTN argc, CHAR16** argv);
    CHAR16 *description;
} COMMAND;

typedef struct {
    CHAR16* Buffer;
    UINTN   CurrentSize;   // Taille allouée actuelle
    UINTN   UsedSize;      // Nombre de caractères réellement utilisés
} NANO_LINE;



extern COMMAND Commands[];
extern UINTN CMD_COUNT;

EFI_STATUS CMDecho(UINTN argc, CHAR16** argv);
EFI_STATUS CMDhelp(UINTN argc, CHAR16** argv);
EFI_STATUS CMDpower(UINTN argc, CHAR16** argv);
EFI_STATUS CMDtime(UINTN argc, CHAR16** argv);
EFI_STATUS CMDclear(UINTN argc, CHAR16** argv);
EFI_STATUS CMDexit(UINTN argc, CHAR16** argv);
EFI_STATUS CMDexc(UINTN argc, CHAR16** argv);
EFI_STATUS GetCurrentPathString(CHAR16* OutBuffer, UINTN MaxLength);
EFI_STATUS CMDls(UINTN argc, CHAR16** argv);
EFI_STATUS CMDcd(UINTN argc, CHAR16** argv);
EFI_STATUS CMDpwd(UINTN argc, CHAR16** argv);
EFI_STATUS CMDmkdir(UINTN argc, CHAR16** argv);
EFI_STATUS CMDrm(UINTN argc, CHAR16** argv);
EFI_STATUS CMDcp(UINTN argc, CHAR16** argv);
EFI_STATUS CMDdd(UINTN argc, CHAR16** argv);
EFI_STATUS CMDcat(UINTN argc, CHAR16** argv);
EFI_STATUS CMDhexdump(UINTN argc, CHAR16** argv);
EFI_STATUS CMDnano(UINTN argc, CHAR16** argv);
EFI_STATUS CMDvol(UINTN argc, CHAR16** argv);
EFI_STATUS CMDtest(UINTN argc, CHAR16** argv);
EFI_STATUS CMDcheckargs(UINTN argc, CHAR16** argv);
EFI_STATUS CMDconfig(UINTN argc, CHAR16** argv);
EFI_STATUS CMDlistres(UINTN argc, CHAR16** argv);
EFI_STATUS CMDsetres(UINTN argc, CHAR16** argv);
EFI_STATUS CMDsh(UINTN argc, CHAR16** argv);
EFI_STATUS CMDloadcfg(UINTN argc, CHAR16** argv);
EFI_STATUS CMDresetcfg(UINTN argc, CHAR16** argv);
EFI_STATUS CMDreloadcfg(UINTN argc, CHAR16** argv);
EFI_STATUS CMDimg(UINTN argc, CHAR16** argv);
EFI_STATUS CMDraminfo(UINTN argc, CHAR16** argv);
EFI_STATUS CMDdownload(UINTN argc, CHAR16** argv);
EFI_STATUS CMDnetdetails(UINTN argc, CHAR16** argv);
EFI_STATUS CMDnslookup(UINTN argc, CHAR16** argv);
EFI_STATUS CMDsetfont(UINTN argc, CHAR16** argv);