#pragma once

#include <efi.h>
#include <efilib.h>

EFI_INPUT_KEY WaitForInput();
EFI_INPUT_KEY QwertyToAzerty(EFI_INPUT_KEY key);

extern EFI_HANDLE gImageHandle;
extern EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *InputEx;


typedef struct {
    CHAR16 Normal;  // Caractère AZERTY normal
    CHAR16 Shift;   // Caractère AZERTY avec Majuscule
    CHAR16 AltGr;   // Caractère AZERTY avec AltGr
} AZERTY_ENTRY;

typedef struct {
    struct {   
        UINT32 Info ;
        UINT32 File ;
        UINT32 Folder ;
        UINT32 Prompt ;
        UINT32 Warning ;
        UINT32 Error ;
        UINT32 Sucess ;
        UINT32 Background;
    } Theme;
    CHAR16 Prompt[32];
} CONFIG;

extern CONFIG ActualConfig;
extern CONFIG BackupConfig;

#define CHECK_STATUS(status) do {if(EFI_ERROR(status)) return status;} while(0)

void* kmalloc(UINTN Size);
void kfree(void* pointer);

UINTN StrToHex(CONST CHAR16* Str);
EFI_STATUS LoadCFG(CONST CHAR16* Path);
void GetConfigValue(CHAR16* Line, CHAR16** Key, CHAR16** Value);
UINTN Char16ToChar8(CONST CHAR16* Src, CHAR8* Dest, UINTN MaxLenght);
UINTN Char8ToChar16(CONST CHAR8* Src, CHAR16* Dest, UINTN MaxLenght);

EFI_STATUS GeneralInit();
EFI_STATUS SetKeyboardLeds(UINT8 mode);
CHAR16* StrStr(CONST CHAR16* Str, CONST CHAR16* Search);