#!/bin/sh

# Defaults for SD operation
MAX_CHANNELS=4
MODE="PAL"
PRIMARY_BUFFER="YUV422"
SECONDARY_BUFFER="None"

usage="Usage: $0 [-h]
Options:
  -h  Start in HD mode
"

while getopts 'h' OPT
do
  case $OPT in
  h)   MAX_CHANNELS=2
       MODE="1920x1080i50"
       SECONDARY_BUFFER="YUV422"
       ;;
  ?)   echo "${usage}"
       exit 1
       ;;
  esac
done

#sudo nice --10 ./dvs_sdi -c 4 -mc 0 -mt LTC -rt LTC -f YUV422 -s None
sudo nice --10 ./dvs_sdi -c $MAX_CHANNELS -mode $MODE -mc 0 -mt LTC -rt LTC -f $PRIMARY_BUFFER -s $SECONDARY_BUFFER
