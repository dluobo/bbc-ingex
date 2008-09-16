#!/bin/sh

mode=PAL/YUV422/FRAME/AUDIO8CH
#mode=NTSC/YUV422/FRAME/AUDIO8CH
mc=on
SVRAM=$DVSSDK/linux-x86/bin/svram

for i in 0 1
do
  echo Setting card $i: $mode, multichannel $mc
  env SCSIVIDEO_CMD=PCI,card=$i $SVRAM mode $mode
  env SCSIVIDEO_CMD=PCI,card=$i $SVRAM multichannel $mc
done

