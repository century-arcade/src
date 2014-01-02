
PLATFORMVER=0.83
FROTZ = $(PLATFORMSRC)/frotz/frotz 

# always call the submake, let it do the dependency checking
$(FROTZ): 
	make -C $(PLATFORMSRC)/frotz frotz-curses CC="$(ARCADE)/host/usr/bin/i586-linux-gcc -DARCADE"

zmachine-initramfs: busybox-initramfs $(FROTZ)
	cp $(FROTZ) $(INITRAMFS)/bin/frotz
	echo '/bin/frotz /cdrom/*.z?' > $(TITLE_INIT)

zmachine-isoroot:
	cp $(PLATFORMSRC)/frotz.conf $(ISOROOT)/

zmachine-clean:
	make -C $(PLATFORMSRC)/frotz distclean

include $(ARCADE)/src/base/Makefile.inc
