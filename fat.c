#include <efi.h>
#include <efilib.h>
#include "fat.h"
#include "disk.h"
#include "display.h"
#include "memory.h"
#include "func.h"
#include <stddef.h>

PARTITION* PartitionList = NULL;
UINT64 AvailableOSUID = 0;
FAT32_FS* Fat32List = NULL;
UINTN Fat32Count = 0;
UINTN PartitionCount = 0;
UINT32* FatCache = NULL;
UINT32 FatCacheDisk = (UINT32)-1; //None

CONST CHAR16* GetFSnameMBR(UINT8 type){
    switch(type){
    case 0x00 :
        return L"Unused/empty";
        break;
    case 0x01 :
        return L"FAT12";
        break;
    case 0x04 : case 0x06 : case 0x0E :
        return L"FAT16";
        break;
    case 0x0B: case 0x0C :
        return L"FAT32";
        break;
    case 0x82 : case 0x83 :
        return L"EXT";
        break;
    case 0x07 :
        return L"EXFAT/NTFS";
        break;
    case 0x27 :
        return L"WinRe";
        break;
    case 0xEE :
        return L"Protective GPT";
        break;
    default:
        return L"Other";
        break;
    }
}

EFI_STATUS CheckForFSAndAdd(DISK* disk){
    UINT8* buffer = kmalloc(512);
    EFI_STATUS status = disk->read(disk->handle,0,buffer);
    MBR* mbr = (MBR*)buffer;
    BOOLEAN foundCorrectEntry = FALSE;
    status = EFI_SUCCESS;
    if(EFI_ERROR(status)) return status;
    if(buffer[510]!=0x55 || buffer[511]!=0xAA) {return EFI_NOT_FOUND;} //Sig wrong, can't happen with MBR/FAT32
    for(UINTN i = 0; i < 4; i++){
        if(mbr->partitions[i].lba_start == 0x0 || mbr->partitions[i].sector_count == 0) continue; //Invalid entry
        EFI_STATUS statusl;
        CPrint(THEME_ERROR,L"%u\n",mbr->partitions[i].type);
        switch (mbr->partitions[i].type){
        case 0x0B: case 0x0C:
            CPrint(THEME_ERROR,L"fat32");
            statusl = CheckForFAT32AndAdd(disk,mbr->partitions[i].lba_start);
            break;
        default:
            CPrint(THEME_ERROR,L"exfat");
            PartitionList[PartitionCount].fsType = mbr->partitions[i].type;
            PartitionList[PartitionCount].fs = NULL;
            PartitionList[PartitionCount].disk = disk;
            PartitionList[PartitionCount].OSUID = AvailableOSUID;
            PartitionList[PartitionCount].sectorCount = mbr->partitions[i].sector_count;
            PartitionList[PartitionCount].startLba = mbr->partitions[i].lba_start;
            PartitionCount++;
            AvailableOSUID++;
            statusl = EFI_SUCCESS;
            break; 
        }  
        if(EFI_ERROR(statusl)) status = statusl;
        foundCorrectEntry=TRUE;
    }
    if(!foundCorrectEntry) status = CheckForFAT32AndAdd(disk,0);
    return status;
    
}

EFI_STATUS CheckForFAT32AndAdd(DISK* disk,UINT32 lbaStart)
{
    UINT8* buffer = kmalloc(512);
    EFI_STATUS status = disk->read(disk->handle,lbaStart,buffer);
    CheckError(status);
    FAT32_BPB* bpb = (FAT32_BPB*) buffer;
    if (buffer[510] != 0x55 || buffer[511] != 0xAA) return EFI_VOLUME_CORRUPTED; //BPB invalide
    if(strncmpa(bpb->fs_type, (CONST CHAR8*)"FAT32", 5)!=0) return EFI_UNSUPPORTED; //Not FAT32
    if(bpb->bytes_per_sector!=512) return EFI_UNSUPPORTED; //Prefer to stick to 512B sector
    if(bpb->sectors_per_cluster==0) return EFI_VOLUME_CORRUPTED; //impossible
    if(bpb->fat_size_32==0)return EFI_UNSUPPORTED; //Either FAT16 or corrupted
    if(bpb->root_cluster<2)return EFI_VOLUME_CORRUPTED; //Root cluster is never the first cluster
    //Disk checked, we can add it
    if(!Fat32List) Fat32List = kmalloc(32*sizeof(FAT32_FS));
    CheckBuffer(Fat32List);
    if(!PartitionList) PartitionList = kmalloc(32*sizeof(PARTITION));
    CheckBuffer(PartitionList);
    PartitionList[PartitionCount].fsType = 0x0C;
    PartitionList[PartitionCount].fs = &(Fat32List[Fat32Count]);
    PartitionList[PartitionCount].disk = disk;
    PartitionList[PartitionCount].OSUID = AvailableOSUID;
    PartitionList[PartitionCount].sectorCount = bpb->total_sectors_32;
    PartitionList[PartitionCount].startLba = lbaStart;
    PartitionCount++;
    AvailableOSUID++;
    
    Fat32Count++;
    
    CopyMem(&(((FAT32_FS*)PartitionList[PartitionCount-1].fs)->bpb),buffer,sizeof(FAT32_BPB));
    FAT32_FS* fs= PartitionList[PartitionCount-1].fs;
    fs->bytes_per_sector=bpb->bytes_per_sector;
    fs->fat_start_lba=lbaStart+bpb->reserved_sector_count;
    fs->data_start_lba=lbaStart+bpb->reserved_sector_count + (bpb->num_fats * bpb->fat_size_32);
    fs->root_dir_cluster=bpb->root_cluster;
    fs->sectors_per_cluster=bpb->sectors_per_cluster;
    return EFI_SUCCESS;
}

