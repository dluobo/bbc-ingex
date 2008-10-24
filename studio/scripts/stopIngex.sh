#!/bin/sh

# Stop the Ingex processes listed here
PROCESSES="player Recorder nexus_multicast nexus_web sys_info_web dvs_sdi xferserver.pl"

# Also kill the PIDs that are in the file named here
KONSOLE_PIDS="/tmp/ingexPIDs.txt"

# Check to see if capture is already running. If it is, check with the user to avoid stopping it by mistake
ABORT=0
if sudo killall -q -0 -e dvs_sdi
then
  kdialog --title "Confirm... " --warningyesno "Ingex is running. Are you sure you want to stop it?"
  ABORT=$?
fi
if [ $ABORT -ne 0 ] ; then
  exit 1
fi

# first kill quad split, recorder and capture processes, if they're running.
# Check whether process is running and kill if it is
echo "Stopping Ingex processes..."

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



echo "All Ingex processes stopped."