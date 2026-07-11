#pragma once

#include <efi.h>
#include <efilib.h>
#include "disk.h"

extern FS_NODE* ShellNode;
typedef struct {
    UINTN SizeX;
    UINTN SizeY;
} GopModeList;

extern UINTN ModeCount;
extern EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* GopInfo;
extern UINT32 *Framebuffer;
UINT32 RGB(UINT8 Red,UINT8 Green,UINT8 Blue);

EFI_STATUS SetCursor(INT64 X,INT64 Y);
EFI_STATUS GetCursor(INT64* X,INT64* Y);
EFI_STATUS GopInit();
void Actualize();
void TemporaryBuffer(BOOLEAN State);
void CPrintWait(BOOLEAN State);
void CPrint(UINT32 color, CONST CHAR16 *fmt, ...);
void CPrintFree(UINT32 PosX, UINT32 PosY, UINT32 color, CONST CHAR16 *fmt, ...);
void ShellPrint(UINT32 color, CONST CHAR16 *fmt, ...);
void RenderString(CHAR16* buffer,UINT32 Color);
UINT16 GetCharIndex(CHAR16 c);
GopModeList* GetModeList();
EFI_STATUS SetMode(UINTN Mode);