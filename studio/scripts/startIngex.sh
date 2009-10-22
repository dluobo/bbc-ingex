#!/bin/sh

# Starts the Ingex processes as enabled in the file ~/ingex.conf or /etc/ingex.conf

usage="Usage: $0 [-h]
Options:
  -h  Start in HD mode
"

HD_MODE=0
while getopts 'h' OPT
do
  case $OPT in
  h)   HD_MODE=1
       ;;
  ?)   echo "${usage}"
       exit 1
       ;;
  esac
done


# File to store PIDs of the konsoles started here, so we can close them later when stopping ingex with stopIngex.sh
KONSOLE_PIDS="/tmp/ingexPIDs.txt"

# Set defaults for Ingex processes and parameters
# These can be overidden by ~/ingex.conf or /etc/ingex.conf
declare -x INGEX_DIR="/home/ingex/ap-workspace/ingex"

CAPTURE=1
MULTICAST=0
TRANSFER=0
INGEX_MONITOR=0
SYSTEM_MONITOR=0
QUAD_SPLIT=0
RECORDERS=

# Nameserver details
NAMESERVER=":8888"
DOTTED_DECIMAL=1

# Database details
DB_HOST='localhost'
DB_NAME='prodautodb'
DB_USER='bamzooki'
DB_PASS='bamzooki'

# *** Default HD Capture options ****
HD_CAPTURE_CHANNELS=2
HD_CAPTURE_MODE="1920x1080i50"
HD_CAPTURE_PRIMARY_BUFFER="YUV422"
HD_CAPTURE_SECONDARY_BUFFER="YUV422"
HD_CAPTURE_TIMECODE="LTC"
#HD_CAPTURE_OPTIONS="-aes8"
HD_CAPTURE_OPTIONS=""

# HD Recorders (space-separated)
HD_RECORDERS="Ingex-HD"

# HD Quad player options
HD_QUAD_OPTIONS="--disable-shuttle --audio-lineup -18 --audio-mon 2 --source-aspect 16:9 --quad-split --hide-progress-bar --shm-in 0s --shm-in 1s"

# Default SD Capture options
SD_CAPTURE_CHANNELS=4
SD_CAPTURE_MODE="PAL"
SD_CAPTURE_PRIMARY_BUFFER="YUV422"
SD_CAPTURE_SECONDARY_BUFFER="None"
SD_CAPTURE_TIMECODE="LTC"
SD_CAPTURE_OPTIONS=""

# SD Recorders (space-separated)
SD_RECORDERS="Ingex"

# SD Quad player options
SD_QUAD_OPTIONS="--disable-shuttle --audio-lineup -18 --audio-mon 2 --source-aspect 16:9 --quad-split --hide-progress-bar --shm-in 0p --shm-in 1p --shm-in 2p --shm-in 3p"

# Transfer (copy) options
# Set COPY_FTP_SERVER to be the hostname of your ftp server, 
# otherwise the transfer will be done by file copying.
COPY_FTP_SERVER=
#COPY_FTP_SERVER='yourFTPserver'
COPY_FTP_USER='ingex'
COPY_FTP_PASSWORD='ingex'

# Priority 1 files (usually the offline quality) can also be copied to a second destination, typically a local USB drive. Put the destination here to activate this (e.g. /video_removeable which might link to your usb drive)
#COPY_EXTRA_DEST='/video_removeable'
COPY_EXTRA_DEST=

# **** end of transfer options ****


# ***** Multicasting *****
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

# ********** There are no options to edit below this line *************
#**********************************************************************

# Get the required configuration
if [ -r $HOME/ingex.conf ]
then
        # Read in ingex mode configuration
        . $HOME/ingex.conf

elif [ -r /etc/ingex.conf ]
then
        # Read in ingex mode configuration
        . /etc/ingex.conf
fi

# set up paths etc
capture_path="$INGEX_DIR/studio/capture/"
xfer_path="$INGEX_DIR/studio/processing/media_transfer/"
scripts_path="$INGEX_DIR/studio/scripts/"

# recorder details
recorder_path="$INGEX_DIR/studio/ace-tao/Recorder/"
run_recorder="./Recorder"

routerlogger_path="$INGEX_DIR/studio/ace-tao/routerlog/"
run_routerLogger="./run_routerlogger.sh"

# assemble the database arguments 
DB_PARAMS="--dbhost $DB_HOST --dbname $DB_NAME --dbuser $DB_USER --dbpass $DB_PASS"

# Check to see if capture is already running. If it is, check with the user to avoid restarting by mistake
ABORT=0
if sudo killall -q -0 -e dvs_sdi
then
  kdialog --title "Ingex is already running" --warningyesno "Are you sure you want to restart Ingex?"
  ABORT=$?
fi
if [ $ABORT -ne 0 ] ; then
  exit 1
fi

