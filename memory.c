#include <efi.h>
#include <efilib.h>
#include "memory.h"
#include "vars.h"


BOOLEAN InBS=TRUE;
UINTN MemSize = 0;
UINTN UsableMemSize = 0;
UINTN UsedMem = 0;
VOID* HeapStart;
MEMORY_MAP* MapStart;
UINTN HeapSize;
UINTN MapSize;

void* kmalloc(UINTN Size){
    UINTN NumOfPage = (Size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
    if(InBS){
        return AllocatePool(Size);
    } else {
        for(UINTN i = 0; i < HeapSize/EFI_PAGE_SIZE; i++){
            if (i + NumOfPage > HeapSize / EFI_PAGE_SIZE) break;
            if(!MapStart[i].Used){
                BOOLEAN Usable = TRUE;
                for(UINTN j = i; j<i+NumOfPage;j++){
                    if(MapStart[j].Used){
                        Usable = FALSE;
                        break;
                    }
                } 
                if(!Usable){//Block here unavailable
                    continue;
                } else {
                    for(UINTN j = i; j<i+NumOfPage;j++){
                        MapStart[j].Used = TRUE;
                        MapStart[j].From = i;
                    }
                    UsedMem += NumOfPage * EFI_PAGE_SIZE;
                    return HeapStart+i*EFI_PAGE_SIZE;
                }
            }
        } 
    }
    return NULL;
    
}

void kfree(void* pointer){
    if(InBS){
        FreePool(pointer);
        return;
    }
    if ((UINT8*)pointer < (UINT8*)HeapStart ||(UINT8*)pointer >= (UINT8*)HeapStart + HeapSize)return; //Out od heap : impossible
    UINTN offset = (UINT8*)pointer - (UINT8*)HeapStart;
    if(offset % EFI_PAGE_SIZE != 0) //Not aligned
        return;
    UINTN pos = offset / EFI_PAGE_SIZE; //Start page number
    if(!MapStart[pos].Used || MapStart[pos].From != pos) //Not used  or dependant : impossible
        return;
    MapStart[pos].Used = FALSE;
    MapStart[pos].From = 0;
    UsedMem -= EFI_PAGE_SIZE;
    for(UINTN i = pos + 1; i < HeapSize / EFI_PAGE_SIZE; i++){
        if(MapStart[i].Used && MapStart[i].From == pos){
            MapStart[i].Used = FALSE;
            MapStart[i].From = 0;
            UsedMem -= EFI_PAGE_SIZE;
        } else {
            break;
        }
    }
}

EFI_STATUS krealloc(VOID** pointer,UINTN size){
    if(!pointer||!(*pointer)) return EFI_INVALID_PARAMETER; // No ptr
    if(size<=4096) return EFI_SUCCESS; //Less than what's allocated, he can take the padding
    UINTN NumOfPage = (size + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
    UINTN offset = (UINT8*)*pointer - (UINT8*)HeapStart;
    if(offset%EFI_PAGE_SIZE) return EFI_INVALID_PARAMETER; //Not aligned
    UINTN pos = offset/EFI_PAGE_SIZE;
    BOOLEAN usable = TRUE;
    for(UINTN i = 1; i < NumOfPage; i++){ //Check if we can expend
        if((!MapStart[pos+i].Used)||(MapStart[pos+i].From==pos)) continue;
        usable = FALSE; 
        break;
    }
    if(usable){// Expand
        for(UINTN i = 1; i < NumOfPage; i++){
            MapStart[pos+i].From = pos;
            
        }
        return EFI_SUCCESS;
    }   
    
    VOID* tmp = kmalloc(size);
    if(!tmp) return EFI_OUT_OF_RESOURCES;
    UINTN pageCount = 1;
    for(UINTN i = pos + 1; i < HeapSize / EFI_PAGE_SIZE; i++){
        if(MapStart[i].Used && MapStart[i].From == pos){
            pageCount++;
            UsedMem+=EFI_PAGE_SIZE;
        } else {
            break;
        }
    }
    CopyMem(tmp,*pointer,pageCount*EFI_PAGE_SIZE);
    kfree(*pointer);
    *pointer = tmp;
    return EFI_SUCCESS;
    
}