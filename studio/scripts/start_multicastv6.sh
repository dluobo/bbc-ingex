#! /bin/bash

# choose sending from either "den" or "evan"
LOCATION=den
#LOCATION=evan

# multicast executable
MULTICAST=/home/ingex/ap-workspace/ingex/studio/capture/nexus_multicast

# -g option runs program under gdb so that a crash displays a stack trace
if [ "$1" = "-g" ] ; then
	MULTICAST="gdb -q -ex run -ex where --args $MULTICAST"
	shift			# swallow option
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


# start multicast streams

if [ "$LOCATION" = "den" ]
then

    # for den (cam 1-4)
    echo Sending from den...
    $MULTICAST -c 0 -q -s 240x192 [ff1e::1]:2000 &
    $MULTICAST -c 1 -q -s 240x192 [ff1e::1]:2001 &
    $MULTICAST -c 2 -q -s 240x192 [ff1e::1]:2002 &
    $MULTICAST -c 3 -q -s 240x192 [ff1e::1]:2003 &

elif [ "$LOCATION" = "evan" ]
then

    # for evan (cam 7 and 5,6)
    echo Sending from evan...
    $MULTICAST -c 0 -q -s 240x192 [ff1e::2]:2000 &
    # not used#$MULTICAST -c 1 -q -s 240x192 239.255.1.2:2001 &
    $MULTICAST -c 2 -q -s 240x192 [ff1e::2]:2002 &
    $MULTICAST -c 3 -q -s 240x192 [ff1e::2]:2003 &

else

    echo Unknown location.

fi

