#! /bin/bash

# Ingex SD startup

# first kill quad split, recorder and capture processes, if they're running
# Check whether process is running and kill if it is
if killall -q -0 -e player
then
	killall -e player
	while killall -q -0 -e player
	do
		usleep 100000
	done
fi

if killall -q -0 -e Recorder
then
	killall -s SIGINT -e Recorder
	while killall -q -0 -e Recorder
	do
		usleep 100000
	done
fi

usleep 1000000

# killall dvs_sdi needs to be run with sudo because dvs_sdi was started with a sudo nice.. command (see below)
if sudo killall -q -0 -e dvs_sdi
then
	sudo killall -s SIGINT -e dvs_sdi
	while sudo killall -q -0 -e dvs_sdi
	do
		usleep 100000
	done
fi


# change dvs config file to the sd version
sudo ln -f -s /etc/dvs_sd.conf /etc/dvs.conf
sudo /etc/init.d/dvs_setup restart

xterm -T Capture-SD -geometry 132x25 -e "sudo nice --adjustment=-10 /home/john/ap-workspace/ingex/studio/capture/dvs_sdi -c 4 -mt LTC -mc 0 -rt LTC -f YUV422 -s None" &

usleep 100000

xterm -T Recorder-SD -geometry 132x25 -e "/home/john/ap-workspace/ingex/studio/ace-tao/Recorder/Recorder Ingex bamzooki bamzooki -ORBDefaultInitRef corbaloc:iiop::8888 -ORBDottedDecimalAddresses 1" &


# start the quad-split player - X11 output only
#/home/john/ap-workspace/ingex/player/scripts/quad-split.sh

exit
