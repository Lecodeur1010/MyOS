#include <efi.h>
#include <efilib.h>
#ifndef DISK_H
#define DISK_H
#define IDE_PRIMAIRY 0x1F0
#define IDE_SECONDARY 0x170
#define IDE_MASTER 0xE0
#define IDE_SLAVE 0xF0

extern UINTN IDEDiskCount;

EFI_STATUS IDEReadLBA(UINT64 handle, UINT64 lba, VOID *ptr);
EFI_STATUS IDEWriteLBA(UINT64 handle, UINT64 lba, VOID *ptr);
EFI_STATUS IDEInit();

typedef struct {
    UINTN handle;
    EFI_STATUS (*read)(UINT64 handle, UINT64 lba, VOID *ptr);
    EFI_STATUS (*write)(UINT64 handle, UINT64 lba, VOID *ptr);
    UINT64 total_sectors;
    UINT32 sector_size;
} DISK;

//----------------------------------------------------------MBR----------------------------------------------------------------------------

typedef struct  __attribute__((__packed__)) {
    UINT8 loader[446];
    struct __attribute__((packed)) {
        UINT8  status;
        UINT8  chs_first[3];
        UINT8  type;
        UINT8  chs_last[3];
        UINT32 lba_start;
        UINT32 sector_count;
    } partitions[4];
    UINT16 signature;
} MBR ;

//----------------------------------------------------------GPT----------------------------------------------------------------------------

typedef struct __attribute__((packed)){
    CHAR8  signature[8];       // "EFI PART"
    UINT32 revision;
    UINT32 header_size;
    UINT32 header_crc32;
    UINT32 reserved;
    UINT64 current_lba;
    UINT64 backup_lba;
    UINT64 first_usable_lba;
    UINT64 last_usable_lba;
    UINT8  disk_guid[16];
    UINT64 partition_entries_lba;
    UINT32 num_partition_entries;
    UINT32 sizeof_partition_entry;
    UINT32 partition_entries_crc32;
} GPT_HEADER;

typedef struct __attribute__((packed)){
    UINT8  partition_type_guid[16];
    UINT8  unique_partition_guid[16];
    UINT64 starting_lba;
    UINT64 ending_lba;
    UINT64 attributes;
    CHAR16 partition_name[36];
} GPT_ENTRY;

//----------------------------------------------------AHCI-----------------------------------------------------------------------------------

typedef volatile struct __attribute__((packed)){
    UINT32 clb;       // 0x00
    UINT32 clbu;      // 0x04
    UINT32 fb;        // 0x08
    UINT32 fbu;       // 0x0C
    UINT32 is;        // 0x10
    UINT32 ie;        // 0x14
    UINT32 cmd;       // 0x18
    UINT32 reserved0; // 0x1C
    UINT32 tfd;       // 0x20
    UINT32 sig;       // 0x24
    UINT32 ssts;      // 0x28
    UINT32 sctl;      // 0x2C
    UINT32 serr;      // 0x30
    UINT32 sact;      // 0x34
    UINT32 ci;        // 0x38
    UINT32 sntf;      // 0x3C
    UINT32 fbs;       // 0x40
    UINT32 reserved1[11];
    UINT32 vendor[4];
} HBA_PORT;

typedef volatile struct __attribute__((packed)){
    UINT32 cap;
    UINT32 ghc;
    UINT32 is;
    UINT32 pi;
    UINT32 vs;
    UINT32 ccc_ctl;
    UINT32 ccc_pts;
    UINT32 em_loc;
    UINT32 em_ctl;
    UINT32 cap2;
    UINT32 bohc;
    UINT8 reserved[0xA0 - 0x2C];
    UINT8 vendor[0x100 - 0xA0];
    HBA_PORT ports[32];
} HBA_MEM;

typedef struct __attribute__((packed)){
    UINT8 cfl:5;
    UINT8 a:1;
    UINT8 w:1;
    UINT8 p:1;

    UINT8 r:1;
    UINT8 b:1;
    UINT8 c:1;
    UINT8 reserved0:1;
    UINT8 pmp:4;

    UINT16 prdtl;

    volatile UINT32 prdbc;

    UINT32 ctba;
    UINT32 ctbau;

    UINT32 reserved1[4];
} HBA_CMD_HEADER;

typedef struct __attribute__((packed)){
    UINT32 dba;
    UINT32 dbau;
    UINT32 reserved0;

    UINT32 dbc:22;
    UINT32 reserved1:9;
    UINT32 i:1;
} HBA_PRDT_ENTRY;

typedef struct __attribute__((packed)){
    UINT8 cfis[64];
    UINT8 acmd[16];
    UINT8 reserved[48];
    HBA_PRDT_ENTRY prdt_entry[1];
} HBA_CMD_TBL;

typedef struct __attribute__((packed)){
    UINT8 fis_type;

    UINT8 pmport:4;
    UINT8 reserved0:3;
    UINT8 c:1;

    UINT8 command;
    UINT8 featurel;

    UINT8 lba0;
    UINT8 lba1;
    UINT8 lba2;
    UINT8 device;

    UINT8 lba3;
    UINT8 lba4;
    UINT8 lba5;
    UINT8 featureh;

    UINT8 countl;
    UINT8 counth;
    UINT8 icc;
    UINT8 control;

    UINT8 reserved1[4];
} FIS_REG_H2D;

typedef struct {
    UINT8 port;
    volatile HBA_PORT* hba_port;
    HBA_CMD_HEADER* cmd_header;
} AHCI_DEVICE;

extern AHCI_DEVICE* AHCDeviceList;
extern UINTN AHCIDiskCount;

extern UINTN DiskCount;
extern DISK Disklist[32];

EFI_STATUS AHCIInit();
EFI_STATUS AHCIControllerInit(UINT32 pciIndex);
EFI_STATUS AHCIReadLBA(UINT64 handle, UINT64 lba, VOID *ptr);

#endif