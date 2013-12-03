PLATFORMVER=0.81
#
# The 'base' platform should include all relevant drivers and utilities.
#
# Other platforms copy this base platform and remove what they don't need.

base: $(KERNEL)
#	cp $(PLATFORMSRC)/frotz.conf $(ISOROOT)/

# busybox.config should have the install prefix set to ${INITRAMFS}
initramfs-setup: base-initramfs
	INITRAMFS=$(INITRAMFS) make -C $(BUSYBOXDIR) install

base-initramfs: initramfs $(BUSYBOX)
	mkdir -p $(INITRAMFS)/usr/share/terminfo/l
	cp $(BR_TARGET)/usr/share/terminfo/l/linux-vt $(INITRAMFS)/usr/share/terminfo/l/linux
	cp $(BUSYBOX) $(INITRAMFS)/bin/busybox
	cd $(INITRAMFS)/bin ; ln -sf busybox sh
	cp $(PLATFORMSRC)/init.sh $(INITRAMFS)/init
	cp $(ARCADE)/src/sbin/* $(INITRAMFS)/sbin/

base-clean:
	# actions to take on 'make clean' for this platform
