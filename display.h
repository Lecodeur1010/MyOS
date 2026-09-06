#pragma once

#include <efi.h>
#include <efilib.h>
#include "disk.h"

extern FS_NODE* ShellNode;
typedef struct {
    UINTN SizeX;
    UINTN SizeY;
} GopModeList;

#define PSF2_MAGIC0 0x72
#define PSF2_MAGIC1 0xb5
#define PSF2_MAGIC2 0x4a
#define PSF2_MAGIC3 0x86
#define PSF2_MAGIC_SEPARATOR 0xFF
#define PSF2_START_SEQ       0xFE
#define PSF2_HAS_UNICODE_TABLE 0x01

typedef struct {
    uint8_t  magic[4];       // 0x72, 0xb5, 0x4a, 0x86 (Signature PSF2)
    uint32_t version;        // Toujours 0
    uint32_t headersize;     // Taille de l'en-tête en octets (généralement 32)
    uint32_t flags;          // Bit 0 = 1 si une table Unicode est présente à la fin
    uint32_t length;         // Nombre total de glyphes (ex: 256 ou 512)
    uint32_t charsize;       // Taille d'un glyphe en octets (ex: 16 pour du 8x16)
    uint32_t height;         // Hauteur en pixels (ex: 16)
    uint32_t width;          // Largeur en pixels (ex: 8)
} __attribute__((packed)) PSF2_HEADERS;

typedef struct {
    UINT8 magic[2];     // 0x36, 0x04
    UINT8 mode;         // Mode (flags) pour la table Unicode / taille
    UINT8 charsize;     // Hauteur du glyphe en octets (la largeur est TOUJOURS de 8px)
} __attribute__((packed)) PSF1_HEADER;

extern UINTN ModeCount;
extern EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* GopInfo;
extern UINT32 *Framebuffer;
extern UINT32 *ActualFramebuffer;
extern PSF2_HEADERS* ActualFontHeaders;
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
void RenderChar(CHAR16 c,UINT32 Color);
void Scroll();
void ToggleCursor();
UINT32 GetGlyphIndex(CHAR16 unicode_char);
GopModeList* GetModeList();
EFI_STATUS SetMode(UINTN Mode);
EFI_STATUS InitFont(VOID* FontBuffer);