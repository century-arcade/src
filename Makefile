-include .arcaderc
ARCADE ?= /opt/arcade

MKIZO = $(ARCADE)/src/tools/mkizo
VANITY_HASHER = $(ARCADE)/src/tools/vainhash/vainhash
VANITY_OPTS = -w 8 -p face

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

$(ARCADE)/src/buildroot.config: $(ARCADE)/src/Makefile

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
ABST ?= README.TXT     # ABSTRACT_FILE
APPI ?= Linux          # APPLICATION_ID
COPY ?= README.TXT     # COPYRIGHT_FILE
BIBL ?= README.TXT     # BIBLIOGRAPHIC_FILE

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

all: $(GAME)-$(VERSION).iso.zip
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
	ARCADE=$(ARCADE) INITRAMFS=$(INITRAMFS) make -C $(BUSYBOXDIR) all

$(KERNEL): $(LINUXDIR)/.config initramfs-setup
	make -C $(LINUXDIR) modules_install INSTALL_MOD_PATH=${INITRAMFS}
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
	mkdir -p $(INITRAMFS)/tmp
	mkdir -p $(INITRAMFS)/cdrom
	mkdir -p $(INITRAMFS)/save

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
		-o $*.iso \
		$(ISOROOT)/

%-$(VERSION).iso: %.iso izomaker
	$(MKIZO) $< -o $@

%-$(VERSION).iso.zip: %-$(VERSION).iso vanityhasher
	$(VANITY_HASHER) $(VANITY_OPTS) $<
	zip $@ $<
	truncate --size=+8 $@
	$(VANITY_HASHER) $(VANITY_OPTS) $@

endif # PLATFORM

%.lss: %.jpg
	$(ARCADE)/src/tools/mksplash.sh $< $@

izomaker:
	$(MAKE) -C $(ARCADE)/src/tools/mkizo

vanityhasher:
	$(MAKE) -C $(ARCADE)/src/tools/vainhash

.PHONY: clean
clean: $(PLATFORM)-clean

