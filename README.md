century-arcade
==============

Futureproof bootable ready-to-play ISOs of classic games

=== To setup a build machine ===

* packages to install on Debian/Ubuntu:
    apt-get install build-essential libgmp3-dev libmpfr-dev libmpc-dev texinfo libncurses5-dev unzip git-all

* cd $(ARCADE) ; git clone git@github.com:century-arcade/src.git
    Note: 'make setup' will download and run buildroot, downloading ~200MB and using 2.5GB of disk for building the toolchain

    ARCADE=$(ARCADE) make -C src setup

* download and build the linux kernel
    /opt/arcade$ make -C src kernel

* download and build BusyBox
    /opt/arcade$ make -C src busybox



