# The Century Arcade

A kit for making bootable ready-to-play ISOs of classic games

## To setup a build machine

1. Start with Ubuntu Server 12.04.3LTS.

2. Install these packages

		# apt-get update
   		# apt-get install build-essential libgmp3-dev libmpfr-dev libmpc-dev texinfo libncurses5-dev unzip git

3. Choose the toplevel $ARCADE directory [default /opt/arcade].  Make sure it has at least 10GB free.

    	$ export ARCADE=/opt/arcade
    	$ mkdir -p $ARCADE

4. Checkout the Century Arcade src repository.

    	$ git clone https://github.com/century-arcade/src.git $ARCADE/src

5. Setup buildroot and build the toolchain.  This downloads ~200MB and uses 2.5GB of disk.

    	$ ln -s $ARCADE/src/Makefile $ARCADE/Makefile
    	$ PATH="$PATH:$ARCADE/host/usr/bin"
    	$ make -C $ARCADE setup

## To build the ISO for a game

1. Download the game source package (like [LostPig-source.zip]()) and unzip into a folder like $ARCADE/games/GameFolder.src

2. make GAMESRC=$ARCADE/games/GameFolder.src

## Notes

Makefile tries to include .arcaderc very first thing for default GAMESRC and PROJECT values for development purposes (so a bare 'make' actually does something reasonable).
