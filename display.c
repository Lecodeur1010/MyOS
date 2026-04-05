#include "display.h"
#include <efi.h>
#include <efilib.h>
#include "func.h"
#include "memory.h"
#include "font.h"

#define CHAR_HEIGHT 16
#define CHAR_WIDTH 8



EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *GopInfo = NULL;
EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
UINT32 *Framebuffer = NULL;
UINT32 *ActualFramebuffer = NULL;
UINT32 CursorX = 0;
UINT32 CursorY = 0;
UINT32 MaxChar = 0;
UINT32 MaxLines = 0;
BOOLEAN WaitForActualize = FALSE;

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

    MaxChar = GopInfo->HorizontalResolution / CHAR_WIDTH;
    MaxLines = GopInfo->VerticalResolution / CHAR_HEIGHT;
    
    if(status)CPrint(THEME_WARNING,L"Warning : Error occured while setting se resolution to the highest one (%r)  ; default resolution used", status);
    return EFI_SUCCESS;
}

EFI_STATUS RenderPixel(UINT32 Color,UINT32 x,UINT32 y){
    UINT64 Pos = (GopInfo->PixelsPerScanLine)*y+x;
    Framebuffer[Pos]=Color;
    return EFI_SUCCESS;
}

EFI_STATUS DrawRectangle(UINT32 Color,UINT32 x,UINT32 y, UINT32 w, UINT32 h, BOOLEAN ActualBuffer){
    if(!ActualBuffer)
        for (UINTN i = x; i < x+w; i++)
            for (UINTN j = y; y< y+h; j++)
                Framebuffer[(GopInfo->PixelsPerScanLine)*j+i]=Color;        
    else 
        for (UINTN i = x; i < x+w; i++)
            for (UINTN j = y; y< y+h; j++)
                ActualFramebuffer[(GopInfo->PixelsPerScanLine)*j+i]=Color;
}

EFI_STATUS FillDisplay(UINT32 Color){
    for(UINTN pos = 0;pos<(GopInfo->VerticalResolution*GopInfo->PixelsPerScanLine);pos++){
        Framebuffer[pos]=Color;
    }
    Actualize();
    return EFI_SUCCESS;
}

void Actualize(){
    CopyMem(ActualFramebuffer,Framebuffer,(GopInfo->VerticalResolution) * GopInfo->PixelsPerScanLine*sizeof(UINT32));
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
    RenderString(buffer,color);
    kfree(buffer);
    if(!WaitForActualize)
    Actualize();
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
    UINTN line_size = GopInfo->PixelsPerScanLine * CHAR_HEIGHT;
    UINTN total_pixels = (GopInfo->VerticalResolution - CHAR_HEIGHT) * GopInfo->PixelsPerScanLine;

    CopyMem(Framebuffer,Framebuffer + line_size,total_pixels * sizeof(UINT32));
    UINTN* last_line64 = (UINTN*)&Framebuffer[(GopInfo->VerticalResolution - CHAR_HEIGHT) * GopInfo->PixelsPerScanLine];
    UINTN line_blocks64 = line_size / 2;

    for (UINTN i = 0; i < line_blocks64; i++)
        last_line64[i] = 0;

    if (line_size % 2)
        last_line64[line_blocks64 * 2] = 0;

    CursorY = MaxLines - 1;

}

UINT16 GetCharIndex(CHAR16 c){
    for (int i = 0; i < sizeof(font_default_code_points) / sizeof(CHAR16); i++) {
        if (font_default_code_points[i] == c)
            return i;
    }
    //Not found; trying '?'
    for (int i = 0; i < sizeof(font_default_code_points) / sizeof(CHAR16); i++) {
        if (font_default_code_points[i] == L'?')
            return i;
    }
    //Not found either - idc anymore
    return 0;
    
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
        for(CHAR8 Y = 0;Y<CHAR_HEIGHT;Y++){
            UINT8 line_byte = font_default_data[index][Y][0];
            for(CHAR8 X = 0;X<CHAR_WIDTH;X++){
                if (line_byte & (1 << (7 - X)))
                    RenderPixel(Color, CursorX*CHAR_WIDTH + X, CursorY*CHAR_HEIGHT + Y);
                else 
                    RenderPixel(0, CursorX*CHAR_WIDTH + X, CursorY*CHAR_HEIGHT + Y);
                
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
}

EFI_STATUS ToggleCursor(){
    for (int y = 0; y < CHAR_HEIGHT; y++) {
        for (int x = 0; x < CHAR_WIDTH; x++) {
            ActualFramebuffer[(CursorY*CHAR_HEIGHT + y) * GopInfo->PixelsPerScanLine +
                        (CursorX*CHAR_WIDTH + x)] ^= 0xFFFFFF; // inverse les couleurs
        }
    }
    return EFI_SUCCESS;
}
GOP_MODE_LIST* GetModeList(UINTN* Count){
    *Count = gop->Mode->MaxMode;
    GOP_MODE_LIST* ModeList = kmalloc(*Count * sizeof(GOP_MODE_LIST));
    if(!ModeList) return NULL;
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
    if(Framebuffer!=ActualFramebuffer)FreePool(Framebuffer);
    Framebuffer = kmalloc((GopInfo->VerticalResolution) * GopInfo->PixelsPerScanLine*sizeof(UINT32));
    if(!Framebuffer){
        //We skip double buffering
        Framebuffer=ActualFramebuffer;
    }
    FillDisplay(0);

    CursorX = 0;
    CursorY = 0;

    MaxChar = GopInfo->HorizontalResolution / 8;
    MaxLines = GopInfo->VerticalResolution / CHAR_HEIGHT;
}
