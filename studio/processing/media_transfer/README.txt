COPYING MATERIAL TO A SERVER

Each recorder's COPY_COMMAND should point to xferclient.pl, with ENCODE1_DEST, ENCODE2_DEST etc pointing to directories on the server(s).

There should be an instance of xferserver.pl running, which will take commands from xferclient.pl when each recorder runs it.

This in turn will call cpfs to copy each file, sending signals to switch rate control on and off as recordings start and stop.

The location of cpfs is defined in xferserver.pl.  After making cpfs, it will need to be copied to this location if it is not the current directory.

Alternatively, FTP can be used to transfer material - see xferserver.pl for command-line options

(The present scheme does not use media_transfer.pl.)

AUTOMATICALLY GENERATING SAMBA EXPORTS FOR AVID

export_for_avidd.pl needs to be run as a system service:

1) Put export_for_avidd.pl in /usr/local/sbin, and export_for_avidd in /etc/init.d, change ownerships to root and make exectable.

2) sudo /sbin/chkconfig -a export_for_avidd

AUTOMATICALLY IMPORTING MXF AND CUT DATA INTO SERVER DATABASE

import_db_infod.pl needs to be run as a system service:

1) Put import_db_infod.pl in /usr/local/sbin, and import_db_infod in /etc/init.d, change ownerships to root and make exectable.

2) sudo /sbin/chkconfig -a import_db_infod

ADDITIONAL PERL MODULES

Both the above need:
  Proc::Daemon
  Linux::Inotify2


