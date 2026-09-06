#include "display.h"
#include <efi.h>
#include <efilib.h>
#include "func.h"
#include "disk.h"
#include "graphics.h"
#include "memory.h"

extern UINT8 _binary_font_psf_start[];
extern UINT8 _binary_font_psf_end[];

static UINTN CharHeight = 16;
static UINTN CharWidth = 8;
static UINTN CharSize = 16;


EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* GopInfo = NULL;
static EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
UINT32 *Framebuffer;
UINT32 *ActualFramebuffer;
static UINT32 *TempFramebuffer;
static UINT32 TempCursorX;
static UINT32 TempCursorY;
static UINT32 CursorX;
static UINT32 CursorY;
static UINT32 MaxChar;
static UINT32 MaxLines;
static BOOLEAN WaitForActualize = FALSE;
UINTN ModeCount = 0;
FS_NODE* ShellNode; 
PSF2_HEADERS* ActualFontHeaders;
static UINT8* ActualFontData;
static UINTN FontSize;

UINT32 RGB(UINT8 Red, UINT8 Green, UINT8 Blue){
    if(!GopInfo)return 0;
    if(GopInfo->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) return Red | (Green << 8) | (Blue << 16);
    if(GopInfo->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) return Blue | (Green << 8) | (Red << 16);
    return 0;
}

VOID ToggleCursor() {
    if (!GopInfo || !ActualFramebuffer) return;
    UINT32 StartX = CursorX * CharWidth;
    UINT32 StartY = CursorY * CharHeight;

    if (StartX + CharWidth > GopInfo->HorizontalResolution || 
        StartY + CharHeight > GopInfo->VerticalResolution) return;

    for (UINTN y = 0; y < CharHeight; y++) {
        for (UINTN x = 0; x < CharWidth; x++) {
            UINTN index = (GopInfo->PixelsPerScanLine) * (StartY + y) + (StartX + x);
            UINT32 color = (ActualFramebuffer[index] == 0) ? 0xFFFFFF : 0x000000;
            RenderPixelBypass(color, StartX + x, StartY + y); 
        }
    }
}

EFI_STATUS InitFont(VOID* FontBuffer) {
    if (!FontBuffer) return EFI_INVALID_PARAMETER;

    UINT8* magic = (UINT8*)FontBuffer;

    // 1. Nettoyage de l'ancienne police en mémoire s'il y en a une
    if (ActualFontHeaders && (UINT8*)ActualFontHeaders != _binary_font_psf_start) {
        kfree(ActualFontHeaders);
        ActualFontHeaders = NULL;
        ActualFontData = NULL;
    }

    // --- TRAITEMENT PSF1 ---
    if (magic[0] == 0x36 && magic[1] == 0x04) {
        PSF1_HEADER* psf1 = (PSF1_HEADER*)FontBuffer;

        UINT32 glyph_count = (psf1->mode & 0x01) ? 512 : 256;
        UINT32 data_size = glyph_count * psf1->charsize;

        // On alloue un header PSF2 fictif/synthétisé pour garder le reste du code unifié !
        ActualFontHeaders = kmalloc(sizeof(PSF2_HEADERS) + data_size);
        if (!ActualFontHeaders) return EFI_OUT_OF_RESOURCES;

        // Remplissage d'une structure PSF2 virtuelle
        ActualFontHeaders->magic[0] = 0x72;
        ActualFontHeaders->magic[1] = 0xB5;
        ActualFontHeaders->magic[2] = 0x4A;
        ActualFontHeaders->magic[3] = 0x86;
        ActualFontHeaders->version = 0;
        ActualFontHeaders->headersize = sizeof(PSF2_HEADERS);
        ActualFontHeaders->flags = 0; // Pas de table Unicode
        ActualFontHeaders->length = glyph_count;
        ActualFontHeaders->charsize = psf1->charsize;
        ActualFontHeaders->height = psf1->charsize;
        ActualFontHeaders->width = 8;

        ActualFontData = ((UINT8*)ActualFontHeaders) + sizeof(PSF2_HEADERS);
        CopyMem(ActualFontData, magic + sizeof(PSF1_HEADER), data_size);

        CharWidth = 8;
        CharHeight = psf1->charsize;
        CharSize = psf1->charsize;
        FontSize = sizeof(PSF2_HEADERS) + data_size;

        if (GopInfo) {
            MaxChar = GopInfo->HorizontalResolution / CharWidth;
            MaxLines = GopInfo->VerticalResolution / CharHeight;
        }

        return EFI_SUCCESS;
    }

    // --- TRAITEMENT PSF2 ---
    if (magic[0] == 0x72 && magic[1] == 0xB5 && magic[2] == 0x4A && magic[3] == 0x86) {
        PSF2_HEADERS* psf2 = (PSF2_HEADERS*)FontBuffer;

        UINT32 size = psf2->headersize + (psf2->length * psf2->charsize);

        // Analyse de la table Unicode si présente
        if (psf2->flags & PSF2_HAS_UNICODE_TABLE) {
            UINT8 *ptr = (UINT8 *)FontBuffer + size;
            UINT32 glyphs_found = 0;

            while (glyphs_found < psf2->length) {
                if (*ptr == 0xFF) {
                    glyphs_found++;
                }
                ptr++;
            }
            size = (UINT32)(ptr - (UINT8 *)FontBuffer);
        }

        ActualFontHeaders = kmalloc(size);
        if (!ActualFontHeaders) return EFI_OUT_OF_RESOURCES;

        CopyMem(ActualFontHeaders, FontBuffer, size);

        ActualFontData = ((UINT8*)ActualFontHeaders) + ActualFontHeaders->headersize;
        CharHeight = ActualFontHeaders->height;
        CharWidth = ActualFontHeaders->width;
        CharSize = ActualFontHeaders->charsize;
        FontSize = size;

        if (GopInfo) {
            MaxChar = GopInfo->HorizontalResolution / CharWidth;
            MaxLines = GopInfo->VerticalResolution / CharHeight;
        }
        return EFI_SUCCESS;
    }

    return EFI_UNSUPPORTED; // Format ni PSF1 ni PSF2
}


