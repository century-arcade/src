#!/bin/sh

/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sysfs /sys

# dynamic /dev updates
echo /sbin/hotplug > /proc/sys/kernel/hotplug
/bin/busybox mdev -s

/bin/busybox openvt -c 1 /bin/sh

/bin/busybox modprobe piix

/bin/busybox mount -t iso9660 /dev/hdc /cdrom

/sbin/restore_save

/bin/busybox openvt -w -c 2 /bin/sh