# first kill quad split, recorder and capture processes, if they're running
# Check whether process is running and kill if it is
echo "Checking for Ingex processes..."
PROCESSES="player Recorder nexus_multicast nexus_web sys_info_web dvs_sdi xferserver.pl"
for PROC in  $PROCESSES ; do
  if sudo killall -q -0 -e ${PROC}
  then
    sudo killall -s SIGINT -e ${PROC}
    while sudo killall -q -0 -e ${PROC}
    do
    	usleep 100000
    done
  fi
  usleep 1000000
done

# close the konsoles for the above processes
if [ -r ${KONSOLE_PIDS} ] ; then
  #echo "Found PIDs file"
  for PID in $( < ${KONSOLE_PIDS} ); do
    if sudo kill -0 $PID
    then
      # echo "Killing $PID"
      sudo kill $PID
    fi
  done
  rm -f ${KONSOLE_PIDS}
fi
# If any were already running, by now, all Ingex precesses and their konsoles should have stopped

# Start Ingex
echo "Starting Ingex..."

# set corba options
CORBA_OPTIONS="-ORBDefaultInitRef corbaloc:iiop:$NAMESERVER -ORBDottedDecimalAddresses $DOTTED_DECIMAL"

# set the capture mode for HD or SD
if [ $HD_MODE -ge 1 ] ; then
    # Set capture options to HD
    CAPTURE_CHANNELS="${HD_CAPTURE_CHANNELS}"
    CAPTURE_MODE="${HD_CAPTURE_MODE}"
    CAPTURE_PRIMARY_BUFFER="${HD_CAPTURE_PRIMARY_BUFFER}"
    CAPTURE_SECONDARY_BUFFER="${HD_CAPTURE_SECONDARY_BUFFER}"
    CAPTURE_TIMECODE="${HD_CAPTURE_TIMECODE}"
    CAPTURE_OPTIONS="${HD_CAPTURE_OPTIONS}"

    # Recorders
    RECORDERS="${HD_RECORDERS}"

    # Quad player options
    QUAD_OPTIONS="${HD_QUAD_OPTIONS}"
else
    # Set capture options to SD
    CAPTURE_CHANNELS="${SD_CAPTURE_CHANNELS}"
    CAPTURE_MODE="${SD_CAPTURE_MODE}"
    CAPTURE_PRIMARY_BUFFER="${SD_CAPTURE_PRIMARY_BUFFER}"
    CAPTURE_SECONDARY_BUFFER="${SD_CAPTURE_SECONDARY_BUFFER}"
    CAPTURE_TIMECODE="${SD_CAPTURE_TIMECODE}"
    CAPTURE_OPTIONS="${SD_CAPTURE_OPTIONS}"

    # Recorders (space-separated)
    RECORDERS="${SD_RECORDERS}"

    # Quad player options
    QUAD_OPTIONS="${SD_QUAD_OPTIONS}"

fi

# export some settings, so other scripts can pick them up if necessary
declare -x INGEX_CHANNELS=${CAPTURE_CHANNELS}

# get the pids of existing windows
PIDS=`pidof konsole`

# make an exclusion list of existing windows
ARGS=""
for pid in $PIDS; do
        ARGS="${ARGS} -o ${pid}"
done

# start a new konsole window
kstart --geometry 0x08+0+15 \
konsole \
--script \
--noscrollbar \
--nomenubar

# find the pid of the new window
PID=$(pidof ${ARGS} konsole)

capture_window="konsole-${PID}"

# save PID
echo ${PID} >> ${KONSOLE_PIDS}

#echo "New Window is $capture_window..."

# seem to need this to allow things to settle!
sleep 1
# move/resize the window in pixels
dcop $capture_window konsole-mainwindow#1 setGeometry 0,0,1024,344

#echo "Started konsole $capture_window..."
tab=$(dcop $capture_window konsole currentSession)

# set the required script path to all sessions
if [ $CAPTURE -ge 1 ] ; then
  dcop $capture_window $tab renameSession Capture
  dcop $capture_window $tab sendSession "cd $capture_path"
  dcop $capture_window $tab sendSession "sudo ./dvs_sdi -c $CAPTURE_CHANNELS -mode $CAPTURE_MODE -f $CAPTURE_PRIMARY_BUFFER -s $CAPTURE_SECONDARY_BUFFER -mc 0 -tt $CAPTURE_TIMECODE $CAPTURE_OPTIONS"
  tab=
fi

