#include <efi.h>
#include <efilib.h>
#include "io.h"
#include "disk.h"
#include "display.h"
#include "memory.h"
#include "fat.h"
#include "pci.h"
#include "func.h"

UINTN DiskCount = 0;
DISK Disklist[32];

//----------------------------------------------------IDE-----------------------------------------------------------------------------------

UINTN IDEDiskCount = 0;

EFI_STATUS AddDisk(DISK* disk){
    if(!disk) return EFI_INVALID_PARAMETER;
    Disklist[DiskCount].read=disk->read;
    Disklist[DiskCount].write=disk->write;
    Disklist[DiskCount].handle=disk->handle;
    Disklist[DiskCount].sector_size=disk->sector_size;
    Disklist[DiskCount].total_sectors=disk->total_sectors;

    DiskCount++;
    return EFI_SUCCESS;

}

VOID IDEWait(UINT16 io_base){
    for(UINTN k = 0; k < 4; k++)inb(io_base+7);
}

EFI_STATUS IDEWaitBusy(UINT16 io_base){
    for (UINTN i = 0; i < 10000; i++) {
        if (!(inb(io_base + 7) & 0x80))
            return EFI_SUCCESS;
    }
    return EFI_TIMEOUT;
}

EFI_STATUS IDEWaitDrq(UINT16 io_base)
{
    for (UINTN i = 0; i < 1000000; i++) {
        uint8_t s = inb(io_base + 7);

        if ((s & 0x01) || (s & 0x20) || (s == 0xFF) || (!s))
            return EFI_DEVICE_ERROR;

        if (s & 0x08)
            return EFI_SUCCESS;
    }

    return EFI_TIMEOUT;
}

EFI_STATUS IDEProbe(UINT16 io_base, UINT8 drive){
    outb(io_base + 6, drive); //Select disk + wait
    IDEWait(io_base);
    outb(io_base + 7, 0xEC); //Ask of identification
    EFI_STATUS status = IDEWaitDrq(io_base); //Check
    if (EFI_ERROR(status))return EFI_DEVICE_ERROR;
    UINT16 tmp[256];
    insw(io_base, tmp, 256);

    return EFI_SUCCESS;
}

EFI_STATUS IDEReadLBA(UINT64 handle, UINT64 lba, VOID *ptr){
    UINT16 io_base = !(handle & 0b10) ? IDE_PRIMAIRY : IDE_SECONDARY;
    UINT8 drive    = !(handle & 0b1)  ? IDE_MASTER   : IDE_SLAVE;
    EFI_STATUS status = FALSE;
    IDEProbe(io_base, drive);
    if (EFI_ERROR(status)) return status;
    status = IDEWaitBusy(io_base);
    if (EFI_ERROR(status)) return EFI_TIMEOUT;
    outb(io_base + 6, 0xE0 | drive | ((lba >> 24) & 0x0F));
    IDEWait(io_base);
    outb(io_base + 2, 1);
    outb(io_base + 3, (UINT8)(lba)); // LBA low
    outb(io_base + 4, (UINT8)(lba >> 8)); //LBA mid
    outb(io_base + 5, (UINT8)(lba >> 16)); //LBA high
    outb(io_base + 7, 0x20);
    IDEWait(io_base);
    status = IDEWaitBusy(io_base);
    if (EFI_ERROR(status)) return EFI_TIMEOUT;
    status = IDEWaitDrq(io_base);
    if (EFI_ERROR(status)) return status;
    insw(io_base, ptr, 256);
    return EFI_SUCCESS;

}

EFI_STATUS IDEWriteLBA(UINT64 handle, UINT64 lba, VOID *ptr){
    UINT16 io_base = !(handle & 0b10) ? IDE_PRIMAIRY : IDE_SECONDARY;
    UINT8  drive   = !(handle & 0b1)  ? IDE_MASTER   : IDE_SLAVE;
    EFI_STATUS status = IDEProbe(io_base, drive); //Check disk
    if (EFI_ERROR(status)) return status;
    status = IDEWaitBusy(io_base); //Wait for ready
    if (EFI_ERROR(status)) return EFI_TIMEOUT;

    // 3. select drive + LBA
    outb(io_base + 6, drive | ((lba >> 24) & 0x0F));
    IDEWait(io_base);
    outb(io_base + 2, 1); // Nb sector read : 1
    outb(io_base + 3, (uint8_t)(lba));
    outb(io_base + 4, (uint8_t)(lba >> 8));
    outb(io_base + 5, (uint8_t)(lba >> 16));
    outb(io_base + 7, 0x30); //Write order
    status = IDEWaitDrq(io_base); //Wait
    if (EFI_ERROR(status)) return status;
    outsw(io_base, ptr, 256); //Write; count in word, not byte
    outb(io_base + 7, 0xE7); //Clear cache
    status = IDEWaitBusy(io_base); //Wait
    if (EFI_ERROR(status)) return EFI_TIMEOUT;

    return EFI_SUCCESS;
}

