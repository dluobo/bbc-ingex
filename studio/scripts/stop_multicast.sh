#! /bin/bash

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

exit