# ** multicasting **
if [ $MULTICAST -ge 1 ] ; then
  if [ -z $tab ] ; then
    tab=$(dcop $capture_window konsole newSession)
    sleep 1
  fi
  # Prepare options. If mpeg transport stream, set -t option and bitrate
  if [ $MULTICAST_MPEG_TS -ge 1 ] ; then
    OPTIONS="-t -b ${MULTICAST_MPEG_BIT_RATE}"
  fi
  OPTIONS="${OPTIONS} -s ${MULTICAST_SIZE}"

  dcop $capture_window $tab renameSession Multicast
  dcop $capture_window $tab sendSession "cd $scripts_path"

  CHAN=0
  PORT=$MULTICAST_FIRST_PORT
  while [ "$CHAN" -lt "$CHANNELS" ] ; do
    #echo "Starting multicast channel ${CHAN}"
    dcop $capture_window $tab sendSession "$MULTICAST -c ${CHAN} -q ${OPTIONS} ${MULTICAST_ADDR}:${PORT} &"
    let CHAN=$CHAN+1
    let PORT=$PORT+1
  done
  tab=
fi

# ** Transfer **
if [ $TRANSFER -ge 1 ] ; then
  if [ -z $tab ] ; then
    tab=$(dcop $capture_window konsole newSession)
    sleep 1
  fi
  dcop $capture_window $tab renameSession Copy
  dcop $capture_window $tab sendSession "cd $xfer_path"
  if [ -n "${COPY_EXTRA_DEST}" ] ; then
    EXTRA_DEST="-e ${COPY_EXTRA_DEST}"
  fi
  if [ -z "${COPY_FTP_SERVER}" ] ; then
    dcop $capture_window $tab sendSession "./xferserver.pl ${EXTRA_DEST}"
  else
    FTP_OPTIONS="${COPY_FTP_SERVER} ${COPY_FTP_USER} ${COPY_FTP_PASSWORD}" 
    dcop $capture_window $tab sendSession "./xferserver.pl ${EXTRA_DEST} -f $FTP_OPTIONS"
  fi
  tab=
fi

if [ $INGEX_MONITOR -ge 1 ] ; then
  if [ -z $tab ] ; then
    tab=$(dcop $capture_window konsole newSession)
    sleep 1
  fi
  dcop $capture_window $tab renameSession nexusWeb
  dcop $capture_window $tab sendSession "cd $capture_path"
  dcop $capture_window $tab sendSession "./nexus_web"
  tab=
fi

if [ $SYSTEM_MONITOR -ge 1 ] ; then
  if [ -z $tab ] ; then
    tab=$(dcop $capture_window konsole newSession)
    sleep 1
  fi
  dcop $capture_window $tab renameSession sysInfo
  dcop $capture_window $tab sendSession "cd $capture_path"
  dcop $capture_window $tab sendSession "./system_info_web"
  tab=
fi
if [ $QUAD_SPLIT -ge 1 ] ; then
  if [ -z $tab ] ; then
    tab=$(dcop $capture_window konsole newSession)
    sleep 1
  fi
  dcop $capture_window $tab renameSession Quad
  dcop $capture_window $tab sendSession "cd $scripts_path"
  dcop $capture_window $tab sendSession "$INGEX_DIR/player/ingex_player/player $QUAD_OPTIONS"
  tab=
fi
if [ $ROUTER_LOGGER -ge 1 ] ; then
  if [ -z $tab ] ; then
    tab=$(dcop $capture_window konsole newSession)
    sleep 1
  fi
  dcop $capture_window $tab renameSession RouterLogger
  dcop $capture_window $tab sendSession "cd $routerlogger_path"
  dcop $capture_window $tab sendSession "${run_routerLogger}"
  tab=
fi

# Start recorders if any are set in the configuration
if [ -n "$RECORDERS" ] ; then
  # get the pids of exisiting windows
  PIDS=`pidof konsole`

  # make an exclusion list of existing windows
  ARGS=""
  for pid in $PIDS; do
    ARGS="${ARGS} -o ${pid}"
  done

  # start a new konsole window
  kstart --geometry 0x08+0+15 \
  konsole \
   --script \
   --noscrollbar \
   --nomenubar

  # find the pid of the new window
  PID=$(pidof ${ARGS} konsole)

  recorder_window="konsole-${PID}"
  #echo "New Window is $recorder_window..."
  
  # save PID
  echo ${PID} >> ${KONSOLE_PIDS}
  
  # seem to need this to allow things to settle!
  sleep 1

  # move/resize the window in pixels
  dcop $recorder_window konsole-mainwindow#1 setGeometry 0,500,1024,344
  #echo "Started konsole $recorder_window..."

  # start ingex recorders
  tab=$(dcop $recorder_window konsole currentSession)
  for REC in $RECORDERS ; do
    echo "Starting recorder: $REC" 
    if [ -z $tab ] ; then
      tab=$(dcop $recorder_window konsole newSession)
    fi
    #echo "tab is $tab"
    dcop $recorder_window $tab renameSession $REC
    sleep 1
    dcop $recorder_window $tab sendSession "cd $recorder_path"
    dcop $recorder_window $tab sendSession "${run_recorder} --name ${REC} ${DB_PARAMS} ${CORBA_OPTIONS}"
    tab=
  done

fi