EFI_STATUS IDEInit(){
    for(UINTN i = 0; i <4 ; i++){//Check IDE drives
        UINT8 buffer[512];
        EFI_STATUS status = IDEReadLBA(i,0,buffer);
        if(EFI_ERROR(status)) continue;
        IDEDiskCount++;
        DISK ldisk = {.handle = i,.read = IDEReadLBA,.write = IDEWriteLBA,.sector_size=512,.total_sectors=0}; //Would be mf it sector_size != 512
        AddDisk(&ldisk);
        CheckForFSAndAdd(&(Disklist[DiskCount-1]));
    }
    return IDEDiskCount!=0 ? EFI_SUCCESS : EFI_ABORTED;
}

//----------------------------------------------------AHCI-----------------------------------------------------------------------------------

AHCI_DEVICE* AHCIDeviceList = NULL;
UINTN AHCIDiskCount = 0;

EFI_STATUS AHCIInit(){
    EFI_STATUS status;
    for(UINTN i = 0; i < PciCount; i++){
        if(PciList[i].class_code == 0x01)
            if(PciList[i].subclass == 0x06)
                if(PciList[i].prog_if == 0x01){
                    status = AHCIControllerInit(i);
                }
    }
    return status;
}

EFI_STATUS AHCIControllerInit(UINT32 pciIndex){
    UINT64 addr = PciList[pciIndex].bar[5] & ~0xFULL;
    volatile HBA_MEM* abar = (volatile HBA_MEM*)addr;
    abar->ghc |= (1U << 31); //Set bit AHCI Enable high
    abar->ghc |= (1U << 0);
    while (abar->ghc & (1U << 0)); //Reset HR
    abar->ghc |= (1U << 31);
    UINT32 pi = abar->pi; //Ports
    for(UINT8 portIndex = 0; portIndex < 32; portIndex++){
        if (!(pi & (1U << portIndex))) continue; //No port
        volatile HBA_PORT* port = &abar->ports[portIndex]; //Read port
        port->cmd &= ~((1U << 0) | (1U << 4)); // Stop motor
        while (port->cmd & ((1U << 15) | (1U << 14))); //Wait for it
        while (port->cmd & (1U << 15)); // CR
        while (port->cmd & (1U << 14)); // FR
        VOID* ptrTemp = kmalloc(1024);
        if(!ptrTemp) return EFI_OUT_OF_RESOURCES;
        SetMem(ptrTemp, 1024, 0);
        port->clb = (UINT32)(UINTN)ptrTemp;
        port->clbu = (UINT32)(((UINT64)(UINTN)ptrTemp) >> 32);
        ptrTemp = kmalloc(256);
        if(!ptrTemp) return EFI_OUT_OF_RESOURCES;
        SetMem(ptrTemp, 256, 0);
        port->fb = (UINT32)(UINTN)ptrTemp;
        port->fbu = (UINT32)(((UINT64)(UINTN)ptrTemp) >> 32);
        port->serr = 0xFFFFFFFF;
        port->is = 0xFFFFFFFF;
        port->cmd |= (1U << 4); // Start motor
        port->cmd |= (1U << 0);
        for (int i=0;i<100000;i++)
            if (port->sig == 0x00000101) break;
        if (port->sig != 0x00000101) continue; //Invalid signature
        UINT64 clb_addr = ((UINT64)port->clbu << 32) | port->clb;
        HBA_CMD_HEADER* cmdheader = (HBA_CMD_HEADER*)(UINTN)clb_addr;
        for (int i = 0; i < 32; i++) {
            VOID* ctba = kmalloc(256);
            if(!ctba) return EFI_OUT_OF_RESOURCES;
            SetMem(ctba, 256, 0);
            cmdheader[i].prdtl = 1;
            cmdheader[i].ctba = (UINT32)(UINTN)ctba;
            cmdheader[i].ctbau = (UINT32)(((UINT64)(UINTN)ctba) >> 32);
        }
        EFI_STATUS status = EFI_SUCCESS;
        if(!AHCIDeviceList) AHCIDeviceList = kmalloc(sizeof(AHCI_DEVICE));
        else status = krealloc((VOID**)&AHCIDeviceList,sizeof(AHCI_DEVICE)*(AHCIDiskCount+1));
        if(EFI_ERROR(status)||!AHCIDeviceList) return EFI_OUT_OF_RESOURCES;
        AHCIDeviceList[AHCIDiskCount].port = portIndex;
        AHCIDeviceList[AHCIDiskCount].hba_port = port;
        AHCIDeviceList[AHCIDiskCount].cmd_header = cmdheader;
        DISK ldisk = {.handle = (UINT64)&AHCIDeviceList[AHCIDiskCount],.read=AHCIReadLBA,.write=NULL,.sector_size=512,.total_sectors=0}; //Would be mf it sector_size != 512
        AddDisk(&ldisk);
        AHCIDiskCount++;
        status = CheckForFSAndAdd(&(Disklist[DiskCount-1]));
    } 
    
    return EFI_SUCCESS;
} 

