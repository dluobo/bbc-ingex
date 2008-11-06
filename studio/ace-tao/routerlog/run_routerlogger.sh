#! /bin/sh

# StudioA
./routerlogger -v -r 192.168.1.252:4098 -s -t 192.168.1.252:4097 -u \
  -n Router-StudioA -m 9 -d "StudioA - VT1" -p 5 -d "StudioA - VT2" -p6 -d "StudioA - VT3" -p 7 -d "StudioA - VT4" -p 8 \
  -f /store/directors_cut/studioA.db -a corbaloc:iiop:192.168.1.131:8888/NameService \
  -ORBDottedDecimalAddresses 1

# Lot and Stage
#./routerlogger -r /dev/ttyS0 -t /dev/ttyS1 \
#  -n Router-Lot -m 12 -d "Lot - VT1" -p 5 -d "Lot - VT2" -p6 -d "Lot - VT3" -p 7 -d "Lot - VT4" -p 8 \
#  -f /store/directors_cut/lot.db -a corbaloc:iiop:192.168.1.18:8888/NameService \
#  -n Router-Stage -m 11 -d "Stage - VT1" -p 1 -d "Stage - VT2" -p2 -d "Stage - VT3" -p 3 -d "Stage - VT4" -p 4 \
#  -f /store/directors_cut/stage.db -a corbaloc:iiop:192.168.1.182:8888/NameService \
#  -ORBDottedDecimalAddresses 1


