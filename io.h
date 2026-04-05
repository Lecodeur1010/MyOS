#include <efi.h>
#include <efilib.h>
#ifdef __GNUC__
#ifndef IO
#define IO

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

static inline void outb(UINT16 port, UINT8 val){
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(UINT16 port, UINT16 val){
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl(UINT16 port, UINT32 val){
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

#endif
#endif