// --- Recherche du Glyphe par UTF-16 (Unicode) ---

UINT32 GetGlyphIndex(CHAR16 unicode_char) {
    if (!ActualFontHeaders) return 0;

    // Si pas de table Unicode, fallback vers l'index direct
    if (!(ActualFontHeaders->flags & PSF2_HAS_UNICODE_TABLE)) {
        return (unicode_char < ActualFontHeaders->length) ? (UINT32)unicode_char : 0;
    }

    UINT8 *ptr = (UINT8 *)ActualFontHeaders + ActualFontHeaders->headersize + 
                 (ActualFontHeaders->length * ActualFontHeaders->charsize);
    UINT8 *end = (UINT8 *)ActualFontHeaders + FontSize;

    UINT32 current_glyph = 0;

    while (ptr < end && current_glyph < ActualFontHeaders->length) {
        if (*ptr == 0xFF) { // Séparateur PSF2
            current_glyph++;
            ptr++;
            continue;
        }

        // Décodage UTF-8 de la table Unicode vers UTF-16/UCS-2 pour comparaison
        UINT32 ucs_val = 0;
        if ((*ptr & 0x80) == 0) {
            ucs_val = *ptr++;
        } else if ((*ptr & 0xE0) == 0xC0) {
            ucs_val = (*ptr++ & 0x1F) << 6;
            ucs_val |= (*ptr++ & 0x3F);
        } else if ((*ptr & 0xF0) == 0xE0) {
            ucs_val = (*ptr++ & 0x0F) << 12;
            ucs_val |= (*ptr++ & 0x3F) << 6;
            ucs_val |= (*ptr++ & 0x3F);
        } else {
            ptr++; // Séquence 4 octets ou invalide (non supportée en UTF-16 standard)
            continue;
        }

        if (ucs_val == (UINT32)unicode_char) {
            return current_glyph; // Glyphe trouvé !
        }
    }

    return 0; // Glyphe par défaut (rectangle ou '?' en position 0)
}

// --- Rendu d'un Caractère ---

void RenderChar(CHAR16 c, UINT32 Color) {
    if (c == L'\n') {
        CursorX = 0;
        CursorY++;
        if (CursorY >= MaxLines) Scroll();
    } else if (c == L'\b') {
        if (CursorX > 0) CursorX--;
    } else if (c == L'\r') {
        CursorX = 0;
    } else {
        UINT32 glyph_idx = GetGlyphIndex(c);
        UINT8* glyph_ptr = ActualFontData + (glyph_idx * ActualFontHeaders->charsize);
        UINTN bytes_per_line = (CharWidth + 7) / 8;

        for (UINTN py = 0; py < CharHeight; py++) {
            for (UINTN px = 0; px < CharWidth; px++) {
                // Sélection de l'octet et du bit correspondant au pixel (px, py)
                UINT8 byte = glyph_ptr[py * bytes_per_line + (px / 8)];
                BOOLEAN pixel_set = (byte & (1 << (7 - (px % 8)))) != 0;

                UINT32 pixel_color = pixel_set ? Color : ActualConfig.Theme.Background;
                RenderPixel(pixel_color, CursorX * CharWidth + px, CursorY * CharHeight + py);
            }
        }
        CursorX++;
    }
}

