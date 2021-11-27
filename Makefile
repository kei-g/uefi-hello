CC=clang
CFLAGS+=-Iefi
CFLAGS+=-Oz
CFLAGS+=-Wall
CFLAGS+=-Werror
CFLAGS+=-ffreestanding
CFLAGS+=-flto
CFLAGS+=-fno-builtin
CFLAGS+=-fno-exceptions
CFLAGS+=-fno-rtti
CFLAGS+=-fno-stack-check
CFLAGS+=-fno-stack-protector
CFLAGS+=-fno-use-cxa-atexit
CFLAGS+=-fshort-wchar
CFLAGS+=-mno-red-zone
CFLAGS+=-pedantic
CFLAGS+=-std=c2x
CFLAGS+=-target x86_64-pc-mingw-w64
LD=lld-link
LDFLAGS+=/dll
LDFLAGS+=/entry:EfiMain
LDFLAGS+=/nodefaultlib
LDFLAGS+=/version:1.0
LOSETUP=sudo losetup
NAME=hello
OBJCOPY=objcopy
OBJECTS=$(SOURCES:%.c=%.o)
OBJECTS+=spinlock.o
#OFLAGS+=-j.text
#OFLAGS+=-j.rdata
#OFLAGS+=--strip-all
OFLAGS+=--target efi-app-x86_64
OVMF_FD=OVMF.fd
OVMF_ZIP=OVMF-X64-r15214.zip
QEMU=qemu-system-x86_64
QEMUFLAGS+=-M q35
QEMUFLAGS+=-bios $(OVMF_FD)
QEMUFLAGS+=-cpu host
#QEMUFLAGS+=-device intel-iommu
QEMUFLAGS+=-display gtk,grab-on-hover=on,gl=on
QEMUFLAGS+=-drive file=$(TARGET_IMG),format=raw,index=0,media=disk
QEMUFLAGS+=-enable-kvm
QEMUFLAGS+=-k jp
QEMUFLAGS+=-m 1024M
QEMUFLAGS+=-machine type=pc,accel=kvm
QEMUFLAGS+=-smp cpus=4,maxcpus=4,cores=4,threads=1
QEMUFLAGS+=-vga std
#QEMUFLAGS+=-vga virtio
SOURCES+=hello.c
SOURCES+=main.c
TARGET_DLL=$(NAME).dll
TARGET_EFI=$(NAME).efi
TARGET_IMG=$(NAME).img
TARGET_VDI=$(NAME).vdi

all: $(TARGET_EFI)

clean:
	$(RM) -r $(NAME).lib $(OBJECTS) $(TARGET_DLL) $(TARGET_EFI) $(TARGET_IMG) $(TARGET_VDI) mnt/

run: $(OVMF_FD) $(TARGET_IMG)
	$(QEMU) $(QEMUFLAGS)

.PHONY: all clean run

.SUFFIXES: .dll .efi .img .vdi

.c.o:
	$(CC) $(CFLAGS) -c $<

.dll.efi:
	$(OBJCOPY) $(OFLAGS) $^ $@

.img.vdi:
	VBoxManage convertfromraw --format VDI $^ $@

.s.o:
	$(CC) -c $< -target x86_64-pc-mingw-w64

$(OVMF_FD): $(OVMF_ZIP)
	[ -f $@ ] || unzip $^ $@

$(OVMF_ZIP):
	wget https://downloads.sourceforge.net/project/edk2/OVMF/OVMF-X64-r15214.zip

$(TARGET_DLL): $(OBJECTS)
	$(LD) $(LDFLAGS) /out:$@ $^

$(TARGET_IMG): $(TARGET_EFI) $(TARGET_IMG).xz
	[ -f $@ ] || xz -cdv < $(TARGET_IMG).xz > $@
	$(LOSETUP) -fP $(TARGET_IMG)
	$(LOSETUP) -a
	[ -d mnt ] || mkdir mnt
	sudo mount /dev/loop0p1 mnt
	sudo $(RM) mnt/EFI/BOOT/BOOTX64.EFI
	sudo dd if=$(TARGET_EFI) of=mnt/EFI/BOOT/BOOTX64.EFI
	sudo umount mnt
	$(LOSETUP) -d /dev/loop0
	$(RM) -r mnt