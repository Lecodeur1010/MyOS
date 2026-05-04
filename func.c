#include "func.h"
#include <efi.h>
#include <efilib.h>
#include <emmintrin.h>
#include "cmd.h"
#include "display.h"
#include "memory.h"
#include "vars.h"
#include "io.h"

static const char scancode_map[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0,
    '\\','z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ',
    // ...
};

EFI_INPUT_KEY WaitForInput() {
    UINT8 scancode;
    EFI_INPUT_KEY Key = {0};
    // attendre un octet disponible
    while(!(inb(0x64) & 1));
    scancode = inb(0x60);

    // ignorer break codes
    if(scancode & 0x80) return Key;

    // convertir en CHAR16
    Key.UnicodeChar = (CHAR16)scancode_map[scancode];
    return Key;
}

EFI_STATUS ExitBootServices(EFI_HANDLE ImageHandle){
    UINTN MapSize = 0;
    EFI_MEMORY_DESCRIPTOR* Map = NULL;
    UINTN MapKey = 0;
    UINTN DescriptorSize = 0,DesciptorVersion = 0;
    EFI_STATUS status;
    status = uefi_call_wrapper(BS->GetMemoryMap,5,&MapSize,Map,&MapKey,&DescriptorSize,&DesciptorVersion);
    MapSize+=2*DescriptorSize;
    Map = AllocatePool(MapSize);
    status = uefi_call_wrapper(BS->GetMemoryMap,5,&MapSize,Map,&MapKey,&DescriptorSize,&DesciptorVersion);
    if(EFI_ERROR(status)){
        Print(L"Error while fetching memory map : %r\n",status);
        Print(L"Required : %u\n",MapSize);
        return status;
    }
    status = uefi_call_wrapper(BS->ExitBootServices,2,ImageHandle,MapKey);
    if(EFI_ERROR(status)){
        Print(L"Error while exiting BS : %r",status);
        return status;
    }
    InBS = FALSE;
    UINTN BiggestBlockSize = 0;
    VOID* BiggestBlockPos;
    
    for(UINTN i = 0; i < MapSize/DescriptorSize; i++){
        EFI_MEMORY_DESCRIPTOR* desc =(EFI_MEMORY_DESCRIPTOR*)((UINT8*)Map + i * DescriptorSize);
        if(desc->Type==EfiConventionalMemory){
            MemSize+=desc->NumberOfPages*EFI_PAGE_SIZE;
            if(BiggestBlockSize<desc->NumberOfPages){
                BiggestBlockSize=desc->NumberOfPages;
                BiggestBlockPos=(VOID*)desc->PhysicalStart;
            }
        }
    }
    BiggestBlockSize*=EFI_PAGE_SIZE;
    UINTN NumOfPossibleBlock = BiggestBlockSize/(sizeof(MEMORY_MAP)+EFI_PAGE_SIZE);
    MapStart=BiggestBlockPos;
    UINTN MapSizeB=NumOfPossibleBlock*sizeof(MEMORY_MAP);
    HeapStart=MapStart+MapSizeB;
    HeapSize=NumOfPossibleBlock*EFI_PAGE_SIZE;
    UsableMemSize = BiggestBlockSize;
    kfree(Map);
    CPrint(THEME_SUCCESS,L"BS exited successfuly !\n");
    return status;
}

EFI_STATUS FormatWithUnit(UINTN val,CHAR16* buf,UINTN* bufSize){
    double lval =  val;;
    UINTN index = 0;
    CONST CHAR16* units[] = {L"B",L"KiB",L"MiB",L"GiB",L"TiB"};
    for(UINTN i = 0; i < sizeof(units)/sizeof(units[0]); i++){
        if(lval>=1024.0){
            index++;
            lval = lval/1024.0;
        }
    }
    CHAR16 num[32];
    FloatToString(num,TRUE,lval);
    UINTN pos = 0;
    for (UINTN i = 0; i < StrLen(num); i++){//Keep 4 digits after comma
        if(pos>4){
            num[i]=L'\0';
            break;
        }
        if(pos!=0||num[i]==L'.'){
            pos++;
        }
    
    }   
    *bufSize = UnicodeSPrint(buf,*bufSize,L"%s %s",num,units[index]);
    return EFI_SUCCESS;
}

VOID Char8ToChar16(CHAR8 *str, CHAR16 *buf, UINTN lenInChar)
{
    if (!str || !buf) return;
    for (UINTN i = 0; i < lenInChar; i++) {
        buf[i] = (CHAR16)(UINT8)str[i];
    }
}

#define COM1 0x3F8
void SerialInit() {
    outb(COM1 + 1, 0x00);    // disable interrupts
    outb(COM1 + 3, 0x80);    // enable DLAB
    outb(COM1 + 0, 0x03);    // baud rate divisor low (38400)
    outb(COM1 + 1, 0x00);    // baud rate divisor high
    outb(COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7);    // enable FIFO
    outb(COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

void SerialWriteChar(char c) {
    while (!(inb(COM1 + 5) & 0x20)); // wait for empty buffer
    outb(COM1, c);
}

void SerialWrite8(const CHAR8 *str){
    while (*str) {
        SerialWriteChar(*str);
        str++;
    }
}

void SerialWrite(const CHAR16 *str) {
    while (*str) {
        SerialWriteChar(*str<128?*str:'?');
        str++;
    }
}