UINTN ClusterToLBA(PARTITION* partition, UINT32 cluster){
    if(partition->fsType != 0x0C) return 0;

    FAT32_FS* fs = (FAT32_FS*)partition->fs;
    return (cluster-2)*fs->sectors_per_cluster+fs->data_start_lba;
    //Cluster 0/1 are reserved
}

EFI_STATUS NextCluster(PARTITION* partition, UINT32 cluster){
    if(partition->fsType != 0x0C) return 0;
    if(FatCacheDisk != partition->OSUID){
        UINT32* temp = kmalloc(((FAT32_FS*)partition->fs)->bpb.fat_size_32*partition->disk->sector_size);
        if(!temp) return EFI_OUT_OF_RESOURCES;
        if(FatCache)kfree(FatCache);
        FatCache = temp;
        FatCacheDisk = partition->OSUID;
        UINT8* ptr = (UINT8*) FatCache;
        for(UINTN i = 0; i < ((FAT32_FS*)partition->fs)->bpb.fat_size_32; i++){
            EFI_STATUS status = partition->disk->read(partition->disk->handle,((FAT32_FS*)partition->fs)->fat_start_lba+i,ptr+(i*512));
            if(EFI_ERROR(status)) return status;
        }
    }
    UINT32 next = FatCache[cluster] & 0x0FFFFFFF;
    
    if (next >= 0x0FFFFFF8) { //Check if EOF
        return 0x0FFFFFFF; //EOF
    }
    return next;
}

EFI_STATUS ListDir(PARTITION* partition,UINT32 currentCluster, FAT_DIR_ENTRY** dirList, UINTN* validEntryCount){
    *validEntryCount = 0;
    FAT_DIR_ENTRY* ldirList = NULL;
    EFI_STATUS status;
    UINTN entryCount = 0;
    if(((FAT32_FS*)partition->fs)->bpb.bytes_per_sector!=512)return EFI_UNSUPPORTED; //Maybe trash & I doesn't support other than 512
    UINTN sizeDirTable = 0;
    while (currentCluster < 0x0FFFFFF8){
        UINT32 baseLba = ClusterToLBA(partition, currentCluster);
        for (UINTN s = 0; s < ((FAT32_FS*)partition->fs)->sectors_per_cluster; s++) {
            sizeDirTable += 512;

            if (!ldirList)
                ldirList = kmalloc(sizeDirTable);
            else {
                status = krealloc((VOID**)&ldirList, sizeDirTable);
                if (EFI_ERROR(status))
                    return status;
            }
            CheckBuffer(ldirList);
            status = partition->disk->read(partition->disk->handle,baseLba + s,((UINT8*)ldirList) + (entryCount * sizeof(FAT_DIR_ENTRY)));
            if (EFI_ERROR(status)) return status;
            UINTN base = entryCount;
            for (UINTN i = base; i < base + 16; i++) {
                entryCount++;
                if (ldirList[i].name[0] == 0x00)
                    goto found;
                if (ldirList[i].name[0] == 0xE5) continue;
                if (ldirList[i].attr == 0x08) continue;
                if (ldirList[i].attr == 0x0F) continue;
                (*validEntryCount)++;
            }
        }
        currentCluster = NextCluster(partition,currentCluster);
    }
    found:
    *dirList = kmalloc((*validEntryCount) * sizeof(FAT_DIR_ENTRY));
    FAT_DIR_ENTRY* dest = *dirList;
    CheckBuffer(*dirList);

    for (UINTN i = 0; i < entryCount; i++) {
        if (ldirList[i].name[0] == 0xE5) continue;
        if (ldirList[i].attr == 0x08) continue;
        if (ldirList[i].attr == 0x0F) continue;
        if (ldirList[i].name[0] == 0x00) break;
        CopyMem(dest, &ldirList[i], sizeof(FAT_DIR_ENTRY));
        dest++;
    }
    return EFI_SUCCESS;
}
