./routerlogger -r /dev/ttyS0 -v \
  -n RouterLog1 -m 5 -d VT1 -p 1 -d VT2 -p2 -d VT3 -p 3 -d VT4 -p 4 \
  -n RouterLog2 -m 6 -d VT1 -p 1 -d VT2 -p2 -d VT3 -p 3 -d VT4 -p 4 \
  -ORBDefaultInitRef corbaloc:iiop:192.168.1.181:8888 -ORBDottedDecimalAddresses 1
