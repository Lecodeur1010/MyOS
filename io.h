#include <efi.h>
#include <efilib.h>
#ifdef __GNUC__
#ifndef IO_H
#define IO_H

static inline UINT8 inb(UINT16 port){
    UINT8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline UINT16 inw(UINT16 port){
    UINT16 ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline UINT32 inl(UINT16 port){
    UINT32 ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline VOID outb(UINT16 port, UINT8 val){
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline VOID outw(UINT16 port, UINT16 val){
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline VOID outl(UINT16 port, UINT32 val){
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline VOID insw(UINT16 port, VOID* addr, UINT32 count) {
    asm volatile ("rep insw"
                  : "+D"(addr), "+c"(count)
                  : "d"(port)
                  : "memory");
}

static inline VOID outsw(UINT16 port, VOID* addr, UINT32 count) {
    asm volatile ("rep outsw"
                  : "+S"(addr), "+c"(count)
                  : "d"(port));
}

#endif
#endif