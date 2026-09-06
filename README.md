# MyOS

Cmpact shell made with gnu-efi

## Commmands 

It may not be updated - use "help" to get a list

cd : Change directory    
ls/dir : List directory's content    
time : Get cuurent date and time    
power : Manage power. use "off" for shutdown, and "reset" for reboot    
clear : Clear the screen    
pwd : Print working directory     
exit : Exit app    

## Makefile
make : just build the EFI file    
make run : make it run in QEMU    
make clean : remove every build file/folder    

## Dependencies

To compile it, you need
 - gcc
 - gnu-efi
 - ld
 - objcopy
 - QEMU and OVMF (if you want to make it run on them)

It may not compile on non-linux OS
