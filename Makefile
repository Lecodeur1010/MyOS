main.o : main.c
	gcc main.c \
	-c \
	-fno-stack-protector \
	-fpic \
	-fshort-wchar \
	-mno-red-zone \
	-I /usr/include/efi \
	-I /usr/include/efi/x86_64 \
	-o main.o
func.o : func.c
	gcc func.c \
	-c \
	-fno-stack-protector \
	-fpic \
	-fshort-wchar \
	-mno-red-zone \
	-I /usr/include/efi \
	-I /usr/include/efi/x86_64 \
	-o func.o
cmd.o : cmd.c
	gcc cmd.c \
	-c \
	-fno-stack-protector \
	-fpic \
	-fshort-wchar \
	-mno-red-zone \
	-I /usr/include/efi \
	-I /usr/include/efi/x86_64 \
	-o cmd.o
display.o : display.c
	gcc display.c \
	-c \
	-fno-stack-protector \
	-fpic \
	-fshort-wchar \
	-mno-red-zone \
	-I /usr/include/efi \
	-I /usr/include/efi/x86_64 \
	-o display.o

main.so : main.o func.o cmd.o  display.o 
	ld main.o func.o cmd.o  display.o                     \
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

all : main.efi

run : main.efi
	mkdir -p esp/EFI/BOOT
	cp main.efi esp/EFI/BOOT/BOOTX64.EFI
	printf "fs0:\ncd \\EFI\\BOOT\nBOOTX64.EFI\n" > esp/startup.nsh
	qemu-system-x86_64 -cpu qemu64 \
        -drive if=pflash,format=raw,unit=0,file=/usr/share/OVMF/OVMF_CODE_4M.fd,readonly=on \
        -drive format=raw,file=fat:rw:esp \
		-drive format=raw,file=image.img \
        -net none \
		-display gtk,zoom-to-fit=on\

install:main.efi
	@read -p "Disque" dev; \
	sudo mount $$dev /mnt && sudo cp main.efi /mnt/EFI/Apps/main.EFI && sudo umount /mnt

clean:
	rm -rf *.o *.so *.efi esp