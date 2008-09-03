COPYING MATERIAL TO A SERVER

Each recorder's COPY_COMMAND should point to xferclient.pl, with ENCODE1_DEST, ENCODE2_DEST etc pointing to directories on the server(s).

There should be an instance of xferserver.pl running, which will take commands from xferclient.pl when each recorder runs it.

This in turn will call cpfs to copy each file, sending signals to switch rate control on and off as recordings start and stop.

The location of cpfs is defined in xferserver.pl.  After making cpfs, it will need to be copied to this location if it is not the current directory.

(The present scheme does not use media_transfer.pl.)