#include <efi.h>
#include <efilib.h>

#ifndef PCI_H
#define PCI_H

typedef struct {
    UINT8 bus;
    UINT8 device;
    UINT8 func;
    UINT16 vendor_id;
    UINT16 device_id;

    UINT8 class_code;
    UINT8 subclass;
    UINT8 prog_if;

    UINT32 bar[6];
} PCI_DEVICE;

extern PCI_DEVICE PciList[256];
extern UINTN PciCount;
extern CHAR16 *PciClassDescrption[18];

static inline UINT32 PciAdress(UINT8 bus,UINT8 device,UINT8 function,UINT8 offset){
    return (1 << 31) | (bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC);
}
EFI_STATUS EnumeratePciBus(UINT8 bus, BOOLEAN ScanSubBus);
EFI_STATUS EnumeratePci();


#endif