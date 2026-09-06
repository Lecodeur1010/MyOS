#include "func.h"
#include <efi.h>
#include <efilib.h>
#include "display.h"
#include "disk.h"
#include "graphics.h"
#include "memory.h"

EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *InputEx = NULL;
EFI_HANDLE gImageHandle;

CONFIG ActualConfig;
CONFIG BackupConfig;

AZERTY_ENTRY AzertyMap[128];

EFI_INPUT_KEY WaitForInput() {
    EFI_KEY_DATA KeyData;
    EFI_STATUS status;
    EFI_EVENT TimerEvent;
    EFI_EVENT Events[2];
    UINTN Index;

    uefi_call_wrapper(BS->CreateEvent, 5, EVT_TIMER, 0, NULL, NULL, &TimerEvent);
    uefi_call_wrapper(BS->SetTimer, 3, TimerEvent, TimerPeriodic, 5000000); // 500ms en unités de 100ns
    Events[0] = ST->ConIn->WaitForKey; // Événement clavier UEFI
    Events[1] = TimerEvent;
    while(1) {
        uefi_call_wrapper(BS->WaitForEvent, 3, 2, Events, &Index);
        if (Index == 0) { 
            break; 
        } else if (Index == 1) { 
            ToggleCursor(); 
        }
    }
    status = uefi_call_wrapper(InputEx->ReadKeyStrokeEx, 2, InputEx, &KeyData);
    
    if (EFI_ERROR(status)) {
        EFI_INPUT_KEY emptyKey = {0, 0};
        return emptyKey;
    }

    UINT32 state = KeyData.KeyState.KeyShiftState;
    BOOLEAN isAltGr = (state & EFI_RIGHT_ALT_PRESSED);
    BOOLEAN isShift = (state & (EFI_LEFT_SHIFT_PRESSED | EFI_RIGHT_SHIFT_PRESSED | EFI_CAPS_LOCK_ACTIVE));
    CHAR16 chr = KeyData.Key.UnicodeChar;

    if (KeyData.Key.ScanCode == 0x0015) {
        if (isShift) {
            KeyData.Key.UnicodeChar = AzertyMap[0x15].Shift;
        } else {
            KeyData.Key.UnicodeChar = AzertyMap[0x15].Normal;
        }
        // On remet le ScanCode à 0 pour éviter que le reste du Shell croit 
        // que l'utilisateur a appuyé sur la vraie touche F11 !
        KeyData.Key.ScanCode = 0; 
        return KeyData.Key;
    }
    
    if (chr > 0 && chr < 128) {
        // CAS 1 : AltGr est actif
        if (isAltGr) {
            if (AzertyMap[chr].AltGr != 0) {
                KeyData.Key.UnicodeChar = AzertyMap[chr].AltGr;
                return KeyData.Key;
            }
        }
        
        // CAS 2 : L'UEFI envoie une lettre MAJUSCULE (A-Z)
        if (chr >= L'A' && chr <= L'Z') {
            CHAR16 m_index = chr + (L'a' - L'A'); // On cherche la version minuscule dans l'index
            KeyData.Key.UnicodeChar = AzertyMap[m_index].Shift;
            return KeyData.Key;
        }
        
        // CAS 3 : L'UEFI envoie un symbole shifté du QWERTY (!, @, #, $, etc.)
        if (chr == L'!' || chr == L'@' || chr == L'#' || chr == L'$' || chr == L'%' ||
            chr == L'^' || chr == L'&' || chr == L'*' || chr == L'(' || chr == L')' ||
            chr == L'_' || chr == L'+' || chr == L'{' || chr == L'}' || chr == L'|' ||
            chr == L':' || chr == L'"' || chr == L'<' || chr == L'>' || chr == L'?' || chr == L'~') {
            
            KeyData.Key.UnicodeChar = AzertyMap[chr].Shift;
            return KeyData.Key;
        }
        
        // CAS 4 : Mode normal (Minuscules et symboles de base)
        KeyData.Key.UnicodeChar = AzertyMap[chr].Normal;
    }

    return KeyData.Key;
}

