-include .arcaderc
ARCADE ?= /opt/arcade

MKIZO = $(ARCADE)/src/tools/mkizo/mkizo
VANITY_HASHER = $(ARCADE)/src/tools/vainhash/vainhash
VANITY_OPTS = -w 8 -p dead
TINYALSA= $(ARCADE)/src/tools/tinyalsa
TINYPLAY= $(ARCADE)/src/tools/tinyplay

BUILDROOT_VER = 2013.11
UCLIBC_VER=0.9.33.2
LINUX_VER = 3.10.23
BUSYBOX_VER = 1.21.1

DOWNLOADS = $(ARCADE)/downloads
WGET := wget --directory-prefix=$(DOWNLOADS)

ARCADE_CC = $(TOOLCHAINDIR)/usr/bin/i586-linux-gcc
ARCADE_CXX = $(TOOLCHAINDIR)/usr/bin/i586-linux-g++

all:

TOOLCHAINDIR = $(ARCADE)/host
BUILDROOTDIR = $(ARCADE)/build/buildroot-$(BUILDROOT_VER)
UCLIBCDIR = $(BUILDROOTDIR)/output/build/uclibc-$(UCLIBC_VER)
SYSROOT = $(TOOLCHAINDIR)/usr/i586-buildroot-linux-uclibc/sysroot

export PATH := $(TOOLCHAINDIR)/usr/bin:$(SYSROOT)/usr/bin:$(PATH)

setup: buildroot 

setup-git: $(ARCADE)/src/Makefile $(ARCADE)/www/index.html

$(ARCADE)/src/Makefile:
	git clone https://github.com/century-arcade/src.git $(ARCADE)/src

$(ARCADE)/www/index.html:
	git clone https://github.com/century-arcade/www.git $(ARCADE)/www

$(DOWNLOADS)/buildroot-$(BUILDROOT_VER).tar.bz2:
	$(WGET) http://buildroot.uclibc.org/downloads/buildroot-$(BUILDROOT_VER).tar.bz2

$(DOWNLOADS)/uClibc-$(UCLIBC_VER).tar.bz2:
	$(WGET) http://www.uclibc.org/downloads/uClibc-$(UCLIBC_VER).tar.bz2

$(ARCADE)/src/buildroot.defconfig: $(ARCADE)/src/Makefile

