-include .arcaderc
ARCADE ?= /opt/arcade

VANITY_HASH= --digest=sha1 --bits=32 10decade

BUILDROOT_VER = 2013.08.1
LINUX_VER = 3.10.17
BUSYBOX_VER = 1.21.1

DOWNLOADS = $(ARCADE)/downloads
WGET := wget --directory-prefix=$(DOWNLOADS)

ifndef GAMESRC

BUILDROOTDIR = $(ARCADE)/build/buildroot-$(BUILDROOT_VER)

all:

setup: buildroot 
	git clone git@github.com:century-arcade/src.git $(ARCADE)/src
	git clone git@github.com:century-arcade/www.git $(ARCADE)/www

$(DOWNLOADS)/buildroot-$(BUILDROOT_VER).tar.bz2:
	$(WGET) http://buildroot.uclibc.org/downloads/buildroot-$(BUILDROOT_VER).tar.bz2

buildroot: $(DOWNLOADS)/buildroot-$(BUILDROOT_VER).tar.bz2
	mkdir -p $(ARCADE)/build
	mkdir -p $(ARCADE)/images
	tar jx -C $(ARCADE)/build -f $<
	cp $(ARCADE)/src/buildroot.config $(BUILDROOTDIR)/.config
	ARCADE=$(ARCADE) make -C $(BUILDROOTDIR)
	cp $(BUILDROOTDIR)/isolinux.bin $(ARCADE)/images/

else

include $(GAMESRC)/BUILD

ifndef PLATFORM
$(error PLATFORM must be set in $(GAMESRC)/BUILD)
endif

ifndef GAME
$(error GAME must be set in $(GAMESRC)/BUILD)
endif

PUBL ?= Century Arcade # PUBLISHER_ID
PREP ?=                  PREPARER_ID
SYSI ?= Linux          # SYSTEM_ID
VOLI ?=                  VOLUME_ID
VOLS ?=                  VOLUMESET_ID
ABST ?= README         # ABSTRACT_FILE
APPI ?= Linux          # APPLICATION_ID
COPY ?= README         # COPYRIGHT_FILE
BIBL ?= README         # BIBLIOGRAPHIC_FILE

endif

ifdef PLATFORM
BUILDDIR = $(ARCADE)/build.$(PLATFORM)

LINUXDIR = $(BUILDDIR)/linux-$(LINUX_VER)
INITRAMFS = $(LINUXDIR)/initramfs
KERNEL = $(LINUXDIR)/arch/x86/boot/bzImage

BUSYBOXDIR = $(BUILDDIR)/busybox-$(BUSYBOX_VER)
BUSYBOX = $(BUSYBOXDIR)/busybox

PLATFORMSRC = $(ARCADE)/src/$(PLATFORM)

ifdef GAME
ISOROOT = $(BUILDDIR)/$(GAME).isoroot

all: $(GAME).iso
endif

include $(ARCADE)/src/Makefile.$(PLATFORM)

$(DOWNLOADS)/linux-$(LINUX_VER).tar.xz:
	$(WGET) https://www.kernel.org/pub/linux/kernel/v3.x/linux-$(LINUX_VER).tar.xz

$(DOWNLOADS)/busybox-$(BUSYBOX_VER).tar.bz2:
	$(WGET) http://www.busybox.net/downloads/busybox-$(BUSYBOX_VER).tar.bz2

$(BUSYBOXDIR)/Makefile: $(DOWNLOADS)/busybox-$(BUSYBOX_VER).tar.bz2
	mkdir -p $(BUILDDIR)
	tar jx -C $(BUILDDIR) -f $<
	touch $(BUSYBOXDIR)/Makefile

$(LINUXDIR)/Makefile: $(DOWNLOADS)/linux-$(LINUX_VER).tar.xz
	mkdir -p $(BUILDDIR)
	tar Jx -C $(BUILDDIR) -f $<
	touch $(LINUXDIR)/Makefile

$(BUSYBOXDIR)/.config: $(PLATFORMSRC)/busybox.config
	cp $(PLATFORMSRC)/busybox.config $(BUSYBOXDIR)/.config

$(LINUXDIR)/.config: $(PLATFORMSRC)/linux.config $(LINUXDIR)/Makefile
	cp $(PLATFORMSRC)/linux.config $(LINUXDIR)/.config

$(BUSYBOX): $(BUSYBOXDIR)/.config
	ARCADE=$(ARCADE) make -C $(BUSYBOXDIR) all

$(KERNEL): $(LINUXDIR)/.config initramfs-setup
	make -C $(LINUXDIR) all

kernel-image: $(KERNEL)
	cp $(KERNEL) $(ARCADE)/images/bzImage.$(PLATFORM)
	
busybox-image: $(BUSYBOX)
	cp $(BUSYBOX) $(ARCADE)/images/busybox.$(PLATFORM)

save-busybox: busybox-image
	cp $(BUSYBOXDIR)/.config $(PLATFORMSRC)/busybox.config

save-linux: kernel-image
	cp $(LINUXDIR)/.config $(PLATFORMSRC)/linux.config

save-config: save-linux save-busybox

initramfs: $(BUSYBOX)
	mkdir -p $(INITRAMFS)/dev
	mkdir -p $(INITRAMFS)/proc
	mkdir -p $(INITRAMFS)/sys
	mkdir -p $(INITRAMFS)/bin
	mkdir -p $(INITRAMFS)/etc
	cp $(PLATFORMSRC)/linuxrc $(INITRAMFS)/linuxrc

clean-iso:
	rm -rf $(ISOROOT)

iso-setup: clean-iso
	mkdir -p $(ISOROOT)
	cp $(addprefix $(GAMESRC)/,$(GAMEFILES)) $(ISOROOT)/

%.isoroot: kernel-image iso-setup
	mkdir -p $(ISOROOT)/boot/isolinux
	cp $(ARCADE)/images/isolinux.bin $(ISOROOT)/boot/
	cp $(ARCADE)/images/bzImage.$(PLATFORM) $(ISOROOT)/boot/bzImage
	cp $(PLATFORMSRC)/isolinux.cfg $(ISOROOT)/boot/isolinux/

%.iso: %.isoroot $(PLATFORM)
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
		-o $@.prehash \
		$(ISOROOT)/

%.iso.zip: %-prehash.iso
	$(ARCADE)/src/tools/iso2zip $< -o $*-prehash.izo
	$(ARCADE)/src/vanityhash --append --workers=8 $(VANITY_HASH) < $*-prehash.izo > $*.iso
	zip $*.iso $*.iso.zip.prehash
	$(ARCADE)/src/vanityhash --append --workers=8 $(VANITY_HASH) < $*.iso.zip.prehash > $*.iso.zip

endif