void InitAzertyMap() {
    // 1. Remplissage par défaut (Comportement QWERTY = AZERTY)
    for (int i = 0; i < 128; i++) {
        AzertyMap[i].Normal = (CHAR16)i;
        AzertyMap[i].AltGr  = 0;

        // Si c'est une lettre minuscule, sa version Shift par défaut est sa Majuscule
        if (i >= 'a' && i <= 'z') {
            AzertyMap[i].Shift = (CHAR16)(i - ('a' - 'A'));
        } else {
            AzertyMap[i].Shift = (CHAR16)i;
        }
    }
    AzertyMap[L'q'].Normal = L'a';
    AzertyMap[L'q'].Shift  = L'A';
    AzertyMap[L'Q']=AzertyMap[L'q'];
    AzertyMap[L'a'].Normal = L'q';
    AzertyMap[L'a'].Shift  = L'Q';
    AzertyMap[L'A']=AzertyMap[L'a'];
    AzertyMap[L'w'].Normal = L'z';
    AzertyMap[L'w'].Shift  = L'Z';
    AzertyMap[L'W']=AzertyMap[L'w'];
    AzertyMap[L'z'].Normal = L'w';
    AzertyMap[L'z'].Shift  = L'W';
    AzertyMap[L'Z']=AzertyMap[L'z'];
    AzertyMap[L'['].Normal  = L'^';
    AzertyMap[L'['].Shift = L'¨';
    AzertyMap[L'{']=AzertyMap[L'['];
    AzertyMap[L']'].Normal = L'$';
    AzertyMap[L']'].Shift  = L'£';
    AzertyMap[L']'].AltGr  = L'¤';
    AzertyMap[L'}']=AzertyMap[L']'];
    AzertyMap[L'\\'].Normal = L'*';
    AzertyMap[L'\\'].Shift  = L'µ';
    AzertyMap[L'|']=AzertyMap[L'\\'];
    AzertyMap[L';'].Normal = L'm';
    AzertyMap[L';'].Shift  = L'M';
    AzertyMap[L':']=AzertyMap[L';'];
    AzertyMap[L'\''].Normal = L'ù';
    AzertyMap[L'\''].Shift  = L'%';
    AzertyMap[L'"']=AzertyMap[L'\''];
    AzertyMap[L'/'].Normal = L'!';
    AzertyMap[L'/'].Shift  = L'§';
    AzertyMap[L'?']=AzertyMap[L'/'];
    AzertyMap[L'.'].Normal = L':';
    AzertyMap[L'.'].Shift  = L'/';
    AzertyMap[L'>']=AzertyMap[L'.'];
    AzertyMap[L','].Normal = L';';
    AzertyMap[L','].Shift  = L'.';
    AzertyMap[L'<']=AzertyMap[L','];
    AzertyMap[L'm'].Normal = L',';
    AzertyMap[L'm'].Shift  = L'?';
    AzertyMap[L'M']=AzertyMap[L'm'];
    AzertyMap[L'`'].Normal = L'<';
    AzertyMap[L'`'].Shift = L'>';
    AzertyMap[L'~']=AzertyMap[L'`'];
    AzertyMap[L'1'].Normal = L'&';
    AzertyMap[L'1'].Shift  = L'1';
    AzertyMap[L'1'].AltGr  = L'¹';
    AzertyMap[L'!']=AzertyMap[L'1'];
    AzertyMap[L'2'].Normal = L'é';
    AzertyMap[L'2'].Shift  = L'2';
    AzertyMap[L'2'].AltGr  = L'~';
    AzertyMap[L'@']=AzertyMap[L'2'];
    AzertyMap[L'3'].Normal = L'"';
    AzertyMap[L'3'].Shift  = L'3';
    AzertyMap[L'3'].AltGr  = L'#';
    AzertyMap[L'#']=AzertyMap[L'3'];
    AzertyMap[L'4'].Normal = L'\'';
    AzertyMap[L'4'].Shift  = L'4';
    AzertyMap[L'4'].AltGr  = L'{';
    AzertyMap[L'$']=AzertyMap[L'4'];
    AzertyMap[L'5'].Normal = L'(';
    AzertyMap[L'5'].Shift  = L'5';
    AzertyMap[L'5'].AltGr  = L'[';
    AzertyMap[L'%']=AzertyMap[L'5'];
    AzertyMap[L'6'].Normal = L'-';
    AzertyMap[L'6'].Shift  = L'6';
    AzertyMap[L'6'].AltGr  = L'|';
    AzertyMap[L'^']=AzertyMap[L'6'];
    AzertyMap[L'7'].Normal = L'è';
    AzertyMap[L'7'].Shift  = L'7';
    AzertyMap[L'7'].AltGr  = L'`';
    AzertyMap[L'&']=AzertyMap[L'7'];
    AzertyMap[L'8'].Normal = L'_';
    AzertyMap[L'8'].Shift  = L'8';
    AzertyMap[L'8'].AltGr  = L'\\';
    AzertyMap[L'*']=AzertyMap[L'8'];
    AzertyMap[L'9'].Normal = L'ç';
    AzertyMap[L'9'].Shift  = L'9';
    AzertyMap[L'9'].AltGr  = L'^';
    AzertyMap[L'(']=AzertyMap[L'9'];
    AzertyMap[L'0'].Normal = L'à';
    AzertyMap[L'0'].Shift  = L'0';
    AzertyMap[L'0'].AltGr  = L'@';
    AzertyMap[L')']=AzertyMap[L'0'];
    AzertyMap[L'-'].Normal = L')';
    AzertyMap[L'-'].Shift  = L'°';
    AzertyMap[L'-'].AltGr  = L']';
    AzertyMap[L'_']=AzertyMap[L'-'];
    AzertyMap[L'='].Normal = L'=';
    AzertyMap[L'='].Shift  = L'+';
    AzertyMap[L'='].AltGr  = L'}';
    AzertyMap[L'+']=AzertyMap[L'='];
}

