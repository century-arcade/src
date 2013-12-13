#!/bin/sh

# Usage: buildiso.sh $ARCADE/games/LostPig-source.zip
#
# extracts the source materials into LostPig-source.src
#       and creates the versioned .iso in current directory

GAMESRC=$1

ARCADE=/opt/arcade
ISODIR=${ARCADE}/games   # `dirname ${GAMESRC}`

GAME=`basename ${GAMESRC} -source.zip`
SRCDIR=${ARCADE}/build/games/${GAME}.src

GAMEISO=${ISODIR}/${GAME}.iso

echo Creating ${GAMEISO} from ${GAMESRC}

rm -rf ${SRCDIR} && \
mkdir -p ${SRCDIR} && \
unzip -d ${SRCDIR} ${GAMESRC} && \
make -f ${ARCADE}/Makefile GAMESRC=${SRCDIR}

