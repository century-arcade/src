#!/bin/sh

/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sysfs /sys
/bin/busybox mdev -s
/bin/busybox openvt -w -c 1 /bin/sh
