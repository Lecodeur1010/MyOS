#include "display.h"
#include <efi.h>
#include <efilib.h>
#include "func.h"

EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* GopInfo = NULL;
EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
UINT32 *Framebuffer;

UINT32 RGB(UINT8 Red, UINT8 Green, UINT8 Blue){
    if(!GopInfo)return 0;
    if(GopInfo->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) return Red | (Green << 8) | (Blue << 16);
    if(GopInfo->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) return Blue | (Green << 8) | (Red << 16);
    return 0;
}

EFI_STATUS GopInit(){
    uefi_call_wrapper(BS->LocateProtocol,&EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,NULL,&gop);
    GopInfo=gop->Mode->Info;
    Framebuffer=gop->Mode->FrameBufferBase;
    return EFI_SUCCESS;
}

EFI_STATUS SetPixel(UINT32 Color,UINT32 x,UINT32 y){
    UINT64 Pos = (GopInfo->PixelsPerScanLine)*y+x;
    Framebuffer[Pos]=Color;
    return EFI_SUCCESS;
}

EFI_STATUS FillDisplay(UINT32 Color){
    for(UINTN y = 0;y<GopInfo->VerticalResolution;y++){
        for(UINTN x = 0;x<GopInfo->HorizontalResolution;x++){
            SetPixel(Color,x,y);
        }
    }
    return EFI_SUCCESS;
}

void CPrint(UINTN color, CONST CHAR16 *fmt, ...){
    va_list args;
    CHAR16 buffer[256];

    va_start(args, fmt);    
    UnicodeVSPrint(buffer, sizeof(buffer), fmt, args);
    va_end(args);                            
    uefi_call_wrapper(gST->ConOut->SetAttribute,2,gST->ConOut, color); 
    uefi_call_wrapper(gST->ConOut->OutputString,2,gST->ConOut, buffer);
    uefi_call_wrapper(gST->ConOut->SetAttribute,2,gST->ConOut, THEME_INFO);
}