void GetConfigValue(CHAR16* Line, CHAR16** Key, CHAR16** Value) {
    *Key = Line;
    *Value = NULL;
    
    // 1. Chercher le '='
    for(UINTN i = 0; Line[i] != L'\0'; i++) {
        if(Line[i] == L'=') {
            Line[i] = L'\0';
            *Value = &Line[i + 1];
            UINTN keyLen = StrLen(*Key);
            while (keyLen > 0 && (*Key)[keyLen - 1] == L' ') {
                (*Key)[keyLen - 1] = L'\0';
                keyLen--;
            }
            while (**Value == L' ') (*Value)++;
            return;
        }
    }
}

void CutLine(CHAR16* Line,CHAR16** NextLine){
    *NextLine = NULL;
    while(*Line!=L'\0'&&*Line!=L'\r'&&*Line!=L'\n') Line++;
    if(*Line==L'\0') return;
    if(*Line==L'\r'&&*(Line+1)==L'\n'){ 
        *Line = L'\0';
        *NextLine = Line+2;
    } else {
        *Line = L'\0';
        *NextLine = Line+1;
    }
    return;
}

void ApplyConfig(CHAR16* Key, CHAR16* Value) {
    if (StrCmp(Key, L"info") == 0)      ActualConfig.Theme.Info = StrToHex(Value);
    else if (StrCmp(Key, L"theme.ls_file") == 0)    ActualConfig.Theme.File = StrToHex(Value);
    else if (StrCmp(Key, L"theme.ls_folder") == 0) ActualConfig.Theme.Folder = StrToHex(Value);
    else if (StrCmp(Key, L"theme.prompt") == 0)    ActualConfig.Theme.Prompt = StrToHex(Value);
    else if (StrCmp(Key, L"theme.warning") == 0) ActualConfig.Theme.Warning = StrToHex(Value);
    else if (StrCmp(Key, L"theme.error") == 0) ActualConfig.Theme.Error = StrToHex(Value);
    else if (StrCmp(Key, L"theme.success") == 0) ActualConfig.Theme.Sucess = StrToHex(Value);
    else if (StrCmp(Key, L"theme.bg") == 0) ActualConfig.Theme.Background = StrToHex(Value);
    else if (StrCmp(Key, L"prompt") == 0) StrCpy(ActualConfig.Prompt,Value);
    else if (StrCmp(Key, L"font") == 0) {
        
    }
    else CPrint(ActualConfig.Theme.Warning,L"<%s> not reconised\n",Key);
}

