#!/bin/sh

avid=DETRITUS
#avid=N226

offline_video=/video2/mxf_offline
offline_avid=/samba-exports/avid_offline/Avid\ MediaFiles/MXF

online_video=/video2/mxf_online
online_avid=/samba-exports/avid_online/Avid\ MediaFiles/MXF

online2_video=/video2/mxf_online_lot
online2_avid=/samba-exports/avid_online_lot/Avid\ MediaFiles/MXF

for i in 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30
do
  date=200811$i
  echo $date

#  mkdir $offline_video/$date
  cd "$offline_avid"
  rm $avid.$date
  ln -s $offline_video/$date $avid.$date

#  mkdir $online_video/$date
  cd "$online_avid"
  rm $avid.$date
  ln -s $online_video/$date $avid.$date

#  mkdir $online2_video/$date
  cd "$online2_avid"
  rm $avid.$date
  ln -s $online2_video/$date $avid.$date
done

