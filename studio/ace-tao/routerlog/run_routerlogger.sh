#! /bin/sh

# StudioA
./routerlogger -v -r 192.168.1.246:4098 -s -t 192.168.1.246:4097 -u \
  -n Router-StudioA -m 9 -d "StudioA - VT1" -p 5 -d "StudioA - VT2" -p6 -d "StudioA - VT3" -p 7 -d "StudioA - VT4" -p 8 \
  -c "StudioA multi-camera clip" \
  -a corbaloc:iiop:192.168.1.181:8888/NameService \
  -ORBDottedDecimalAddresses 1

# Lot and Stage
#./routerlogger -r /dev/ttyS0 -t /dev/ttyS1 \
#./routerlogger -v -r 192.168.1.246:4098 -s -t 192.168.1.246:4097 -u \
#  -n Router-Lot -m 12 -d "Lot - VT1" -p 5 -d "Lot - VT2" -p6 -d "Lot - VT3" -p 7 -d "Lot - VT4" -p 8 \
#  -f /store/directors_cut/lot.db -a corbaloc:iiop:192.168.1.181:8888/NameService \
#  -n Router-Stage -m 11 -d "Stage - VT1" -p 1 -d "Stage - VT2" -p2 -d "Stage - VT3" -p 3 -d "Stage - VT4" -p 4 \
#  -f /store/directors_cut/stage.db -a corbaloc:iiop:192.168.1.184:8888/NameService \
#  -ORBDottedDecimalAddresses 1