BR_PATCHES = $(addsuffix .patched,$(addprefix $(BUILDROOTDIR)/,$(notdir $(basename $(wildcard $(ARCADE)/src/buildroot-patches/*.patch)))))

$(BUILDROOTDIR)/.config: $(ARCADE)/src/buildroot.defconfig $(BUILDROOTDIR)/Makefile $(BR_PATCHES)
	echo $(BR_PATCHES)
	make -C $(BUILDROOTDIR) defconfig BR2_DEFCONFIG=$(ARCADE)/src/buildroot.defconfig

save-buildroot:
	make -C $(BUILDROOTDIR) savedefconfig BR2_DEFCONFIG=$(ARCADE)/src/buildroot.defconfig

$(BUILDROOTDIR)/Makefile: $(DOWNLOADS)/buildroot-$(BUILDROOT_VER).tar.bz2
	mkdir -p $(ARCADE)/build
	tar jx -C $(ARCADE)/build -f $<


$(BUILDROOTDIR)/%.patched: $(ARCADE)/src/buildroot-patches/%.patch
	patch -d $(BUILDROOTDIR) -p1 < $<
	touch $@

$(UCLIBCDIR)/Makefile: $(DOWNLOADS)/uClibc-$(UCLIBC_VER).tar.bz2
	mkdir -p $(ARCADE)/build
	tar jx -C $(ARCADE)/build -f $<

$(UCLIBCDIR)/.config: $(ARCADE)/src/uclibc.config $(UCLIBCDIR)/Makefile
	cp $(ARCADE)/src/uclibc.config $(UCLIBCDIR)/.config

uclibc: $(UCLIBCDIR)/.config $(UCLIBCDIR)/Makefile
	make -C $(UCLIBCDIR) all install

save-uclibc:
	cp $(UCLIBCDIR)/.config $(ARCADE)/src/uclibc.config

buildroot: $(BUILDROOTDIR)/.config $(BUILDROOTDIR)/Makefile
	ARCADE=$(ARCADE) make -C $(BUILDROOTDIR) V=1
	ln -sf $(TOOLCHAINDIR)/usr/i586-buildroot-linux-uclibc/sysroot $(TOOLCHAINDIR)/sysroot

ifdef GAMESRC

include $(GAMESRC)/BUILD

ifndef PLATFORM
$(error PLATFORM must be set in $(GAMESRC)/BUILD)
endif

ifndef GAME
$(error GAME must be set in $(GAMESRC)/BUILD)
endif

PUBL ?= Century Arcade # PUBLISHER_ID
PREP ?=                  PREPARER_ID
SYSI ?= LINUX          # SYSTEM_ID
VOLI ?=                  VOLUME_ID
VOLS ?=                  VOLUMESET_ID
ABST ?= README.TXT     # ABSTRACT_FILE
APPI ?= LINUX          # APPLICATION_ID
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
VERSION = -$(PLATFORMVER)$(GAMEVER)
endif

all:

include $(ARCADE)/src/$(PLATFORM)/Makefile.inc

all: $(GAME)$(VERSION).iso.zip

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

$(KERNEL): $(LINUXDIR)/.config $(PLATFORM)-initramfs
	cp $(ARCADE)/src/assets/arcade_ascii_224.ppm $(LINUXDIR)/drivers/video/logo/logo_linux_clut224.ppm
	make -C $(LINUXDIR) modules modules_install INSTALL_MOD_PATH=${INITRAMFS}
	make -C $(LINUXDIR) all

save-busybox: $(BUSYBOX)
	cp $(BUSYBOXDIR)/.config $(PLATFORMSRC)/busybox.config

save-linux: $(KERNEL)
	cp $(LINUXDIR)/.config $(PLATFORMSRC)/linux.config

save-config: save-linux save-busybox

clean-initramfs:
	rm -rf $(INITRAMFS)
	mkdir -p $(INITRAMFS)/dev
	mkdir -p $(INITRAMFS)/proc
	mkdir -p $(INITRAMFS)/sbin
	mkdir -p $(INITRAMFS)/sys
	mkdir -p $(INITRAMFS)/bin
	mkdir -p $(INITRAMFS)/etc
	mkdir -p $(INITRAMFS)/tmp
	mkdir -p $(INITRAMFS)/cdrom
	mkdir -p $(INITRAMFS)/save

$(PLATFORM)-initramfs:

clean-isoroot:
	rm -rf $(ISOROOT)
	mkdir -p $(ISOROOT)/boot/isolinux
ifdef GAMEFILES
	cp $(addprefix $(GAMESRC)/,$(GAMEFILES)) $(ISOROOT)/
endif

%.isoroot: $(KERNEL) $(PLATFORM)-isoroot
	cp $(BUILDROOTDIR)/output/images/isolinux.bin $(ISOROOT)/boot/
	cp $(KERNEL) $(ISOROOT)/boot/bzImage

vaingold:
	truncate --size=8 $@

%$(VERSION).iso: %.isoroot $(PLATFORM) vaingold izomaker
	system_id="$(SYSI)" \
	volume_id="$(VOLI)" \
	volume_set_id="$(VOLS)" \
	preparer_id="$(PREP)" \
	publisher_id="$(PUBL)" \
	application_id="$(APPI)" \
	copyright_file_id="$(COPY)" \
	abstract_file_id="$(ABST)" \
	bibliographical_file_id="$(BIBL)" \
	creation_date="$(CREATED)" \
	modification_date="$(MODIFIED)" \
	expiration_date="$(EXPIRES)" \
	effective_date="$(EFFECTIVE)" \
		$(MKIZO) -f -c vaingold \
		-o $@ \
		-b boot/isolinux.bin \
		$(ISOROOT)/

%$(VERSION).iso.zip: %$(VERSION).iso vanityhasher
	$(VANITY_HASHER) $(VANITY_OPTS) $<
	zip $@ $<

endif # PLATFORM

izomaker:
	$(MAKE) -C $(ARCADE)/src/tools/mkizo

vanityhasher:
	$(MAKE) -C $(ARCADE)/src/tools/vainhash

TINYPLAY_SRCS= $(addprefix $(TINYALSA)/, mixer.c pcm.c tinyplay.c)

$(TINYPLAY): $(TINYPLAY_SRCS)
	$(ARCADE_CC) -static -o $@ -I$(TINYALSA)/include $(TINYPLAY_SRCS)

#$(FBSHOW): $(ARCADE)/src/sdl/fbshow.c
#	$(ARCADE_CC) -static -o $@ $(ARCADE)/src/sdl/fbshow.c -lpng -lz

.PHONY: clean

clean: $(PLATFORM)-clean

# in case the platform doesn't define it because there's nothing to do
$(PLATFORM)-clean:

