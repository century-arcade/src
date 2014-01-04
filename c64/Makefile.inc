PLATFORMVER=0.7

VICEVER = 2.4
VICEDIR = $(ARCADE)/src/c64/vice-$(VICEVER)
X64 = $(VICEDIR)/src/x64

$(X64):
	cd $(VICEDIR) ; ./configure \
	CFLAGS=-DARCADE \
	LDFLAGS=-static \
	CC=$(TOOLCHAINDIR)/usr/bin/i586-linux-gcc \
	--prefix=/usr \
	--host=i586-linux  \
	--enable-embedded  \
	--enable-sdlui  \
	--with-sdlsound  \
	--without-alsa  \
	--disable-ffmpeg  \
	--disable-realdevice  \
	--disable-ipv6  \
	--disable-bundle  \
	--disable-lame  \
	--disable-editline  \
	--disable-rs232  \
	--disable-hidmgr  \
	--disable-hidutils  \
	--without-readline  \
	--without-pulse  \
	--without-oss  \
	--without-resid  \
	--without-residfp  \
	--without-png  \
	--without-zlib 
	make CROSS=true -C $(VICEDIR) all x64sc

c64-initramfs: busybox-initramfs $(X64)
	strip $(X64)
	cp $(X64) $(INITRAMFS)/bin/
	cp $(ARCADE)/src/c64/title-init.sh $(TITLE_INIT)
	mkdir -p $(INITRAMFS)/usr/lib/vice/C64
	mkdir -p $(INITRAMFS)/usr/lib/vice/PRINTER
	cp $(VICEDIR)/data/C64/sdl_sym.vkm $(INITRAMFS)/usr/lib/vice/C64
	cp $(VICEDIR)/data/PRINTER/mps803.vpl $(INITRAMFS)/usr/lib/vice/PRINTER

c64-isoroot: base-isoroot
	cp $(PLATFORMSRC)/vice.cfg $(ISOROOT)/

c64-clean: base-clean
	make -C $(VICEDIR) clean

include $(ARCADE)/src/base/Makefile.inc
