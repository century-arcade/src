#!/bin/sh

/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sysfs /sys

# dynamic /dev updates
echo /sbin/hotplug > /proc/sys/kernel/hotplug
/bin/busybox mdev -s

/bin/busybox modprobe snd_ens1370
/bin/busybox modprobe snd_intel8x0

# show splash screen from initramfs while cdrom is detected
# /bin/fbshow /splash.png
/bin/tinyplay /startup.wav &

/bin/busybox modprobe piix

# while not exist /cdrom/boot,

/bin/busybox mount -t iso9660 /dev/hdc /cdrom

# endwhile

/sbin/restore_save

cd /cdrom

/bin/title-init

# show an end screen of some kind
/bin/busybox openvt -w -c 1 /bin/sh
