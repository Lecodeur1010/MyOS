#include "graphics.h"
#include "display.h"
#include "disk.h"
#include "func.h"
#include <efi.h>
#include <efilib.h>

EFI_STATUS RenderPixel(UINT32 Color,UINT32 x,UINT32 y){
    UINT64 Pos = (GopInfo->PixelsPerScanLine)*y+x;
    Framebuffer[Pos]=Color;
    return EFI_SUCCESS;
}

EFI_STATUS FillDisplay(UINT32 Color){
    for(UINTN pos = 0;pos<(GopInfo->VerticalResolution*GopInfo->PixelsPerScanLine);pos++){
        Framebuffer[pos]=Color;
    }
    Actualize();
    
    return EFI_SUCCESS;
}

EFI_STATUS LoadTGA(CHAR16* filename, UINTN startX, UINTN startY){
    FS_NODE* Node;
    EFI_STATUS status = VFSOpen(ActualNode,filename,&Node,EFI_FILE_MODE_READ,0);
    CHECK_STATUS(status);
    void* Buffer = NULL;
    UINTN Size = 0;
    status = VFSRead(Node,&Buffer,&Size);
    CHECK_STATUS(status);
    TGAHeader* Header = (TGAHeader*)Buffer;
    BOOLEAN isTopDown = (Header->ImageDescriptor & 0x20) != 0;
    if(Header->ColorMapType!=0||Header->IDLength!=0||Header->ImageType!=2||(Header->BPP!=32&&Header->BPP!=24)){
        kfree(Buffer);
        return EFI_UNSUPPORTED;
    }
    UINT32* Data = (UINT32*)((uint8_t*)Buffer + sizeof(TGAHeader));
    if (startX + Header->Width > GopInfo->HorizontalResolution || 
        startY + Header->Height > GopInfo->VerticalResolution) { // <--- Vertical !
        kfree(Buffer);
        return EFI_INVALID_PARAMETER; // Plus précis que EFI_UNSUPPORTED
    }
    if(Header->BPP==32){
        for(UINTN y = 0; y < Header->Height; y++){
            for(UINTN x = 0; x < Header->Width; x++){
                UINTN sourceY = isTopDown ? y : (Header->Height - 1 - y);
                UINT32 raw = Data[sourceY * Header->Width + x];
                UINT8 b = (raw >> 0)  & 0xFF;
                UINT8 g = (raw >> 8)  & 0xFF;
                UINT8 r = (raw >> 16) & 0xFF;
                RenderPixel(RGB(r, g, b), startX + x, startY + y);
            }
        }
    } else {
        for(UINTN y = 0; y < Header->Height; y++){
            for(UINTN x = 0; x < Header->Width; x++){
                UINTN sourceY = isTopDown ? y : (Header->Height - 1 - y);
                UINT8* pixelData = (UINT8*)Data + (sourceY * Header->Width + x) * 3;
                UINT8 b = pixelData[0];
                UINT8 g = pixelData[1];
                UINT8 r = pixelData[2];
                RenderPixel(RGB(r, g, b), startX + x, startY + y);
            }
        }
    }
    return EFI_SUCCESS;
}