#include "display.h"
#include <efi.h>
#include <efilib.h>
#include "func.h"
#include "font.h"

#define LINE_HEIGHT 16
EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* GopInfo = NULL;
EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
UINT32 *Framebuffer;
UINT32 *ActualFramebuffer;
UINT32 CursorX;
UINT32 CursorY;
UINT32 MaxChar;
UINT32 MaxLines;

UINT32 RGB(UINT8 Red, UINT8 Green, UINT8 Blue){
    if(!GopInfo)return 0;
    if(GopInfo->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) return Red | (Green << 8) | (Blue << 16);
    if(GopInfo->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) return Blue | (Green << 8) | (Red << 16);
    return 0;
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

    if (EFI_ERROR(status))
        return status;
    

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

    BOOLEAN IsError = FALSE;
    
    IsError = uefi_call_wrapper(gop->SetMode,2,gop,BestMode);
    
    
    GopInfo = gop->Mode->Info;
    ActualFramebuffer = (UINT32*)(UINTN)gop->Mode->FrameBufferBase;
    Framebuffer = AllocatePool((GopInfo->VerticalResolution) * GopInfo->PixelsPerScanLine*sizeof(UINT32));


    CursorX = 0;
    CursorY = 0;

    MaxChar = GopInfo->HorizontalResolution / 8;
    MaxLines = GopInfo->VerticalResolution / LINE_HEIGHT;
    
    if(IsError)CPrint(THEME_WARNING,L"Warning : Error occured while setting se resolution to the highest one ; default resolution used");
    return EFI_SUCCESS;
}

EFI_STATUS RenderPixel(UINT32 Color,UINT32 x,UINT32 y){
    UINT64 Pos = (GopInfo->PixelsPerScanLine)*y+x;
    Framebuffer[Pos]=Color;
    return EFI_SUCCESS;
}

EFI_STATUS FillDisplay(UINT32 Color){
    for(UINTN y = 0;y<GopInfo->VerticalResolution;y++){
        for(UINTN x = 0;x<GopInfo->HorizontalResolution;x++){
            RenderPixel(Color,x,y);
        }
    }
    return EFI_SUCCESS;
}

void Actualize(){
    CopyMem(ActualFramebuffer,Framebuffer,(GopInfo->VerticalResolution) * GopInfo->PixelsPerScanLine*sizeof(UINT32));

}

void CPrint(UINT32 color, CONST CHAR16 *fmt, ...){
    va_list args;
    va_start(args, fmt);    
    UINTN Size = UnicodeVSPrint(NULL,0 , fmt, args);
    CHAR16* buffer = kmalloc((Size+1)*sizeof(CHAR16));
    UnicodeVSPrint(buffer,(Size+1)*sizeof(CHAR16) , fmt, args);
    va_end(args);                   
    RenderString(buffer,color);
    kfree(buffer);
    Actualize();
}

void Scroll() {
    UINTN line_size = GopInfo->PixelsPerScanLine * LINE_HEIGHT;
    UINTN total_pixels = (GopInfo->VerticalResolution - LINE_HEIGHT) * GopInfo->PixelsPerScanLine;

    CopyMem(Framebuffer,Framebuffer + line_size,total_pixels * sizeof(UINT32));
    // Effacer la dernière ligne
    UINTN* last_line64 = (UINTN*)&Framebuffer[(GopInfo->VerticalResolution - LINE_HEIGHT) * GopInfo->PixelsPerScanLine];
    UINTN line_blocks64 = line_size / 2;

    for (UINTN i = 0; i < line_blocks64; i++)
        last_line64[i] = 0;

    // Dernier pixel si impair
    if (line_size % 2)
        last_line64[line_blocks64 * 2] = 0;

    CursorY = MaxLines - 1;

}

UINT16 GetCharIndex(CHAR16 c){
    for (int i = 0; i < 749; i++) {
        if (font_default_code_points[i] == c)
            return i;
    }
    return 32;
}

void RenderChar(CHAR16 c,UINT32 Color){
    if(c==L'\n'){
        CursorX=0;
        CursorY++;
        if(CursorY>=MaxLines)
            Scroll();
        
    } else if (c==L'\b'){
        if(CursorX!=0)
            CursorX--;
    }   else if (c == L'\r') {
        CursorX = 0;
    }
     else {
        UINT16 index = GetCharIndex(c);
        for(CHAR8 Y = 0;Y<LINE_HEIGHT;Y++){
            UINT8 line_byte = font_default_data[index][Y][0];
            for(CHAR8 X = 0;X<8;X++){
                if (line_byte & (1 << (7 - X)))
                    RenderPixel(Color, CursorX*8 + X, CursorY*LINE_HEIGHT + Y);
                else 
                    RenderPixel(0, CursorX*8 + X, CursorY*LINE_HEIGHT + Y);
                
            }
        }
        CursorX++;
    }
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

EFI_STATUS SetCursor(INT64 X,INT64 Y){
    if(X>=0)
        if(X<=MaxChar)
            CursorX=X;
    if(Y>=0)
        if(Y<=MaxLines)
            CursorY=Y;
}

GopModeList* GetModeList(UINTN* Count){
    *Count = gop->Mode->MaxMode;
    GopModeList* ModeList = AllocatePool(*Count * sizeof(GopModeList));
    EFI_STATUS status;
    for (UINT32 i = 0; i < *Count; i++) {
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
    if(EFI_ERROR(status)) return status;
    GopInfo = gop->Mode->Info;
    ActualFramebuffer = (UINT32*)(UINTN)gop->Mode->FrameBufferBase;
    FreePool(Framebuffer);
    Framebuffer = kmalloc((GopInfo->VerticalResolution) * GopInfo->PixelsPerScanLine*sizeof(UINT32));
    FillDisplay(0);

    CursorX = 0;
    CursorY = 0;

    MaxChar = GopInfo->HorizontalResolution / 8;
    MaxLines = GopInfo->VerticalResolution / LINE_HEIGHT;
}