EFI_STATUS GopInit(){
    EFI_STATUS status;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;

    EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

    status = uefi_call_wrapper(BS->LocateHandleBuffer, 5,ByProtocol,&gopGuid,NULL,&HandleCount,&HandleBuffer);

    if (EFI_ERROR(status) || HandleCount == 0)
        return status;

    status = uefi_call_wrapper(BS->HandleProtocol, 3,
        HandleBuffer[0],
        &gopGuid,
        (void**)&gop);

    CHECK_STATUS(status,NULL,FALSE,NOP);
    

    UINT32 BestMode = 0;
    UINT32 MaxPixels = 0;

    for (UINT32 i = 0; i < gop->Mode->MaxMode; i++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* Info;
        UINTN SizeOfInfo;
        status = uefi_call_wrapper(gop->QueryMode,4,gop, i, &SizeOfInfo, &Info);
        if(EFI_ERROR(status))continue;
        UINT32 pixels = Info->HorizontalResolution * Info->VerticalResolution;
        if (pixels > MaxPixels) {
            if(Info->HorizontalResolution==2048 && Info->VerticalResolution==2048) continue;
            MaxPixels = pixels;
            BestMode = i;
        }
        kfree(Info);
    }
    
    status = uefi_call_wrapper(gop->SetMode,2,gop,BestMode);
    
    GopInfo = gop->Mode->Info;
    ActualFramebuffer = (UINT32*)(UINTN)gop->Mode->FrameBufferBase;
    Framebuffer = kmalloc((GopInfo->VerticalResolution) * GopInfo->PixelsPerScanLine*sizeof(UINT32));
    if(!Framebuffer){
        //We skip double buffering
        Framebuffer=ActualFramebuffer;
    }

    CursorX = 0;
    CursorY = 0;

    MaxChar = GopInfo->HorizontalResolution / CharWidth;
    MaxLines = GopInfo->VerticalResolution / CharHeight;
    
    if(status)CPrint(ActualConfig.Theme.Warning,L"Warning : Error occured while setting se resolution to the highest one (%r)  ; default resolution used", status);
    return InitFont((PSF2_HEADERS*)_binary_font_psf_start);
}


void Actualize(){
    CopyMem(ActualFramebuffer,Framebuffer,(GopInfo->VerticalResolution) * GopInfo->PixelsPerScanLine*sizeof(UINT32));
}

void TemporaryBuffer(BOOLEAN State){
    if(State){
        TempCursorX=CursorX;
        TempCursorY=CursorY;
        CursorX=0;
        CursorY=0;
        TempFramebuffer=Framebuffer;
        Framebuffer = kmalloc((GopInfo->VerticalResolution) * GopInfo->PixelsPerScanLine*sizeof(UINT32));
        if(!Framebuffer)Framebuffer=ActualFramebuffer; //We skip double buffering
        FillDisplay(0);
    } else {
        CursorX=TempCursorX;
        CursorY=TempCursorY;
        if(Framebuffer!=ActualFramebuffer)kfree(Framebuffer);
        Framebuffer=TempFramebuffer;
        TempFramebuffer=NULL;
        Actualize();
    }

}

void CPrintWait(BOOLEAN State){
    WaitForActualize=State;
    if(!State)Actualize();
}

void CPrint(UINT32 color, CONST CHAR16 *fmt, ...){
    va_list args;
    va_start(args, fmt);   
    UINTN Size = UnicodeVSPrint(NULL,0 , fmt, args);
    CHAR16* buffer = kmalloc((Size+1)*sizeof(CHAR16));
    if(!buffer) return;
    UnicodeVSPrint(buffer,(Size+1)*sizeof(CHAR16) , fmt, args);
    va_end(args); 
    Print(L"%s",buffer); 
    RenderString(buffer,color);
    kfree(buffer);
    if(!WaitForActualize)
    Actualize();
}

void ShellPrint(UINT32 color, CONST CHAR16 *fmt, ...){
    va_list args;
    va_start(args, fmt);    
    UINTN SizeChars = UnicodeVSPrint(NULL, 0, fmt, args);
    CHAR16* buffer = kmalloc((SizeChars + 1) * sizeof(CHAR16));
    
    if(!buffer) { va_end(args); return; }
    UnicodeVSPrint(buffer, (SizeChars + 1) * sizeof(CHAR16), fmt, args);
    va_end(args); 

    if (ShellNode) {
        CHAR8* utf8Buffer = NULL;
        Char16ToChar8(buffer, &utf8Buffer);
        ShellNode->Write(ShellNode, utf8Buffer, SizeChars, TRUE);
        kfree(utf8Buffer);
    } else {
        CPrint(color,L"%s",buffer);
    }
    kfree(buffer);
}

