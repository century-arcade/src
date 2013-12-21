PLATFORMVER=0.91
#
# The 'base' platform should include all relevant drivers and utilities.
#

TITLE_INIT=$(INITRAMFS)/bin/title-init
SDL_LINK= -L$(SYSROOT)/usr/lib/ -lSDL -lpthread -ldl

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

base-isoroot: clean-isoroot
	cp $(ARCADE)/src/base/isolinux.cfg $(ISOROOT)/boot/isolinux/

base-clean:
	# actions to take on 'make clean' for this platform