EFI_STATUS LoadCFG(CONST CHAR16* Path) {
    if(!Path) return EFI_INVALID_PARAMETER;
    FS_NODE *Node;
    EFI_STATUS status = VFSOpen(ActualNode, Path, &Node, EFI_FILE_MODE_READ, 0);
    CHECK_STATUS(status,NULL,TRUE,;);
    
    UINTN Size = 0;
    CHAR8* Buff = NULL;
    status = VFSRead(Node, (void**)&Buff, &Size);
    if(EFI_ERROR(status)){ Node->Close(Node); return status; }
    
    // Allocation sécurisée : taille du buffer * 2 octets par CHAR16
    CHAR16 *ConfigFileContent = NULL;
    Char8ToChar16(Buff, &ConfigFileContent);
    ConfigFileContent[Size]=L'\0';
    
    kfree(Buff); 
    Node->Close(Node);

    CHAR16 *ActualLine = ConfigFileContent;
    CHAR16 *NextLine = NULL;

    while (ActualLine != NULL) {
        CutLine(ActualLine, &NextLine);
        if(*ActualLine==L'#'){
            ActualLine = NextLine;
            continue;
        }
        if (ActualLine[0] != L'\0') {
            CHAR16 *Key = NULL, *Value = NULL;
            GetConfigValue(ActualLine, &Key, &Value);
            if (Key && Value) {
                ApplyConfig(Key,Value);
            }
        }
        ActualLine = NextLine;
    }
    kfree(ConfigFileContent);
    return EFI_SUCCESS;
}

UINTN AsciiSPrint(CHAR8 *Buffer, UINTN BufferSize, CONST CHAR8 *Format, ...) {
    va_list Args;
    va_start(Args, Format);
    UINTN Length = AsciiVSPrint(Buffer, BufferSize, Format, Args);
    va_end(Args);
    return Length;
}

UINTN Char16ToChar8(CONST CHAR16* Src, CHAR8** Dest) {
    if (Src == NULL || Dest == NULL) {
        if (Dest != NULL) *Dest = NULL;
        return 0;
    }

    // 1. Calcul de la taille exacte nécessaire en octets UTF-8
    UINTN Utf8ByteCount = 0;
    for (UINTN i = 0; Src[i] != L'\0'; i++) {
        UINT16 c = Src[i];
        if (c <= 0x007F)      Utf8ByteCount += 1; // ASCII standard
        else if (c <= 0x07FF) Utf8ByteCount += 2; // Accents (é, à, è, etc.)
        else                  Utf8ByteCount += 3; // Caractères spéciaux/asiatiques
    }

    // 2. Allocation dynamique de la taille exacte (+1 pour le '\0')
    *Dest = kmalloc(Utf8ByteCount + 1);
    if (*Dest == NULL) {
        return 0;
    }

    // 3. Conversion UTF-16 vers UTF-8
    UINTN j = 0;
    for (UINTN i = 0; Src[i] != L'\0'; i++) {
        UINT16 c = Src[i];
        if (c <= 0x007F) {
            (*Dest)[j++] = (CHAR8)c;
        } else if (c <= 0x07FF) {
            (*Dest)[j++] = (CHAR8)(0xC0 | ((c >> 6) & 0x1F));
            (*Dest)[j++] = (CHAR8)(0x80 | (c & 0x3F));
        } else {
            (*Dest)[j++] = (CHAR8)(0xE0 | ((c >> 12) & 0x0F));
            (*Dest)[j++] = (CHAR8)(0x80 | ((c >> 6) & 0x3F));
            (*Dest)[j++] = (CHAR8)(0x80 | (c & 0x3F));
        }
    }
    (*Dest)[j] = '\0';

    return j;
}

