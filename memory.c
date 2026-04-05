#include <efi.h>
#include <efilib.h>
#include "memory.h"
#include "vars.h"

#define HEAP_SIZE 1024*1024*4 //4 MiB
#define BLOCK_SIZE 4096

BOOLEAN InBS=FALSE;

UINT8 heap[HEAP_SIZE];
MEMORY_MAP map[HEAP_SIZE/BLOCK_SIZE]; //Using page of 4KiB

void* kmalloc(UINTN Size){
    UINTN NumOfPage = (Size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if(InBS){
        return AllocatePool(Size);
    } else {
        for(UINTN i = 0; i < HEAP_SIZE/BLOCK_SIZE; i++){
            if (i + NumOfPage > HEAP_SIZE / BLOCK_SIZE) break;
            if(!map[i].Used){
                BOOLEAN Usable = TRUE;
                for(UINTN j = i; j<i+NumOfPage;j++){
                    if(map[j].Used){
                        Usable = FALSE;
                        break;
                    }
                } 
                if(!Usable){//Block here unavailable
                    continue;
                } else {
                    for(UINTN j = i; j<i+NumOfPage;j++){
                        map[j].Used = TRUE;
                        map[j].From = i;
                    }
                    return &heap[i*BLOCK_SIZE];;
                }
            }
        } 
    }
    return NULL;
    
}

void kfree(void* pointer){
    if(InBS){
        FreePool(pointer);
    } else {
        if (pointer < (void*)heap || pointer >= (void*)(heap + HEAP_SIZE)) return;
        UINTN offset = (UINT8*)pointer-(UINT8*)heap;
        UINTN pos = offset / BLOCK_SIZE;
        map[pos].Used=FALSE;
        map[pos].From=0;
        if(pos+1==HEAP_SIZE/BLOCK_SIZE)return;
        for(UINTN i = pos;i<HEAP_SIZE / BLOCK_SIZE;i++){
            if(map[i].Used && map[i].From==pos){
                map[i].Used=FALSE;
                map[i].From=0;
            } else {
                break;
            }
        }
    }
    return;
}