void CPrintFree(UINT32 PosX, UINT32 PosY, UINT32 color, CONST CHAR16 *fmt, ...){
    va_list args;
    va_start(args, fmt);    
    UINTN Size = UnicodeVSPrint(NULL,0 , fmt, args);
    CHAR16* buffer = kmalloc((Size+1)*sizeof(CHAR16));
    if(!buffer) return;
    UnicodeVSPrint(buffer,(Size+1)*sizeof(CHAR16) , fmt, args);
    va_end(args);
    UINT32 CursorXTemp = CursorX;
    UINT32 CursorYTemp = CursorY; 
    CursorX=PosX;
    CursorY=PosY;                  
    RenderString(buffer,color);
    CursorX=CursorXTemp;
    CursorY=CursorYTemp;
    kfree(buffer);
    Actualize();
}

void Scroll() {
    UINTN line_size = GopInfo->PixelsPerScanLine * CharHeight;
    UINTN total_pixels = (GopInfo->VerticalResolution - CharHeight) * GopInfo->PixelsPerScanLine;

    CopyMem(Framebuffer,Framebuffer + line_size,total_pixels * sizeof(UINT32));
    UINTN* last_line64 = (UINTN*)&Framebuffer[(GopInfo->VerticalResolution - CharHeight) * GopInfo->PixelsPerScanLine];
    UINTN line_blocks64 = line_size / 2;
    UINTN double_pixel_color = ((UINTN)ActualConfig.Theme.Background << 32) | ActualConfig.Theme.Background;
    for (UINTN i = 0; i < line_blocks64; i++)
        last_line64[i] = double_pixel_color;

    if (line_size % 2)
        last_line64[line_blocks64 * 2] = double_pixel_color;

    CursorY = MaxLines - 1;

}

void RenderString(CHAR16* buffer,UINT32 Color){
    while(*buffer){
        RenderChar(*buffer,Color);
        if(CursorX==MaxChar){
            CursorY++;
            if(CursorY>=MaxLines)
                Scroll();
                
            CursorX=0;
        }
        buffer++;
    }
}

EFI_STATUS GetCursor(INT64* X,INT64* Y){
    *X = CursorX;
    *Y = CursorY;
    return EFI_SUCCESS;
}

EFI_STATUS SetCursor(INT64 X,INT64 Y){
    if(X>=0)
        if(X<=MaxChar)
            CursorX=X;
    if(Y>=0)
        if(Y<=MaxLines)
            CursorY=Y;
    Actualize();
    return EFI_SUCCESS;
}

GopModeList* GetModeList(){
    ModeCount = gop->Mode->MaxMode;
    GopModeList* ModeList = kmalloc(ModeCount * sizeof(GopModeList));
    if(!ModeList) return NULL;
    EFI_STATUS status;
    for (UINT32 i = 0; i < ModeCount; i++) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* Info;
        UINTN SizeOfInfo;
        status = uefi_call_wrapper(gop->QueryMode,4,gop, i, &SizeOfInfo, &Info);
        if(EFI_ERROR(status)){
            ModeList[i].SizeX = (UINTN)(-1);
            ModeList[i].SizeY = (UINTN)(-1);
            continue;
        }
        ModeList[i].SizeX = Info->HorizontalResolution;
        ModeList[i].SizeY = Info->VerticalResolution;
        
        kfree(Info);
    }
    return ModeList;
}

EFI_STATUS SetMode(UINTN Mode)
{
    EFI_STATUS status = uefi_call_wrapper(gop->SetMode,2,gop,Mode);
    CHECK_STATUS(status,NULL,TRUE,;);
    GopInfo = gop->Mode->Info;
    ActualFramebuffer = (UINT32*)(UINTN)gop->Mode->FrameBufferBase;
    if(Framebuffer!=ActualFramebuffer)kfree(Framebuffer);
    Framebuffer = kmalloc((GopInfo->VerticalResolution) * GopInfo->PixelsPerScanLine*sizeof(UINT32));
    if(!Framebuffer){
        //We skip double buffering
        Framebuffer=ActualFramebuffer;
    }
    FillDisplay(0);

    CursorX = 0;
    CursorY = 0;

    MaxChar = GopInfo->HorizontalResolution / CharWidth;
    MaxLines = GopInfo->VerticalResolution / CharHeight;
    return EFI_SUCCESS;
}
