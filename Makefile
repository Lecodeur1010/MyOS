SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

%.o: %.c
	gcc $< \
	-c \
	-fno-stack-protector \
	-fpic \
	-fshort-wchar \
	-mno-red-zone \
	-maccumulate-outgoing-args \
	-I /usr/include/efi \
	-I /usr/include/efi/x86_64 \
	-Wall \
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
		-device ahci,id=ahci0 \
		-drive file=fat:rw:esp,if=none,id=d0,format=raw \
		-device ide-hd,bus=ahci0.0,drive=d0 \
		-drive file=44S.img,if=none,id=d1,format=raw \
		-device ide-hd,bus=ahci0.1,drive=d1 \
		-drive file=2048S.img,if=none,id=d2,format=raw \
		-device ide-hd,bus=ahci0.2,drive=d2 \
		-drive file=2048.img,if=none,id=d3,format=raw \
		-device ide-hd,bus=ahci0.3,drive=d3 \
		-drive file=2048M.img,if=none,id=d4,format=raw \
		-device ide-hd,bus=ahci0.4,drive=d4\
        -net none \
		-m 1G\
		-display gtk,zoom-to-fit=on\
		-serial stdio
install:main.efi
	sudo mount /dev/nvme0n1p1 /mnt/efi
	sudo cp main.efi /mnt/efi/EFI/Misc/OS.efi
	sudo umount /mnt/efi
clean:
	rm -rf *.o *.so *.efi esp