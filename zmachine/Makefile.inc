
PLATFORMVER=0.82
FROTZ = $(PLATFORMSRC)/frotz/frotz 

zmachine: $(KERNEL)
	cp $(PLATFORMSRC)/frotz.conf $(ISOROOT)/

# always call the submake, let it do the dependency checking
frotz: 
	make -C $(PLATFORMSRC)/frotz frotz-curses CC="$(ARCADE)/host/usr/bin/i586-linux-gcc -DARCADE"

# so frotz bin gets copied
initramfs-setup: frotz-initramfs

frotz-initramfs: initramfs $(BUSYBOX) frotz
	mkdir -p $(INITRAMFS)/usr/share/terminfo/l
	cp $(TOOLCHAINDIR)/usr/share/terminfo/l/linux-vt $(INITRAMFS)/usr/share/terminfo/l/linux
	cp $(BUSYBOX) $(INITRAMFS)/bin/busybox
	cd $(INITRAMFS)/bin ; ln -sf busybox sh
	cp $(FROTZ) $(INITRAMFS)/bin/frotz
	cp $(PLATFORMSRC)/init.sh $(INITRAMFS)/init

zmachine-clean:
	make -C $(PLATFORMSRC)/frotz distclean
