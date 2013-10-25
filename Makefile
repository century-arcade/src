ARCADE ?= /opt/arcade
DOWNLOADS := $(ARCADE)/downloads
WGET := wget --directory-prefix=$(DOWNLOADS)

BUILDROOT_VER = 2013.08.1
LINUX_VER = 3.10.17
BUSYBOX_VER = 1.21.1

BUILDROOTDIR := $(ARCADE)/build/buildroot-$(BUILDROOT_VER)
LINUXDIR := $(ARCADE)/build/linux-$(LINUX_VER)
BUSYBOXDIR := $(ARCADE)/build/busybox-$(BUSYBOX_VER)

all:

setup: buildroot 

$(DOWNLOADS)/buildroot-$(BUILDROOT_VER).tar.bz2:
	$(WGET) http://buildroot.uclibc.org/downloads/buildroot-$(BUILDROOT_VER).tar.bz2

$(DOWNLOADS)/linux-$(LINUX_VER).tar.xz:
	$(WGET) https://www.kernel.org/pub/linux/kernel/v3.x/linux-$(LINUX_VER).tar.xz

$(DOWNLOADS)/busybox-$(BUSYBOX_VER).tar.bz2:
	$(WGET) http://www.busybox.net/downloads/busybox-$(BUSYBOX_VER).tar.bz2

buildroot: $(DOWNLOADS)/buildroot-$(BUILDROOT_VER).tar.bz2
	mkdir -p $(ARCADE)/build
	mkdir -p $(ARCADE)/images
	tar jx -C $(ARCADE)/build -f $<
	cp $(ARCADE)/src/buildroot.config $(BUILDROOTDIR)/.config
	ARCADE=$(ARCADE) make -C $(BUILDROOTDIR)
	cp $(BUILDROOTDIR)/isolinux.bin $(ARCADE)/images/
	# make legal-info

linux: $(ARCADE)/images/bzImage
busybox: $(ARCADE)/images/busybox

$(ARCADE)/images/bzImage: $(DOWNLOADS)/linux-$(LINUX_VER).tar.xz
	mkdir -p $(ARCADE)/build
	tar Jx -C $(ARCADE)/build -f $<
	cp $(ARCADE)/src/linux.config $(LINUXDIR)/.config
	ARCADE=$(ARCADE) make -C $(LINUXDIR) all
	cp $(LINUXDIR)/arch/x86/boot/bzImage $(ARCADE)/images/
	
$(ARCADE)/images/busybox: $(DOWNLOADS)/busybox-$(BUSYBOX_VER).tar.bz2
	mkdir -p $(ARCADE)/build
	tar jx -C $(ARCADE)/build -f $<
	cp $(ARCADE)/src/busybox.config $(BUSYBOXDIR)/.config
	ARCADE=$(ARCADE) make -C $(BUSYBOXDIR) all
	cp $(BUSYBOXDIR)/busybox $(ARCADE)/images/

cdroot: linux busybox
	make -C $(FROTZOS)/frotz frotz-curses
	cp $(FROTZOS)/frotz/frotz $(ROOTFS)/bin/
	cp $(FROTZOS)/frotz.conf $(ROOTFS)/etc
	cp games/zork1.z5 $(ROOTFS)/
	mkdir -p $(ROOTFS)/boot/isolinux
	cp $(FROTZOS)/isolinux.cfg $(ROOTFS)/boot/isolinux/
	cp /opt/arcade/images/isolinux.bin $(ROOTFS)/boot/
	cp /opt/arcade/images/bzImage $(ROOTFS)/boot/

ROOTFS := $(ARCADE)/isoroot

$(ARCADE)/Zork1.iso: # cdroot
	mkisofs -J -R -o $@ \
		-b boot/isolinux.bin \
		-c boot/boot.cat \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		$(ROOTFS)/


