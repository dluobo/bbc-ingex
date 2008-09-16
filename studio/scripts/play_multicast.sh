#! /bin/bash

# stop current player if it's running
if killall -q -0 -e player
then
	killall -e player
	while killall -q -0 -e player
	do
		usleep 100000
	done
fi

# start new player
# cam 1 - 4
player --pixel-aspect 1:1 --quad-split --prescaled-split --hide-progress-bar --source-aspect 16:9 --udp-in 239.255.1.1:2000 --udp-in 239.255.1.1:2001 --udp-in 239.255.1.1:2002 --udp-in 239.255.1.1:2003

# all cams
#player --audio-mon 2 --nona-split --prescaled-split --hide-progress-bar --source-aspect 16:9 --udp-in 239.255.1.1:2000 --udp-in 239.255.1.1:2001 --udp-in 239.255.1.1:2002 --udp-in 239.255.1.1:2003 --udp-in 239.255.1.1:2004 --udp-in 239.255.1.1:2006 --udp-in 239.255.1.1:2007

