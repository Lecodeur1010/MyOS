#include <efi.h>
#include <efilib.h>
#include "disk.h"
#ifndef FAT_H
#define FAT_H

typedef struct __attribute__((packed)) {

    UINT8  jump_boot[3];
    CHAR8  oem_name[8];

    UINT16 bytes_per_sector;      // usually 512
    UINT8  sectors_per_cluster;   // power of 2
    UINT16 reserved_sector_count; // usually 32 for FAT32

    UINT8  num_fats;              // usually 2
    UINT16 root_entry_count;      // 0 for FAT32
    UINT16 total_sectors_16;      // 0 if > 65535
    UINT8  media;

    UINT16 fat_size_16;           // 0 for FAT32

    UINT16 sectors_per_track;
    UINT16 number_of_heads;
    UINT32 hidden_sectors;
    UINT32 total_sectors_32;
    UINT32 fat_size_32;           // sectors per FAT
    UINT16 ext_flags;
    UINT16 fs_version;
    UINT32 root_cluster;          // Root dir
    UINT16 fs_info_sector;        // usually 1
    UINT16 backup_boot_sector;    // usually 6

    UINT8  reserved[12];

    UINT8  drive_number;
    UINT8  reserved1;
    UINT8  boot_signature;        // 0x29

    UINT32 volume_id;
    CHAR8  volume_label[11];
    CHAR8  fs_type[8];
    UINT8  boot_code[420];
    UINT16 signature; //0x55AA

} FAT32_BPB;


typedef struct {
    FAT32_BPB bpb;
    UINT32 fat_start_lba;
    UINT32 data_start_lba;
    UINT32 root_dir_cluster;
    UINT16 bytes_per_sector;
    UINT8  sectors_per_cluster;
} FAT32_FS;

typedef struct  __attribute__((__packed__)) {
    CHAR8 name[11];
    UINT8 attr;
    UINT8 reserved;
    UINT8 creation_time_ms;
    UINT16 creation_time;
    UINT16 creation_date;
    UINT16 access_date;
    UINT16 first_cluster_high;
    UINT16 write_time;
    UINT16 write_date;
    UINT16 first_cluster_low;
    UINT32 file_size;
} FAT_DIR_ENTRY;

typedef struct {
    DISK* disk;
    UINT64 startLba;
    UINT64 sectorCount;
    UINT64 OSUID;
    UINT8 fsType;
    VOID* fs;
} PARTITION;

CONST CHAR16* GetFSnameMBR(UINT8 type);
EFI_STATUS CheckForFSAndAdd(DISK* disk);
EFI_STATUS CheckForFAT32AndAdd(DISK* disk,UINT32 lbaStart);
EFI_STATUS ListDir(PARTITION* drive,UINT32 currentCluster, FAT_DIR_ENTRY** dirList, UINTN* entryCount);
extern PARTITION* PartitionList;
extern UINTN PartitionCount;
extern UINT64 AvailableOSUID;

#endif 