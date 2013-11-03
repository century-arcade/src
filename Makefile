-include .arcaderc
ARCADE ?= /opt/arcade

ISO2ZIP = $(ARCADE)/src/tools/iso2zip
VANITY_HASH = $(ARCADE)/src/tools/vanityhash-1.1/vanityhash --append \
				--workers=8 --digest=sha1 --bits=48 a4cade
# --bits=48 10decade

BUILDROOT_VER = 2013.08.1
UCLIBC_VER=0.9.33.2
LINUX_VER = 3.10.17
BUSYBOX_VER = 1.21.1

DOWNLOADS = $(ARCADE)/downloads
WGET := wget --directory-prefix=$(DOWNLOADS)

ifndef GAMESRC

BUILDROOTDIR = $(ARCADE)/build/buildroot-$(BUILDROOT_VER)

all:

setup: buildroot 

setup-git: $(ARCADE)/src/Makefile $(ARCADE)/www/index.html

$(ARCADE)/src/Makefile:
	git clone https://github.com/century-arcade/src.git $(ARCADE)/src

$(ARCADE)/www/index.html:
	git clone https://github.com/century-arcade/www.git $(ARCADE)/www

$(DOWNLOADS)/buildroot-$(BUILDROOT_VER).tar.bz2:
	$(WGET) http://buildroot.uclibc.org/downloads/buildroot-$(BUILDROOT_VER).tar.bz2

$(DOWNLOADS)/uclibc-$(UCLIBC_VER).tar.bz2:
	$(WGET) http://www.uclibc.org/downloads/uClibc-$(UCLIBC_VER).tar.xz

$(ARCADE)/src/buildroot.config: setup-git 

$(BUILDROOTDIR)/.config: $(ARCADE)/src/buildroot.config $(BUILDROOTDIR)/Makefile
	cp $(ARCADE)/src/buildroot.config $(BUILDROOTDIR)/.config

$(BUILDROOTDIR)/Makefile: $(DOWNLOADS)/buildroot-$(BUILDROOT_VER).tar.bz2
	mkdir -p $(ARCADE)/build
	tar jx -C $(ARCADE)/build -f $<

$(UCLIBCDIR)/Makefile: $(DOWNLOADS)/uclibc-$(UCLIBC_VER).tar.bz2
	mkdir -p $(ARCADE)/build
	tar jx -C $(ARCADE)/build -f $<

$(UCLIBCDIR)/.config: $(PLATFORMSRC)/uclibc.config $(UCLIBCDIR)/Makefile
	cp $(PLATFORMSRC)/uclibc.config $(UCLIBCDIR)/.config

uclibc: $(UCLIBCDIR)/.config $(UCLIBCDIR)/Makefile
	make -C uclibc all install

save-uclibc:
	cp $(UCLIBCDIR)/.config $(ARCADE)/src/uclibc.config

buildroot: $(BUILDROOTDIR)/.config $(BUILDROOTDIR)/Makefile
	mkdir -p $(ARCADE)/images
	ARCADE=$(ARCADE) make -C $(BUILDROOTDIR)
	cp $(BUILDROOTDIR)/output/images/isolinux.bin $(ARCADE)/images/

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

all: $(GAME)-beta.iso  # $(GAME).iso.zip
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

$(BUSYBOXDIR)/.config: $(PLATFORMSRC)/busybox.config $(BUSYBOXDIR)/Makefile
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
	mkdir -p $(INITRAMFS)/cdrom

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
	cp $(GAMESRC)/splash.lss $(ISOROOT)/boot/isolinux/
	/bin/echo -ne "\x18splash.lss\x0a\x1a" > $(ISOROOT)/boot/isolinux/display.msg

%-beta.iso: %.isoroot $(PLATFORM)
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
		-o $@ \
		$(ISOROOT)/
	truncate --size=%1M $@     # virtualbox needs size to multiple of 1MB

endif

%.iso.zip: %-beta.iso $(ISO2ZIP)
	$(ISO2ZIP) $< -o $*-prehash.izo
	$(VANITY_HASH) < $*-prehash.izo > $*.iso
	zip $*.iso.zip.prehash $*.iso
	$(VANITY_HASH) < $*.iso.zip.prehash > $*.iso.zip

%.lss: %.ppm
	ppmtolss16 < $< > $@

# use system gcc
$(ISO2ZIP): $(ARCADE)/src/tools/iso2zip.c
	gcc -o $@ $<

.PHONY: clean
clean: $(PLATFORM)-clean
	make -C $(ARCADE)/src/zmachine distclean
