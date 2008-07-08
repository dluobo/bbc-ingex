#!/bin/sh

# Convert the legacy binary encoded timecode files to text encoded timecode files.


# Eg. steps to convert files for channel1:
#
# 1) Backup .tc files
#      find /mnt/store/browse/channel1 -name \*.tc | xargs cp -t /mnt/store/tc-backups/channel1
# 2) Convert .tc files
#      find /mnt/store/browse/channel1 -name \*.tc | xargs ./convert_binary_tc.sh

for tcFile in "$@"
do
	# convert and replace
	if ./convert_binary_tc $tcFile > dump; then
		cp dump $tcFile
	fi
done


