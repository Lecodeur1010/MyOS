SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

%.o: %.c
	gcc $< \
	-c \
	-fno-stack-protector \
	-fpic \
	-fshort-wchar \
	-mno-red-zone \
	-I /usr/include/efi \
	-I /usr/include/efi/x86_64 \
	-o $@

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

all : main.efi

run : main.efi
	mkdir -p esp/EFI/BOOT
	cp main.efi esp/EFI/BOOT/BOOTX64.EFI
	printf "fs0:\ncd \\EFI\\BOOT\nBOOTX64.EFI\n" > esp/startup.nsh
	qemu-system-x86_64 -cpu qemu64 \
        -drive if=pflash,format=raw,unit=0,file=/usr/share/OVMF/OVMF_CODE_4M.fd,readonly=on \
		-drive if=pflash,format=raw,unit=1,file=OVMF_VARS.fd \
        -drive format=raw,if=virtio,file=fat:rw:esp \
		-drive format=raw,if=virtio,file=image.img \
        -net none \
		-display gtk,zoom-to-fit=on

install:main.efi
	sudo mount /dev/nvme0n1p1 /mnt/efi
	sudo cp main.efi /mnt/efi/EFI/Misc/OS.efi
	sudo umount /mnt/efi
clean:
	rm -rf *.o *.so *.efi esp