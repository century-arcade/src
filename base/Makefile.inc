PLATFORMVER=0.91
#
# The 'base' platform should include all relevant drivers and utilities.
#

LINUX_VER = 3.10.23
BUSYBOX_VER = 1.21.1

BASESRC=$(ARCADE)/src/base

TITLE_INIT=$(INITRAMFS)/bin/title-init
SDL_LINK= -L$(SYSROOT)/usr/lib/ -lSDL -lpthread -ldl
BUSYBOXDIR = $(BUILDDIR)/busybox-$(BUSYBOX_VER)
BUSYBOX = $(BUSYBOXDIR)/busybox

LINUXDIR = $(BUILDDIR)/linux-$(LINUX_VER)
INITRAMFS = $(LINUXDIR)/initramfs
KERNEL = $(LINUXDIR)/arch/x86/boot/bzImage

TINYALSA= $(ARCADE)/src/tools/tinyalsa
TINYPLAY= $(ARCADE)/src/tools/tinyplay

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

$(BUSYBOX): $(BUSYBOXDIR)/.config
	ARCADE=$(ARCADE) INITRAMFS=$(INITRAMFS) make -C $(BUSYBOXDIR) all

$(BUSYBOXDIR)/.config: $(BASESRC)/busybox.config $(BUSYBOXDIR)/Makefile
	cp $(BASESRC)/busybox.config $(BUSYBOXDIR)/.config

$(LINUXDIR)/.config: $(BASESRC)/linux.config $(LINUXDIR)/Makefile
	cp $(BASESRC)/linux.config $(LINUXDIR)/.config

$(KERNEL): $(LINUXDIR)/.config $(PLATFORM)-initramfs
	cp $(ARCADE)/src/assets/arcade_ascii_224.ppm $(LINUXDIR)/drivers/video/logo/logo_linux_clut224.ppm
	make -C $(LINUXDIR) modules modules_install INSTALL_MOD_PATH=${INITRAMFS}
	make -C $(LINUXDIR) all

save-busybox: $(BUSYBOX)
	cp $(BUSYBOXDIR)/.config $(BASESRC)/busybox.config

save-linux: $(KERNEL)
	cp $(LINUXDIR)/.config $(BASESRC)/linux.config

save-config: save-linux save-busybox

clean-initramfs:
	rm -rf $(INITRAMFS)
	mkdir -p $(INITRAMFS)/dev
	mkdir -p $(INITRAMFS)/proc
	mkdir -p $(INITRAMFS)/sbin
	mkdir -p $(INITRAMFS)/sys
	mkdir -p $(INITRAMFS)/bin
	mkdir -p $(INITRAMFS)/lib
	mkdir -p $(INITRAMFS)/etc
	mkdir -p $(INITRAMFS)/tmp
	mkdir -p $(INITRAMFS)/cdrom
	mkdir -p $(INITRAMFS)/save

# platforms that inherit from base might not need to add anything to initramfs
$(PLATFORM)-initramfs: clean-initramfs

# busybox.config should have the install prefix set to ${INITRAMFS}
base-initramfs: clean-initramfs $(TINYPLAY)
	cp $(ARCADE)/src/base/init.sh $(INITRAMFS)/init
	cp $(ARCADE)/src/sbin/* $(INITRAMFS)/sbin/
	cp $(TINYPLAY) $(INITRAMFS)/bin
	cp $(ARCADE)/src/assets/retro-game-jingle.wav $(INITRAMFS)/startup.wav
#	cp $(ARCADE)/src/assets/arcade.png $(INITRAMFS)/splash.png

console-initramfs: base-initramfs
	mkdir -p $(INITRAMFS)/usr/share/terminfo/l
	cp $(TOOLCHAINDIR)/usr/share/terminfo/l/linux-vt $(INITRAMFS)/usr/share/terminfo/l/linux

busybox-initramfs: console-initramfs $(BUSYBOX)
	INITRAMFS=$(INITRAMFS) make -C $(BUSYBOXDIR) install

base-isoroot: $(KERNEL) clean-isoroot
	mkdir -p $(ISOROOT)/boot/isolinux/
	cp $(BUILDROOTDIR)/output/images/isolinux.bin $(ISOROOT)/boot/
	cp $(KERNEL) $(ISOROOT)/boot/bzImage
	cp $(ARCADE)/src/base/isolinux.cfg $(ISOROOT)/boot/isolinux/

base-clean:
	# actions to take on 'make clean' for this platform


TINYPLAY_SRCS= $(addprefix $(TINYALSA)/, mixer.c pcm.c tinyplay.c)

$(TINYPLAY): $(TINYPLAY_SRCS)
	$(ARCADE_CC) -static -o $@ -I$(TINYALSA)/include $(TINYPLAY_SRCS)

#$(FBSHOW): $(ARCADE)/src/sdl/fbshow.c
#	$(ARCADE_CC) -static -o $@ $(ARCADE)/src/sdl/fbshow.c -lpng -lz


