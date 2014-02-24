# The Century Arcade

A kit for making bootable ready-to-play ISOs of classic games

## To setup a build machine

1. Start with Ubuntu Server 12.04.3LTS.

2. Install these packages

		# apt-get update
   		# apt-get install git build-essential zip unzip libgmp3-dev libmpfr-dev libmpc-dev texinfo libncurses5-dev

3. Choose the toplevel $ARCADE directory [default /opt/arcade].  Make sure it has at least 10GB free.

    	$ export ARCADE=/opt/arcade
    	$ mkdir -p $ARCADE

4. Checkout the Century Arcade src repository.

    	$ git clone https://github.com/century-arcade/src.git $ARCADE/src

5. Build the toolchain.  This downloads ~200MB and uses 2.5GB of disk under the `$ARCADE` directory.

    	$ make -C $ARCADE/src toolchain

## To build the ISO for a game

1. Download the game source package (like [LostPig-source.zip]()) and unzip into a folder (like `$ARCADE/games/LostPig-source`).

2. `make GAMESRC=$ARCADE/games/LostPig-source`

3. The `LostPig-zmachine.iso` will be in the current directory.

## To play an ISO with qemu

0. [For a release: unzip the .izo.zip to get the .iso.

1. `QEMU_AUDIO_DRV=alsa qemu-system-i386 -soundhw all -cdrom LostPig-zmachine.iso`

## Notes

Makefile tries to include .arcaderc (from the current directory) very first thing for default GAMESRC and PROJECT values for development purposes (so a bare 'make' actually does something reasonable).
