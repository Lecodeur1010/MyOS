#pragma once

#include <efi.h>
#include <efilib.h>

typedef struct __attribute__((packed)) {
    uint8_t  IDLength;
    uint8_t  ColorMapType;
    uint8_t  ImageType;
    
    // ColorMap Specification (5 octets au total)
    uint16_t ColorMapFirstIndex;
    uint16_t ColorMapLength;
    uint8_t  ColorMapEntrySize;
    
    // Image Specification
    uint16_t XOrigin;
    uint16_t YOrigin;
    uint16_t Width;
    uint16_t Height;
    uint8_t  BPP;
    uint8_t  ImageDescriptor;
} TGAHeader;

EFI_STATUS RenderPixel(UINT32 Color,UINT32 x,UINT32 y);
EFI_STATUS RenderPixelBypass(UINT32 Color,UINT32 x,UINT32 y);
EFI_STATUS FillDisplay(UINT32 Color);
EFI_STATUS LoadTGA(CHAR16* filename, UINTN startX, UINTN startY);