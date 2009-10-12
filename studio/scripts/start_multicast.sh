#! /bin/bash

# Set defaults for Ingex processes and parameters
# These can be overidden by ~/ingex.conf or /etc/ingex.conf

# Set number of streams to start by default
DEFAULT_CHANNELS=4

# Set the multicast address and first port to use. The same address will 
# be used for all channels (on this PC) but the port number will increment
MULTICAST_ADDR='239.255.1.1'
MULTICAST_FIRST_PORT=2000

# Set size of down-scaled image. Suggest 360x288 for an mpeg stream or 240x192 
# for simple down-scaling 
#MULTICAST_SIZE='240x192'
MULTICAST_SIZE='360x288'

# For an mpeg transport stream set the next parameter to 1, for image scaling
# only set it to zero. Set your desired mpeg bitrate (kb/s) too.
MULTICAST_MPEG_TS=1
MULTICAST_MPEG_BIT_RATE='1000'

# **** end of multicast options ****

# find ingex executables. Is path in environment?
if [ -z "$INGEX_DIR" ] ; then
  INGEX_DIR="/home/ingex/ap-workspace/ingex"
fi

# ********** There are no options to edit below this line *************
#**********************************************************************

# Get the required configuration
if [ -r $HOME/ingex.conf ] ; then
        # Read in ingex mode configuration
        . $HOME/ingex.conf
elif [ -r /etc/ingex.conf ] ; then
        # Read in ingex mode configuration
        . /etc/ingex.conf
fi

# See if number of channels to multicast is already set
if [ -n "$INGEX_CHANNELS" ] ; then
  CHANNELS=$INGEX_CHANNELS
else 
  CHANNELS=$DEFAULT_CHANNELS
fi

# multicast executable
MULTICAST="${INGEX_DIR}/studio/capture/nexus_multicast"

# Set options. If mpeg transport stream, set -t option and bitrate
if [ $MULTICAST_MPEG_TS -ge 1 ] ; then
  OPTIONS="-t -b ${MULTICAST_MPEG_BIT_RATE}"
fi
OPTIONS="${OPTIONS} -s ${MULTICAST_SIZE}"


# -g option runs program under gdb so that a crash displays a stack trace
if [ "$1" = "-g" ] ; then
   MULTICAST="gdb -q -ex run -ex where --args $MULTICAST"
   shift       # swallow option
fi

# stop all multicast processes

# Check whether process is running and kill if it is
if killall -q -0 -e nexus_multicast
then
   killall -e nexus_multicast
   while killall -q -0 -e nexus_multicast
   do
      usleep 100000
   done
fi

# start multicasting
CHAN=0
PORT=$MULTICAST_FIRST_PORT
while [ "$CHAN" -lt "$CHANNELS" ] ; do
  #echo "Starting channel ${CHAN}"
  $MULTICAST -c ${CHAN} -q ${OPTIONS} ${MULTICAST_ADDR}:${PORT} &
  let CHAN=$CHAN+1
  let PORT=$PORT+1
done

