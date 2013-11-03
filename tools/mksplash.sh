#!/bin/sh

convert +dither -colors 16 $1 ppm:- | ppmtolss16 > $2
