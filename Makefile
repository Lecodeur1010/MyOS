all : main.efi

SRC_C = $(wildcard *.c)
SRC_ASM = $(wildcard *.S)
SRC_PSF = $(wildcard *.psf)
OBJ = $(SRC_C:.c=.o) $(SRC_ASM:.S=.o) $(SRC_PSF:.psf=.o)
.PHONY: all run install clean

%.o: %.c
	gcc $< \
	-c \
	-fno-stack-protector \
	-fpic \
	-fshort-wchar \
	-ffreestanding \
	-fno-strict-aliasing \
	-mno-red-zone \
	-I /usr/include/efi \
	-I /usr/include/efi/x86_64 \
	-Wall \
	-Wshadow \
	-Wdouble-promotion \
	-Wformat=2 \
	-Wunreachable-code \
	-O2 \
	-o $@

%.o: %.S
	gcc -c $< -mno-red-zone -o $@

%.o: %.psf
	objcopy -I binary -O elf64-x86-64 -B i386 $< $@

main.so : $(OBJ)
	ld $(OBJ)                     \
        /usr/lib/crt0-efi-x86_64.o     \
        -nostdlib                      \
        -znocombreloc                  \
        -T /usr/lib/elf_x86_64_efi.lds \
        -shared                        \
        -Bsymbolic                     \
        -L /usr/lib              \
        -l:libefi.a	\
		-l:libgnuefi.a                    \
        -o main.so

main.efi : main.so
	objcopy -j .text                       \
        -j .sdata                      \
        -j .data                       \
        -j .rodata                     \
        -j .dynamic                    \
        -j .dynsym                     \
        -j .rel                        \
        -j .rela                       \
        -j .reloc                      \
        --output-target=efi-app-x86_64 \
        main.so                        \
        main.efi

run : main.efi
	mkdir -p FS/EFI/BOOT
	cp main.efi FS/EFI/BOOT/BOOTX64.EFI
	printf "fs0:\ncd \\EFI\\BOOT\nBOOTX64.EFI\n" > FS/startup.nsh
	dd if=/dev/zero of=main.img bs=1M count=64
	parted -s main.img mklabel gpt
	parted -s main.img mkpart primary fat32 2048s 100%
	parted -s main.img set 1 esp on
	mformat -i main.img@@1048576 -F -v "MAIN"
	mcopy -o -i main.img@@1048576 -s FS/* ::/
	qemu-system-x86_64 -enable-kvm -cpu host -m 1024\
		-drive if=pflash,format=raw,unit=0,file=/usr/share/OVMF/OVMF_CODE_4M.fd,readonly=on \
		-drive if=pflash,format=raw,unit=1,file=./OVMF_VARS.fd \
		-device virtio-rng-pci \
		-drive format=raw,file=disk1.img \
		-drive format=raw,file=disk2.img \
		-drive format=raw,file=main.img \
		-serial stdio \
		-display gtk,zoom-to-fit=on \
		-netdev user,id=net0 \
  		-device e1000,netdev=net0 \

#convert to TGA : convert img.png -define tga:compression=none -depth 8 -type truecoloralpha image.tga

install: main.efi
	sudo mkdir -p /boot/efi/EFI/Misc
	sudo cp main.efi /boot/efi/EFI/Misc/OS.efi

clean:
	rm -rf *.o *.so *.efi esp 
