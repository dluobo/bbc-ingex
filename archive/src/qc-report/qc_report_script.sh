#!/bin/sh

# this script can be called by the qc_player just prior to quiting a session


OUTPUT_LOG_FILENAME="/tmp/qcreportout.txt"

# create a KDE progress bar dialog
dcopRef=`kdialog --progressbar "Processing QC session" 100`

if ! (qc_report --print-qc-url --progress-dcop "$dcopRef" --progress-start 0 --progress-end 95 $@ > $OUTPUT_LOG_FILENAME)
then
	dcop $dcopRef close
	echo "Failed to process qc session"
	kdialog --title "Failed to process QC session" --passivepopup "" 4
	exit 1
fi

# done creating report
dcop $dcopRef setProgress 100
dcop $dcopRef setLabel "Complete"
sleep 1
dcop $dcopRef close


# open firefox with report for printing
cat $OUTPUT_LOG_FILENAME | if read qcReportFilename; then firefox "$qcReportFilename"; fi




