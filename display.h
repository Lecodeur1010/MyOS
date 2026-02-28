#include <efi.h>
#include <efilib.h>

#ifndef DISPLAY
#define DISPLAY

extern EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* GopInfo;
UINT32 RGB(UINT8 Red,UINT8 Green,UINT8 Blue);

EFI_STATUS GopInit();
EFI_STATUS SetPixel(UINT32 Color,UINT32 x,UINT32 y);
EFI_STATUS FillDisplay(UINT32 Color);

#endif