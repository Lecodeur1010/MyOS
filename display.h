#include <efi.h>
#include <efilib.h>

#ifndef DISPLAY
#define DISPLAY

#define THEME_INFO RGB(255,255,255)
#define THEME_FILE_FILE RGB(0,127,255)
#define THEME_FILE_FOLDER RGB(255,255,0)
#define THEME_PROMPT RGB(255,255,0)
#define THEME_WARNING RGB(255,255,0)
#define THEME_ERROR RGB(255,0,0)
#define THEME_SUCCESS RGB(0,255,0)

typedef struct {
    UINTN SizeX;
    UINTN SizeY;
} GopModeList;


extern EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* GopInfo;
UINT32 RGB(UINT8 Red,UINT8 Green,UINT8 Blue);

EFI_STATUS SetCursor(INT64 X,INT64 Y);
EFI_STATUS GopInit();
EFI_STATUS RenderPixel(UINT32 Color,UINT32 x,UINT32 y);
EFI_STATUS FillDisplay(UINT32 Color);
void CPrint(UINT32 color, CONST CHAR16 *fmt, ...);
void RenderString(CHAR16* buffer,UINT32 Color);
UINT16 GetCharIndex(CHAR16 c);
GopModeList* GetModeList(UINTN* Count);
EFI_STATUS SetMode(UINTN Mode);
#endif