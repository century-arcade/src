century-arcade
==============

Futureproof bootable ready-to-play ISOs of classic games

=== To setup a build machine ===

1) Start with a Debian/Ubuntu install

2) Install these packages

    apt-get install build-essential libgmp3-dev libmpfr-dev libmpc-dev texinfo libncurses5-dev unzip git-all

3) Make the toplevel $ARCADE directory [default /opt/arcade].  Make sure it has at least 10GB+ free.

4) wget -P $(ARCADE) https://raw.github.com/century-arcade/src/master/Makefile

5) make -C $(ARCADE) setup

    This will clone the src and www git repositories, setup buildroot, and
    build the toolchain.  This downloads ~200MB and uses 2.5GB of disk.

6) add $(ARCADE)/host/usr/bin/ to your PATH

=== To build the ISO for a game ===

1) Download the game source package (like [LostPig-source.zip]())
   put the .zip in $(ARCADE)/games/

2) Run ./buildiso.sh $(ARCADE)/games/LostPig-source.zip
   will create the .iso in current directory

=== Notes ===

Makefile tries to include .arcaderc very first thing for default GAMESRC and PROJECT values for development purposes (so a bare 'make' actually does something reasonable).
