#!/bin/bash
# quad-split viewer

# Check whether process is running and kill if it is
if killall -q -0 -e player
then
	killall -e player
	while killall -q -0 -e player
	do
		usleep 100000
	done
fi

# -g option runs program under gdb so that a crash displays a stack trace
PRERUN=""
if [ "$1" = "-g" ] ; then
	PRERUN="gdb -q -ex run -ex where --args"
	shift			# swallow option
fi

#
# --x11 is necessary when graphics card / driver cannot suport Xv extension
# audio device 1 was found to work on evan
# assumption is that the audio is in shared mem 0

# normal quad view
$PRERUN /usr/local/bin/player --disable-shuttle --audio-lineup -18 --audio-mon 2 --source-aspect 16:9 --quad-split --hide-progress-bar --shm-in 0p --shm-in 1p --shm-in 2p --shm-in 3p

# SD quad view 2 channels from secondary buffer
#$PRERUN /usr/local/bin/player --disable-shuttle --audio-lineup -18 --audio-mon 2 --source-aspect 16:9 --quad-split --hide-progress-bar --shm-in 0s --shm-in 1s

# SD quad view 4 channels from secondary buffer
#$PRERUN /usr/local/bin/player --disable-shuttle --audio-lineup -18 --audio-mon 2 --source-aspect 16:9 --quad-split --hide-progress-bar --shm-in 0s --shm-in 1s --shm-in 2s --shm-in 3s

# quad view with director's cut logging
#$PRERUN /usr/local/bin/player --disable-shuttle --audio-lineup -18 --audio-mon 2 --source-aspect 16:9 --quad-split --hide-progress-bar --split-select --vswitch-tc LTC.0 --vswitch-db /video/aaf/directors_cut.db --shm-in 0p --shm-in 1p --shm-in 2p --shm-in 3p
