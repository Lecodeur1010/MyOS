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

#define CHECK_STATUS(src, msg, forceRender, action, ...) do { \
    EFI_STATUS _s = (src); \
    if (EFI_ERROR(_s)) { \
        if (msg) { \
            if (!forceRender) ShellPrint(ActualConfig.Theme.Error, msg, ##__VA_ARGS__); \
            else CPrint(ActualConfig.Theme.Error, msg, ##__VA_ARGS__); \
        } \
        action; \
        return _s; \
    } \
} while(0)

#define NOP (void)0

#define UTF8_MARGIN 3

#if defined(__GNUC__) || defined(__clang__)
static __inline__ UINT64 ReadTSC(VOID) {
    UINT32 low, high;
    __asm__ __volatile__ ("rdtsc" : "=a" (low), "=d" (high));
    return ((UINT64)high << 32) | low;
}
#elif defined(_MSC_VER)
#include <intrin.h>
#define ReadTSC __rdtsc
#endif

UINTN StrToHex(CONST CHAR16* Str);
UINTN HexToStr(UINTN Value, CHAR16* Dest, UINTN MaxLength, BOOLEAN IncludePrefix, UINTN MinWidth);
EFI_STATUS LoadCFG(CONST CHAR16* Path);
void GetConfigValue(CHAR16* Line, CHAR16** Key, CHAR16** Value);
UINTN Char16ToChar8(CONST CHAR16* Src, CHAR8** Dest);
UINTN Char8ToChar16(CONST CHAR8* Src, CHAR16** Dest);
UINTN AsciiSPrint(CHAR8 *Buffer, UINTN BufferSize, CONST CHAR8 *Format, ...);
VOID FormatBytes(UINTN bytes, CHAR16* out_str) ;

EFI_STATUS GeneralInit();
EFI_STATUS SetKeyboardLeds(UINT8 mode);
UINT64 RandValue();
UINT64 GetTSCFrequencyPerMs(VOID);