#include <efi.h>
#include <efilib.h>
#include "display.h"
#include "pci.h"
#include "io.h"

PCI_DEVICE PciList[256];
UINTN PciCount = 0;
CHAR16 *PciClassDescrption[18] = {
    L"Other",
    L"Mass storage",
    L"Network",
    L"Display",
    L"Multimedia",
    L"Memory",
    L"Bridge",
    L"Simple com.",
    L"Base system peripherals",
    L"Input device",
    L"Docking station",
    L"CPU",
    L"Serial bus",
    L"Wireless",
    L"I/O",
    L"Satellite",
    L"Encryption/decryption",
    L"Data acquisition"
};

UINT32 PciRead32(UINT8 bus, UINT8 device, UINT8 function, UINT8 offset){
    outl(0xCF8, PciAdress(bus, device, function, offset));
    return inl(0xCFC);
}

EFI_STATUS EnumeratePci(){
    CPrint(THEME_INFO,L"Enumerating PCI device ...\n");
    EnumeratePciBus(0,TRUE);
    CPrint(THEME_SUCCESS,L"PCI device enumerated !\n"); 
    return EFI_SUCCESS;
}

EFI_STATUS EnumeratePciBus(UINT8 bus, BOOLEAN ScanSubBus){
    for(UINT8 device = 0; device < 32; device++){
        UINT16 vendor = PciRead32(bus, device, 0, 0x00) & 0xFFFF;
        if(vendor==0xFFFF)continue; //No device
        UINT8 header = (PciRead32(bus, device, 0, 0x0C) >> 16) & 0xFF;
        UINT8 functions = (header & 0x80) ? 8 : 1;
        for(UINT8 func = 0; func < functions; func++){
            UINT32 data00 = PciRead32(bus, device, func, 0x00);
            if((data00&0xFFFF) == 0xFFFF)
                continue;
            UINT32 data08 = PciRead32(bus,device,func,0x08);
            PciList[PciCount].bus=bus;
            PciList[PciCount].device=device;
            PciList[PciCount].func=func;
            PciList[PciCount].vendor_id = data00 & 0xFFFF;
            PciList[PciCount].device_id = (data00 >> 16) & 0xFFFF;
            PciList[PciCount].class_code = (data08 >> 24) & 0xFF;
            PciList[PciCount].subclass = (data08 >> 16) & 0xFF;
            PciList[PciCount].prog_if = (data08 >> 8)  & 0xFF;
            if(((data00 >> 24) & 0xFF) == 0x06 && ((data00 >> 16) & 0xFF) == 0x04 && ScanSubBus){//PCI Bridge
                UINT32 buses = PciRead32(bus, device, func, 0x18);
                UINT8 secondary = (buses >> 8) & 0xFF;
                EnumeratePciBus(secondary,TRUE);
            } else {
                PciList[PciCount].bar[0] = PciRead32(bus,device,func,0x10);
                PciList[PciCount].bar[1] = PciRead32(bus,device,func,0x14);
                PciList[PciCount].bar[2] = PciRead32(bus,device,func,0x18);
                PciList[PciCount].bar[3] = PciRead32(bus,device,func,0x1C);
                PciList[PciCount].bar[4] = PciRead32(bus,device,func,0x20);
                PciList[PciCount].bar[5] = PciRead32(bus,device,func,0x24);
                PciCount++;
            }

        }
    }
    return EFI_SUCCESS;
}