UINTN Char8ToChar16(CONST CHAR8* Src, CHAR16** Dest) {
    if (Src == NULL || Dest == NULL) {
        if (Dest != NULL) *Dest = NULL;
        return 0;
    }

    UINTN SrcLen = strlena(Src); // Ou AsciiStrLen(Src)

    // 1. Calcul du nombre de caractères Unicode (CHAR16)
    UINTN CharCount = 0;
    for (UINTN i = 0; i < SrcLen; i++) {
        // En UTF-8, les octets de continuation commencent par les bits 10xxxxxx (0x80..0xBF)
        if (((UINT8)Src[i] & 0xC0) != 0x80) {
            CharCount++;
        }
    }

    // 2. Allocation dynamique du buffer CHAR16 (+1 pour le L'\0')
    *Dest = kmalloc((CharCount + 1) * sizeof(CHAR16));
    if (*Dest == NULL) {
        return 0;
    }

    // 3. Décodage UTF-8 vers UTF-16
    UINTN i = 0, j = 0;
    while (i < SrcLen && Src[i] != '\0') {
        UINT32 c = (UINT8)Src[i];

        if (c < 0x80) { // 1 octet (ASCII)
            (*Dest)[j++] = (CHAR16)c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < SrcLen) { // 2 octets (Accents)
            (*Dest)[j++] = (CHAR16)(((c & 0x1F) << 6) | ((UINT8)Src[i + 1] & 0x3F));
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < SrcLen) { // 3 octets
            (*Dest)[j++] = (CHAR16)(((c & 0x0F) << 12) | (((UINT8)Src[i + 1] & 0x3F) << 6) | ((UINT8)Src[i + 2] & 0x3F));
            i += 3;
        } else {
            // Sécurité si séquence UTF-8 corrompue
            i++;
        }
    }
    (*Dest)[j] = L'\0';

    return j; // Renvoie le nombre de CHAR16 écrits (hors NULL)
}

VOID FormatBytes(UINTN bytes, CHAR16* out_str) {
    const CHAR16* units[] = { L"B", L"KiB", L"MiB", L"GiB", L"TiB" };
    UINTN unit_index = 0;
    UINTN val = bytes;
    UINTN remainder = 0;

    // Convertit en divisant par 1024 à chaque étape
    while (val >= 1024 && unit_index < 4) {
        remainder = val % 1024;
        val /= 1024;
        unit_index++;
    }

    if (unit_index == 0) {
        // Octets bruts
        SPrint(out_str, 64, L"%u B", val);
    } else {
        // Calcul d'une décimale avec des entiers : (reste * 10) / 1024
        UINTN decimal = (remainder * 10) / 1024;
        SPrint(out_str, 64, L"%u.%u %s", val, decimal, units[unit_index]);
    }
}

UINTN StrToHex(CONST CHAR16* Str) {
    UINTN Result = 0;
    if (Str[0] == L'0' && (Str[1] == L'x' || Str[1] == L'X')) {
        Str += 2;
    }

    while (*Str != L'\0') {
        CHAR16 c = *Str;
        UINTN Value = 0;

        if (c >= L'0' && c <= L'9') {
            Value = c - L'0';
        } else if (c >= L'a' && c <= L'f') {
            Value = c - L'a' + 10;
        } else if (c >= L'A' && c <= L'F') {
            Value = c - L'A' + 10;
        } else {
            break; 
        }
        Result = (Result << 4) | Value;
        Str++;
    }

    return Result;
}

UINTN HexToStr(UINTN Value, CHAR16* Dest, UINTN MaxLength, BOOLEAN IncludePrefix, UINTN MinWidth) {
    if (Dest == NULL || MaxLength == 0) return 0;

    static CONST CHAR16 HexDigits[] = {
        L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7',
        L'8', L'9', L'A', L'B', L'C', L'D', L'E', L'F', L'\0'
    };
    CHAR16 TempBuf[32];
    UINTN TempIndex = 0;
    UINTN DestIndex = 0;
    if (IncludePrefix) {
        if (MaxLength <= 2) {
            Dest[0] = L'\0';
            return 0;
        }
        Dest[DestIndex++] = L'0';
        Dest[DestIndex++] = L'x';
    }
    if (Value == 0) {
        TempBuf[TempIndex++] = L'0';
    } else {
        while (Value > 0 && TempIndex < 32) {
            TempBuf[TempIndex++] = HexDigits[Value & 0x0F];
            Value >>= 4;
        }
    }
    while (TempIndex < MinWidth && TempIndex < 32) {
        TempBuf[TempIndex++] = L'0';
    }
    while (TempIndex > 0 && DestIndex < MaxLength - 1) {
        Dest[DestIndex++] = TempBuf[--TempIndex];
    }

    Dest[DestIndex] = L'\0';
    return DestIndex;
}

void HookUefiException(UINT8 vector, void(*handler)(void));

