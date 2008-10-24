#!/bin/sh

# Starts the Ingex processes as enabled in the file ./ingex.conf or /etc/ingex.conf

# File to store PIDs of the konsoles started here, so we can clsoe them later when stopping ingex with stopIngex.sh
KONSOLE_PIDS="/tmp/ingexPIDs.txt"

# set up paths etc
INGEX_DIR="/home/ingex/ap-workspace/ingex/studio"
capture_path="$INGEX_DIR/capture/"
xfer_path="$INGEX_DIR/processing/media_transfer/"
scripts_path="$INGEX_DIR/scripts/"

# recorder details
recorder_path="$INGEX_DIR/ace-tao/Recorder/"
run_recorder="./Recorder"
dbLogin=" bamzooki bamzooki"

routerlogger_path="$INGEX_DIR/ace-tao/routerlog/"
run_routerLogger="./routerlogger"

# Set defaults for Ingex processes to start
# These can be overidden by ./ingex.conf and /etc/ingex.conf
CAPTURE=1
MULTICAST=0
TRANSFER=0
INGEX_MONITOR=0
SYSTEM_MONITOR=0
QUAD_SPLIT=0
RECORDERS=
NAMESERVER="192.168.1.181:8888"
# set corba options for recorder command
CORBA_OPTIONS=" -ORBDefaultInitRef corbaloc:iiop:$NAMESERVER -ORBDottedDecimalAddresses 1"

ROUTER_LOGGER=0
ROUTER_TTY="/dev/ttyS0"
ROUTER_PORTS=" -n RouterLog1 -m 5 -d VT1 -p 1 -d VT2 -p 2 -d VT3 -p 3 -d VT4 -p 4 -n RouterLog2 -m 6 -d VT1 -p 1 -d VT2 -p 2 -d VT3 -p 3 -d VT4 -p 4"
ROUTER_LOGGER_CORBA=" -ORBDefaultInitRef corbaloc:iiop:192.168.1.181:8888 -ORBDottedDecimalAddresses 1"

run_routerLogger="${run_routerLogger} -r ${ROUTER_TTY} -v ${ROUTER_PORTS} ${ROUTER_LOGGER_CORBA}"


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

# Get the configuration required for this PC
if [ -r ./ingex.conf ]
then
        # Read in ingex mode configuration
        . ./ingex.conf

elif [ -r /etc/ingex.conf ]
then
        # Read in ingex mode configuration
        . /etc/ingex.conf
fi

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
  dcop $capture_window $tab sendSession "./capture.sh"
  tab=
fi

if [ $MULTICAST -ge 1 ] ; then
  if [ -z $tab ] ; then
    tab=$(dcop $capture_window konsole newSession)
    sleep 1
  fi
  dcop $capture_window $tab renameSession Multicast
  dcop $capture_window $tab sendSession "cd $scripts_path"
  dcop $capture_window $tab sendSession "./start_multicast.sh"
  tab=
fi

if [ $TRANSFER -ge 1 ] ; then
  if [ -z $tab ] ; then
    tab=$(dcop $capture_window konsole newSession)
    sleep 1
  fi
  dcop $capture_window $tab renameSession Copy
  dcop $capture_window $tab sendSession "cd $xfer_path"
  dcop $capture_window $tab sendSession "./xferserver.pl"
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
  dcop $capture_window $tab sendSession "./quad-split.sh"
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
    dcop $recorder_window $tab sendSession "${run_recorder} ${REC} ${dbLogin} ${CORBA_OPTIONS}"
    tab=
  done

fi