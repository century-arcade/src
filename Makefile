PUBL ?= PUBLISHER_ID
PREP ?= PREPARER_ID
SYSI ?= SYSTEM_ID
VOLI ?= VOLUME_ID
VOLS ?= VOLUMESET_ID
ABST ?= ABSTRACT_FILE
APPI ?= APPLICATION_ID
COPY ?= COPYRIGHT_FILE
BIBL ?= BIBLIOGRAPHIC_FILE

ARCADE ?= /opt/arcade
DOWNLOADS := $(ARCADE)/downloads
BUILDDIR := $(ARCADE)/build

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

linux-build-setup: $(DOWNLOADS)/linux-$(LINUX_VER).tar.xz
	mkdir -p $(ARCADE)/build
	tar Jx -C $(ARCADE)/build -f $<
	cp $(ARCADE)/src/linux.config $(LINUXDIR)/.config
	
initramfs-setup: busybox
	mkdir -p $(INITRAMFS)/dev
	mkdir -p $(INITRAMFS)/proc
	mkdir -p $(INITRAMFS)/sys
	mkdir -p $(INITRAMFS)/bin
	mkdir -p $(INITRAMFS)/etc
	mkdir -p $(INITRAMFS)/usr/share/terminfo/l
	cp $(ARCADE)/src/linuxrc $(INITRAMFS)/linuxrc
	cp $(BR_TARGET)/usr/share/terminfo/l/linux $(INITRAMFS)/usr/share/terminfo/l/

busybox-build-setup: $(DOWNLOADS)/busybox-$(BUSYBOX_VER).tar.bz2
	mkdir -p $(ARCADE)/build
	tar jx -C $(ARCADE)/build -f $<
	cp $(ARCADE)/src/busybox.config $(BUSYBOXDIR)/.config

ISOROOT := $(ARCADE)/isoroot

# for constructing the output iso from the source .zip
%.iso: BUILDTMP = $(BUILDDIR)/$(@F)
%.iso: %.zip
	rm -rf $(BUILDTMP)
	mkdir -p $(BUILDTMP)
	unzip -d $(BUILDTMP) $<
	make -C $(BUILDTMP) -f $(notdir $(*F)).mk OUTPUTISO=$@ ARCADE=$(ARCADE)
	mkisofs -J -R \
		-iso-level 1 \
		-no-pad \
		-biblio    "$(BIBL)" \
		-copyright "$(COPY)" \
		-A         "$(APPI)" \
		-abstract  "$(ABST)" \
		-p         "$(PREP)" \
		-publisher "$(PUBL)" \
		-sysid     "$(SYSI)" \
		-V         "$(VOLI)" \
		-volset    "$(VOLS)" \
		-b boot/isolinux.bin \
		-c boot/boot.cat \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		-input-charset=iso8859-1 \
		-o $(BUILDTMP)/$(@F) \
		$(ISOROOT)/
	$(ARCADE)/src/tools/iso2zip $(BUILDTMP)/$(@F) -o $@
	rm -rf $(BUILDTMP)

