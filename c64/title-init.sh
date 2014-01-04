#!/bin/sh

#export SDL_DEBUG=1
export SDL_NOMOUSE=1
export SDL_AUDIODRIVER=alsa
/bin/x64 -config vice.cfg -autostart-handle-tde *.g64
