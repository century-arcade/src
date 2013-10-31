-include .arcaderc
ARCADE ?= /opt/arcade

DOWNLOADS = $(ARCADE)/downloads
WGET := wget --directory-prefix=$(DOWNLOADS)
VANITY_HASH= --digest=sha1 --bits=32 10decade

ifndef GAMESRC

BUILDROOT_VER = 2013.08.1
LINUX_VER = 3.10.17
BUSYBOX_VER = 1.21.1

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

ifndef PROJECT
	$(error PROJECT must be set in $(GAMESRC)/BUILD)
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

ifdef PROJECT
BUILDDIR = $(ARCADE)/build.$(PROJECT)
INITRAMFS = $(BUILDDIR)/initramfs
BUSYBOXDIR = $(BUILDDIR)/busybox-$(BUSYBOX_VER)
LINUXDIR = $(BUILDDIR)/linux-$(LINUX_VER)

BUSYBOX = $(BUSYBOXDIR)/busybox
KERNEL = $(LINUXDIR)/arch/x86/boot/bzImage

PROJECTSRC = $(ARCADE)/src/$(PROJECT)

ifdef GAME
ISOROOT = $(BUILDDIR)/$(GAME).isoroot

all: $(GAME).iso
endif

include $(ARCADE)/src/Makefile.$(PROJECT)

$(DOWNLOADS)/linux-$(LINUX_VER).tar.xz:
	$(WGET) https://www.kernel.org/pub/linux/kernel/v3.x/linux-$(LINUX_VER).tar.xz

$(DOWNLOADS)/busybox-$(BUSYBOX_VER).tar.bz2:
	$(WGET) http://www.busybox.net/downloads/busybox-$(BUSYBOX_VER).tar.bz2

$(BUSYBOXDIR)/.config: $(DOWNLOADS)/busybox-$(BUSYBOX_VER).tar.bz2
	mkdir -p $(BUILDDIR)
	tar jx -C $(BUILDDIR) -f $<
	cp $(PROJECTSRC)/busybox.config $(BUSYBOXDIR)/.config

$(LINUXDIR)/.config: $(DOWNLOADS)/linux-$(LINUX_VER).tar.xz
	mkdir -p $(BUILDDIR)
	tar Jx -C $(BUILDDIR) -f $<
	cp $(PROJECTSRC)/linux.config $(LINUXDIR)/.config

$(BUSYBOX): $(BUSYBOXDIR)/.config
	ARCADE=$(ARCADE) make -C $(BUSYBOXDIR) all

$(KERNEL): $(LINUXDIR)/.config initramfs-setup
	make -C $(LINUXDIR) all

kernel-image: $(KERNEL)
	cp $(KERNEL) $(ARCADE)/images/bzImage.$(PROJECT)
	
busybox-image: $(BUSYBOX)
	cp $(BUSYBOX) $(ARCADE)/images/busybox.$(PROJECT)

save-busybox: busybox-image
	cp $(BUSYBOXDIR)/.config $(PROJECTSRC)/busybox.config

save-linux: kernel-image
	cp $(LINUXDIR)/.config $(PROJECTSRC)/linux.config

save-config: save-linux save-busybox

initramfs: $(BUSYBOX)
	mkdir -p $(INITRAMFS)/dev
	mkdir -p $(INITRAMFS)/proc
	mkdir -p $(INITRAMFS)/sys
	mkdir -p $(INITRAMFS)/bin
	mkdir -p $(INITRAMFS)/etc
	cp $(PROJECTSRC)/linuxrc $(INITRAMFS)/linuxrc

clean-iso:
	rm -rf $(ISOROOT)

iso-setup: clean-iso
	mkdir -p $(ISOROOT)
	cp $(addprefix $(GAMESRC)/,$(GAMEFILES)) $(ISOROOT)/

%.isoroot: kernel-image iso-setup
	mkdir -p $(ISOROOT)/boot/isolinux
	cp $(ARCADE)/images/isolinux.bin $(ISOROOT)/boot/
	cp $(ARCADE)/images/bzImage.$(PROJECT) $(ISOROOT)/boot/bzImage
	cp $(PROJECTSRC)/isolinux.cfg $(ISOROOT)/boot/isolinux/

%.iso: %.isoroot $(PROJECT)
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

