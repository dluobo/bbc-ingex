#! /bin/bash

# Ingex installation directory
INGEX_DIR="/home/ingex/ap-workspace/ingex/studio"

# get current directory
DIR=`pwd`
# Ingex SD startup
SDconfig="$DIR/dvs_sd.conf"
SDmodeName="SD"
HDconfig="$DIR/dvs_hd.conf"
HDmodeName="HD"

# set defaults
mode="SD"
dvsConfig=${SDconfig}
channels=4

usage="Usage: $0 [-s|-h]
Options:
  -s  Start in SD mode (default)
  -h  Start in HD mode
"

while getopts ":hs:" opt; do
  case $opt in
    s  ) mode=${SDmodeName}
         dvsConfig=${SDconfig}
         channels=4 ;;
    h  ) mode=${HDmodeName}
         dvsConfig=${HDconfig}
         channels=2 ;;
    \? ) echo "${usage}"
         exit 1;;
    esac
done

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


# change dvs config file to the appropriate version
if [ -e "$dvsConfig" ] ; then
  sudo ln -f -s "${dvsConfig}" /etc/dvs.conf
  sudo /etc/init.d/dvs_setup restart
fi

# REALLY NEED TO PASS the number of channels into the capture.sh setup <<<<<<<--
# .. but for now have different calls for SD and HD
if [ $mode = "SD" ] ; then
  echo "Starting capture in SD mode"
  xterm -T "Capture-SD" -geometry 132x25 -e "cd ${INGEX_DIR}/capture && ./capture.sh" &
# xterm -T Capture-SD -geometry 132x25 -e "sudo nice --adjustment=-10 /home/ingex/ap-workspace/ingex/studio/capture/dvs_sdi -c 4 -mt LTC -mc 0 -rt LTC -f YUV422 -s None" &
else
  echo "Starting capture in HD mode"
  xterm -T Capture-HD -geometry 132x25 -e "sudo nice --adjustment=-10 ${INGEX_DIR}/capture/dvs_sdi -c 2 -mt LTC -mc 0 -rt LTC" & 
fi

usleep 100000

# start transfer server, if not already running
if killall -q -0 -e xferserver.pl
then
  echo "Transfer service already running."
else
  echo "Starting transfer service"
  xterm -T Transfer_Server -geometry 100x25 -e "cd ${INGEX_DIR}/processing/media_transfer && ./xferserver.pl" &
usleep 100000
fi

# start recorder
echo "Starting Recorder"
xterm -T Recorder-SD -geometry 100x25 -e "cd ${INGEX_DIR}/ace-tao/Recorder && ./run_ingex1.sh" &

# start the quad-split player - but only in SD mode
if [ $mode = "SD" ] ; then
  echo "Starting quad split"
  ${INGEX_DIR}/../player/scripts/quad-split.sh &
fi

exit