extern void isr_0(void);
extern void isr_1(void);
extern void isr_2(void);
extern void isr_3(void);
extern void isr_4(void);
extern void isr_5(void);
extern void isr_6(void);
extern void isr_7(void);
extern void isr_8(void);
extern void isr_9(void);
extern void isr_10(void);
extern void isr_11(void);
extern void isr_12(void);
extern void isr_13(void);
extern void isr_14(void);
extern void isr_15(void);
extern void isr_16(void);
extern void isr_17(void);
extern void isr_18(void);
extern void isr_19(void);
extern void isr_20(void);
extern void isr_21(void);
extern void isr_22(void);
extern void isr_23(void);
extern void isr_24(void);
extern void isr_25(void);
extern void isr_26(void);
extern void isr_27(void);
extern void isr_28(void);
extern void isr_29(void);
extern void isr_30(void);
extern void isr_31(void);

//-----------------------------------------INIT+LEDS---------------------------------------------------------------

EFI_STATUS GeneralInit() {
    EFI_STATUS status;
    EFI_GUID InputExGuid = EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID;
    status = uefi_call_wrapper(
        gBS->HandleProtocol, 3,
        ST->ConsoleInHandle,
        &InputExGuid,
        (VOID**)&InputEx
    );

    ActualConfig.Theme.Info = RGB(255,255,255);
    ActualConfig.Theme.File = RGB(0,127,255);
    ActualConfig.Theme.Folder = RGB(255,255,0);
    ActualConfig.Theme.Prompt = RGB(255,255,0);
    ActualConfig.Theme.Warning = RGB(255,255,0);
    ActualConfig.Theme.Error = RGB(255,0,0);
    ActualConfig.Theme.Sucess = RGB(0,255,0);
    ActualConfig.Theme.Background = RGB(0,0,0);
    CopyMem(&BackupConfig,&ActualConfig,sizeof(CONFIG));
    InitAzertyMap();
    HookUefiException(0, isr_0);
    HookUefiException(1, isr_1);
    HookUefiException(2, isr_2);
    HookUefiException(3, isr_3);
    HookUefiException(4, isr_4);
    HookUefiException(5, isr_5);
    HookUefiException(6, isr_6);
    HookUefiException(7, isr_7);
    HookUefiException(8, isr_8);
    HookUefiException(9, isr_9);
    HookUefiException(10, isr_10);
    HookUefiException(11, isr_11);
    HookUefiException(12, isr_12);
    HookUefiException(13, isr_13);
    HookUefiException(14, isr_14);
    HookUefiException(15, isr_15);
    HookUefiException(16, isr_16);
    HookUefiException(17, isr_17);
    HookUefiException(18, isr_18);
    HookUefiException(19, isr_19);
    HookUefiException(20, isr_20);
    HookUefiException(21, isr_21);
    HookUefiException(22, isr_22);
    HookUefiException(23, isr_23);
    HookUefiException(24, isr_24);
    HookUefiException(25, isr_25);
    HookUefiException(26, isr_26);
    HookUefiException(27, isr_27);
    HookUefiException(28, isr_28);
    HookUefiException(29, isr_29);
    HookUefiException(30, isr_30);
    HookUefiException(31, isr_31);
    FS_NODE* tmp;
    status = VFSOpen(RootNode,L"\\dev\\prompt",&tmp,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,0);
    if(!EFI_ERROR(status)){
        tmp->Write(tmp,"%s > ",sizeof(" %s > "),FALSE);
        tmp->Close(tmp);
    }
    return status;
}
EFI_STATUS SetKeyboardLeds(UINT8 mode) {
    if (InputEx == NULL) return EFI_NOT_READY;
    EFI_KEY_TOGGLE_STATE State = EFI_TOGGLE_STATE_VALID | mode;

    // 3. On applique l'état au clavier via la doc que tu as trouvée !
    return uefi_call_wrapper(
        InputEx->SetState, 2,
        InputEx,
        &State
    );
}

//-----------------------------------------STRUCTS---------------------------------------------------------------


typedef struct ExceptionFrame
{
    /* Registres de contrôle */

    uint64_t cr0,cr2,cr3,cr4;
    uint64_t rax,rbx,rcx,rdx,rsi,rdi,rbp;
    uint64_t r8,r9,r10,r11,r12,r13,r14,r15;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} ExceptionFrame;

