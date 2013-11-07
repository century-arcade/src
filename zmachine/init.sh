#!/bin/sh

/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t devtmpfs devtmpfs /dev
/bin/busybox mount /dev/hdc /cdrom
/bin/busybox openvt -w -c 1 /bin/frotz
