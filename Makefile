-include .arcaderc
ARCADE ?= /opt/arcade

MKIZO = $(ARCADE)/src/tools/mkizo/mkizo
VANITY_HASHER = $(ARCADE)/src/tools/vainhash/vainhash
VANITY_OPTS = -w 8 -p cade

BUILDROOT_VER = 2013.11
UCLIBC_VER = 0.9.33.2

DOWNLOADS = $(ARCADE)/downloads
WGET := wget --directory-prefix=$(DOWNLOADS)

ARCADE_CC = $(TOOLCHAINDIR)/usr/bin/i586-linux-gcc
ARCADE_CXX = $(TOOLCHAINDIR)/usr/bin/i586-linux-g++

all: # so first rule isn't accidentally overridden

TOOLCHAINDIR = $(ARCADE)/host

# sysroot has target's includes, static libs, shared libs, possibly binaries.
# generally, configure --prefix=$(SYSROOT)/usr [--bindir=$(TOOLCHAIN)/usr/bin]
#
# symlink makes it prettier
SYSROOT = $(TOOLCHAINDIR)/sysroot
#SYSROOT = $(TOOLCHAINDIR)/usr/i586-buildroot-linux-uclibc/sysroot

export PATH := $(TOOLCHAINDIR)/usr/bin:$(SYSROOT)/usr/bin:$(PATH)

setup: buildroot 

setup-git: $(ARCADE)/src/Makefile $(ARCADE)/www/index.html

$(ARCADE)/src/Makefile:
	git clone https://github.com/century-arcade/src.git $(ARCADE)/src

$(ARCADE)/www/index.md:
	git clone https://github.com/century-arcade/www.git $(ARCADE)/www

$(DOWNLOADS)/buildroot-$(BUILDROOT_VER).tar.bz2:
	$(WGET) http://buildroot.uclibc.org/downloads/buildroot-$(BUILDROOT_VER).tar.bz2

$(DOWNLOADS)/uClibc-$(UCLIBC_VER).tar.bz2:
	$(WGET) http://www.uclibc.org/downloads/uClibc-$(UCLIBC_VER).tar.bz2

### buildroot download, patching, configuring, building

BUILDROOTDIR = $(ARCADE)/build/buildroot-$(BUILDROOT_VER)

BR_PATCHES = $(addsuffix .patched,$(addprefix $(BUILDROOTDIR)/,$(notdir $(basename $(wildcard $(ARCADE)/src/buildroot-patches/*.patch)))))

$(BUILDROOTDIR)/Makefile: $(DOWNLOADS)/buildroot-$(BUILDROOT_VER).tar.bz2
	mkdir -p $(ARCADE)/build
	tar jx -C $(ARCADE)/build -f $<

$(BUILDROOTDIR)/%.patched: $(ARCADE)/src/buildroot-patches/%.patch
	patch -d $(BUILDROOTDIR) -p1 < $<
	touch $@

$(BUILDROOTDIR)/.config: $(ARCADE)/src/buildroot.defconfig $(BUILDROOTDIR)/Makefile $(BR_PATCHES)
	make -C $(BUILDROOTDIR) defconfig BR2_DEFCONFIG=$(ARCADE)/src/buildroot.defconfig

buildroot: $(BUILDROOTDIR)/.config $(BUILDROOTDIR)/Makefile
	ARCADE=$(ARCADE) make -C $(BUILDROOTDIR) V=1
	ln -sf $(TOOLCHAINDIR)/usr/i586-buildroot-linux-uclibc/sysroot $(TOOLCHAINDIR)/sysroot

save-buildroot:
	make -C $(BUILDROOTDIR) savedefconfig BR2_DEFCONFIG=$(ARCADE)/src/buildroot.defconfig


### uclibc configuration and build

UCLIBCDIR = $(BUILDROOTDIR)/output/build/uclibc-$(UCLIBC_VER)

$(UCLIBCDIR)/Makefile: $(DOWNLOADS)/uClibc-$(UCLIBC_VER).tar.bz2
	mkdir -p $(ARCADE)/build
	tar jx -C $(ARCADE)/build -f $<

$(UCLIBCDIR)/.config: $(ARCADE)/src/uclibc.config $(UCLIBCDIR)/Makefile
	cp $(ARCADE)/src/uclibc.config $(UCLIBCDIR)/.config # XXX defconfig

uclibc: $(UCLIBCDIR)/.config $(UCLIBCDIR)/Makefile
	make -C $(UCLIBCDIR) all install

save-uclibc:
	cp $(UCLIBCDIR)/.config $(ARCADE)/src/uclibc.config

### game IZO construction

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

PLATFORMSRC = $(ARCADE)/src/$(PLATFORM)

ifdef GAME
ISOROOT = $(BUILDDIR)/$(GAME).isoroot
VERSION = -$(PLATFORMVER)$(GAMEVER)
endif

include $(ARCADE)/src/$(PLATFORM)/Makefile.inc

all: $(GAME)-$(PLATFORM)$(VERSION).izo.zip

clean-isoroot:
	rm -rf $(ISOROOT)
	mkdir -p $(ISOROOT)
ifdef GAMEFILES
	cp $(addprefix $(GAMESRC)/,$(GAMEFILES)) $(ISOROOT)/
endif
ifdef SPOILERS
	mkdir $(ISOROOT)/spoilers
	cp $(addprefix $(GAMESRC)/,$(SPOILERS)) $(ISOROOT)/spoilers
endif

$(PLATFORM)-isoroot: base-isoroot

%$(VERSION).iso: $(PLATFORM)-isoroot vaingold $(MKIZO)
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

%$(VERSION).izo.zip: %$(VERSION).iso vanityhasher
	$(VANITY_HASHER) $(VANITY_OPTS) $<
	zip $@ $<

endif # PLATFORM

# XXX: need to make mkizo for host and for target
$(MKIZO):
	$(MAKE) -C $(ARCADE)/src/tools/mkizo
#	CC=$(ARCADE_CC) $(MAKE) -C $(ARCADE)/src/tools/mkizo
#	cp $(ARCADE)/src/tools/mkizo $(TOOLCHAIN)/usr/bin$(MKIZO)

vanityhasher:
	$(MAKE) -C $(ARCADE)/src/tools/vainhash

.PHONY: clean

clean: $(PLATFORM)-clean

# in case the platform doesn't define it because there's nothing to do
$(PLATFORM)-clean:

