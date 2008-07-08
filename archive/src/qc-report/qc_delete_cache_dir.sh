#!/bin/sh

# this script can be called by the qc_player just prior to deleting a LTO directory in the cache


# create a KDE progress bar dialog
dcopRef=`kdialog --progressbar "Processing LTO cache directory" 100`

if ! qc_report --progress-dcop "$dcopRef" --progress-start 0 --progress-end 95 --report $1 --lto $2
then
	dcop $dcopRef close
	echo "Failed to generate QC report for LTO directory $2"
	kdialog --title "Failed to generate QC report for LTO cache directory $2" --passivepopup "" 4
	exit 1
fi


# delete the directory
echo "Deleting LTO cache directory $2"
dcop $dcopRef setProgress 100
dcop $dcopRef setLabel "Deleting LTO cache directory $2"

if ! rm -Rf $2
then
	dcop $dcopRef close
	echo "Failed to delete LTO cache directory $2"
	kdialog --title "Failed to delete LTO cache directory $2" --passivepopup "" 4
	exit 1
fi


# done
dcop $dcopRef close
kdialog --title "LTO cache directory $2 was deleted" --passivepopup "" 2