EFI_STATUS AHCIReadLBA(UINT64 handle, UINT64 lba, VOID *ptr){
    AHCI_DEVICE* dev = (AHCI_DEVICE*)handle;
    volatile HBA_PORT* port = dev->hba_port;
    UINT32 slots = port->sact | port->ci; //Find free slot
    int slot = -1;
    for (int i = 0; i < 32; i++) {
        if (!(slots & (1U << i))) {
            slot = i;
            break;
        }
    }
    if (slot == -1)
        return EFI_DEVICE_ERROR; //None found
    port->is = 0xFFFFFFFF;
    UINT64 clb_addr = ((UINT64)port->clbu << 32) | port->clb;
    HBA_CMD_HEADER* cmdheader = (HBA_CMD_HEADER*)(UINTN)clb_addr; //CMD struture
    HBA_CMD_HEADER* hdr = &cmdheader[slot];
    UINT64 ctba_addr =((UINT64)hdr->ctbau << 32) | hdr->ctba;
    HBA_CMD_TBL* cmdtbl = (HBA_CMD_TBL*)(UINTN)ctba_addr;
    SetMem(cmdtbl, 256, 0);
    hdr->cfl = sizeof(FIS_REG_H2D) / sizeof(UINT32);//Header (read)
    hdr->w = 0;
    hdr->prdtl = 1;
    cmdtbl->prdt_entry[0].dba = (UINT32)(UINTN)ptr; //PRDT
    cmdtbl->prdt_entry[0].dbau = (UINT32)(((UINT64)(UINTN)ptr) >> 32);
    cmdtbl->prdt_entry[0].dbc = 511; // 512 bytes - 1
    cmdtbl->prdt_entry[0].i = 0;

    FIS_REG_H2D* fis = (FIS_REG_H2D*)(&cmdtbl->cfis); //FIS
    SetMem(fis, sizeof(FIS_REG_H2D), 0);

    fis->fis_type = 0x27;
    fis->c = 1; // READ DMA EXT
    fis->countl = 1; // 1 sector
    fis->command = 0x25; // READ DMA EXT (LBA48)
    fis->lba0 = (UINT8)lba;
    fis->lba1 = (UINT8)(lba >> 8);
    fis->lba2 = (UINT8)(lba >> 16);
    fis->lba3 = (UINT8)(lba >> 24); // LBA24-31
    fis->lba4 = 0;                  // LBA32-39
    fis->lba5 = 0;                  // LBA40-47
    fis->device = 1 << 6;           // LBA mode bit
    while (port->tfd & (0x80 | 0x08)); //Wait for ready
    port->ci |= (1U << slot); //Start the cmd;
    while (port->ci & (1U << slot)) { //Wait
        if (port->tfd & 0x01){
            return EFI_DEVICE_ERROR;
        }
    }
    return EFI_SUCCESS;
}

