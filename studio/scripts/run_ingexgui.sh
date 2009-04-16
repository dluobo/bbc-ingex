#! /bin/sh

# -g option runs program under gdb so that a crash displays a stack trace
PRERUN=""
if [ "$1" = "-g" ] ; then
	PRERUN="gdb -q -ex run -ex where --args"
	shift			# swallow option
fi

$PRERUN /home/ingex/ap-workspace/ingex/studio/ace-tao/Ingexgui/src/ingexgui -ORBDefaultInitRef corbaloc:iiop:192.168.1.182:8888 -ORBDottedDecimalAddresses 1
