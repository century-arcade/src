#!/bin/sh

# Usage: buildiso.sh LostPig.zip
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

