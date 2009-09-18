#! /bin/sh

# Choose config depending on location
LOCATION=studio
#LOCATION=stage

if [ "$LOCATION" = "studio" ]
then
# StudioA
./routerlogger -v -r 192.168.1.246:4098 -s -l \
  -n Router-StudioA \
  -m 9 \
  -d "StudioA-VT1" -p 1 \
  -d "StudioA-VT2" -p 2 \
  -d "StudioA-VT3" -p 3 \
  -d "StudioA-VT4" -p 4 \
  -c "StudioA multi-camera clip" \
  -a corbaloc:iiop:192.168.1.136:8888/NameService \
  -ORBDottedDecimalAddresses 1

elif [ "$LOCATION" = "stage" ]
then

# Lot and Stage
#./routerlogger -r /dev/ttyS0 -t /dev/ttyS1 \
./routerlogger -v -r 192.168.1.252:4098 -s  \
  -n Router-Lot \
  -m 12 \
  -d "Lot-VT1" -p 5 \
  -d "Lot-VT2" -p 6 \
  -d "Lot-VT3" -p 7 \
  -d "Lot-VT4" -p 8 \
  -c "Lot multi-camera clip" \
  -a corbaloc:iiop:192.168.1.181:8888/NameService \
  -n Router-Stage \
  -m 11 \
  -d "Stage-VT1" -p 1 \
  -d "Stage-VT2" -p 2 \
  -d "Stage-VT3" -p 3 \
  -d "Stage-VT4" -p 4 \
  -c "Stage multi-camera clip" \
  -a corbaloc:iiop:192.168.1.130:8888/NameService \
  -ORBDottedDecimalAddresses 1

else

echo Unknown location.

fi