#pragma pack(push, 1)
typedef struct {
    UINT16 Limit;
    UINT64 Base;
} IDTR;

typedef struct {
    UINT16 OffsetLow;
    UINT16 Selector;
    UINT8  Ist;
    UINT8  TypeAttr;
    UINT16 OffsetMiddle;
    UINT32 OffsetHigh;
    UINT32 Reserved;
} IDT_ENTRY;
#pragma pack(pop)

//-----------------------------------------HANDLER+HOOK---------------------------------------------------------------

const CHAR16* GetExceptionName(EFI_EXCEPTION_TYPE Type) {
    switch (Type) {
        case 0:  return L"Divide-by-Zero (#DE)";
        case 1:  return L"Debug Exception (#DB)";
        case 2:  return L"NMI Interrupt";
        case 3:  return L"Breakpoint (#BP)";
        case 4:  return L"Overflow (#OF)";
        case 5:  return L"BOUND Range Exceeded (#BR)";
        case 6:  return L"Invalid Opcode (#UD)";
        case 7:  return L"Device Not Available (#NM)";
        case 8:  return L"Double Fault (#DF)";
        case 10: return L"Invalid TSS (#TS)";
        case 11: return L"Segment Not Present (#NP)";
        case 12: return L"Stack Fault (#SS)";
        case 13: return L"General Protection Fault (#GP)";
        case 14: return L"Page Fault (#PF)";
        case 16: return L"x87 FPU Floating-Point Error (#MF)";
        case 17: return L"Alignment Check (#AC)";
        case 18: return L"Machine Check (#MC)";
        case 19: return L"SIMD Floating-Point Exception (#XM)";
        default: return L"Unknown/Reserved Exception";
    }
}

__attribute__((ms_abi)) void MyPanicC(ExceptionFrame* regs) {
    // Ton code d'affichage personnalisé (ex: fond rouge/bleu)
    
    FillDisplay(RGB(0,0,255)); 
    UINT32 OldBackground = ActualConfig.Theme.Background;
    ActualConfig.Theme.Background=RGB(0,0,255);
    SetCursor(0,0);
    CPrint(RGB(255,255,255), L"!!! KERNEL PANIC !!!\n");
    CPrint(RGB(255,255,255), L"An exception occured. The system may be unstable\n");
    CPrint(RGB(255,255,255), L"Exception Vector : 0x%02llx (%s)\n", regs->vector,GetExceptionName(regs->vector));
    CPrint(RGB(255,255,255), L"Error code : 0x%016llx\n", regs->error_code);
    CPrint(RGB(255,255,255), L"======================= Context =======================\n");
    CPrint(RGB(255,255,255), L"RIP           : 0x%016llx   CS     : 0x%04x            \n", regs->rip, regs->cs & 0xFFFF                   );
    CPrint(RGB(255,255,255), L"RFLAGS        : 0x%016llx                              \n", regs->rflags                                   );
    CPrint(RGB(255,255,255), L"====================== Registers ======================\n");
    CPrint(RGB(255,255,255), L"RAX           : 0x%016llx   RBX    : 0x%016llx         \n", regs->rax, regs->rbx                           );
    CPrint(RGB(255,255,255), L"RCX           : 0x%016llx   RDX    : 0x%016llx         \n", regs->rcx, regs->rdx                           );
    CPrint(RGB(255,255,255), L"RSI           : 0x%016llx   RDI    : 0x%016llx         \n", regs->rsi, regs->rdi                           );
    CPrint(RGB(255,255,255), L"RBP           : 0x%016llx   RSP    : 0x%016llx         \n", regs->rbp, regs->rsp                           );
    CPrint(RGB(255,255,255), L"R8            : 0x%016llx   R9     : 0x%016llx         \n", regs->r8,  regs->r9                            );
    CPrint(RGB(255,255,255), L"R10           : 0x%016llx   R11    : 0x%016llx         \n", regs->r10, regs->r11                           );
    CPrint(RGB(255,255,255), L"R12           : 0x%016llx   R13    : 0x%016llx         \n", regs->r12, regs->r13                           );
    CPrint(RGB(255,255,255), L"R14           : 0x%016llx   R15    : 0x%016llx         \n", regs->r14, regs->r15                           );
    CPrint(RGB(255,255,255), L"================== Control Registers ==================\n");
    CPrint(RGB(255,255,255), L"CR0           : 0x%016llx   CR2    : 0x%016llx         \n", regs->cr0, regs->cr2                           );
    CPrint(RGB(255,255,255), L"CR3           : 0x%016llx   CR4    : 0x%016llx         \n", regs->cr3, regs->cr4                           );
    while (1) {
        EFI_KEY_DATA KeyData;
        if(uefi_call_wrapper(InputEx->ReadKeyStrokeEx, 2, InputEx, &KeyData)!=EFI_NOT_READY){
            ActualConfig.Theme.Background = OldBackground;
            FillDisplay(ActualConfig.Theme.Background);
            SetCursor(0,0);
            return;
        };
        SetKeyboardLeds(0x07);
        uefi_call_wrapper(BS->Stall,1,250*1000);
        SetKeyboardLeds(0x00);
        uefi_call_wrapper(BS->Stall,1,250*1000);
    }
}

