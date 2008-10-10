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

if [ -n "$1" ] ; then
	true
else
	tar cf - `find . -type f -not -name publish_to_sf.sh -not -name Root -not -name Repository -not -name Entries` | $TSOCKS ssh $SF_USER@shell.sf.net 'cd /home/groups/i/in/ingex/htdocs && tar xvf -'
fi
