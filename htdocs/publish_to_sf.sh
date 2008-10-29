#!/bin/sh

# Use hostname to see if we're on a direct internet connection
# or whether we need to use a SOCKS proxy
case "$HOSTNAME" in
	hobbes|max|hoyle)
		TSOCKS=""
		;;
	*)
		TSOCKS=tsocks
	;;
esac

case "$USER" in
	stuartc)
		SF_USER=stuart_hc
		;;
	philipn)
		SF_USER=philipn
		;;
	john)
		SF_USER=john_f
		;;
	david)
		SF_USER=david_gk
		;;
	*)
		echo "Add your sourceforge username to this script's switch statement"
		;;
esac

# Turn on echoing of script commands
set -x

# Pass any script args straight to rsync command e.g. --dry-run
$TSOCKS rsync -aiv --no-perms --executability --exclude publish_to_sf.sh --exclude '*.swp' --cvs-exclude $* . $SF_USER,ingex@web.sf.net:htdocs