void HookUefiException(UINT8 vector, void(*handler)(void)) {
    volatile IDTR idtr;
    __asm__ __volatile__("sidt %0" : "=m"(idtr)); // Lit l'IDT actuelle de l'UEFI
    IDT_ENTRY* idt = (IDT_ENTRY*)idtr.Base;
    UINT64 addr = (UINT64)handler;

    __asm__ __volatile__("cli"); // Sécurité

    idt[vector].OffsetLow    = (UINT16)(addr & 0xFFFF);
    UINT16 current_cs;
    __asm__ __volatile__(".code64\n\t" "mov %%cs, %0" : "=r"(current_cs)::);

    idt[vector].Selector = current_cs; // Code segment UEFI standard
    idt[vector].Ist          = 0;
    idt[vector].TypeAttr     = 0x8E;  // 32/64-bit Interrupt Gate, Present
    idt[vector].OffsetMiddle = (UINT16)((addr >> 16) & 0xFFFF);
    idt[vector].OffsetHigh   = (UINT32)((addr >> 32) & 0xFFFFFFFF);
    idt[vector].Reserved     = 0;

    __asm__ __volatile__("sti");
}

BOOLEAN CheckRdrandSupport() {
    UINT32 eax, ebx, ecx, edx;
    // CPUID leaf 1, bit 30 du registre ECX indique le support RDRAND
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    return (ecx & (1 << 30)) != 0;
}

// Génération aléatoire matérielle
BOOLEAN GetHardwareRandom(UINT64 *val) {
    UINT8 success;
    __asm__ volatile("rdrand %0; setc %1" : "=r"(*val), "=qm"(success));
    return success != 0;
}

// Fallback RDTSC
UINT64 GetRdtsc() {
    UINT32 low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((UINT64)high << 32) | low;
}

// Fonction globale pour ton OS
UINT64 RandValue() {
    UINT64 val;
    if (GetHardwareRandom(&val)) {
        return val;
    }
    return GetRdtsc();
}

UINT64 GetTSCFrequencyPerMs(VOID) {
    EFI_EVENT TimerEvent;
    UINTN Index;
    EFI_STATUS Status;

    // 1. Créer un événement de timer
    Status = uefi_call_wrapper(BS->CreateEvent, 5, EVT_TIMER, TPL_CALLBACK, NULL, NULL, &TimerEvent);
    if (EFI_ERROR(Status)) return 0;

    // 2. Armer le timer pour 10 ms (100 000 x 100 ns)
    uefi_call_wrapper(BS->SetTimer, 3, TimerEvent, TimerRelative, 100000ULL);

    // 3. Mesurer les cycles CPU pendant ces 10 ms
    UINT64 StartTSC = ReadTSC();
    uefi_call_wrapper(BS->WaitForEvent, 3, 1, &TimerEvent, &Index);
    UINT64 EndTSC = ReadTSC();

    uefi_call_wrapper(BS->CloseEvent, 1, TimerEvent);

    // 4. (Cycles en 10ms) / 10 = Cycles par milliseconde
    return (EndTSC - StartTSC) / 10;
}