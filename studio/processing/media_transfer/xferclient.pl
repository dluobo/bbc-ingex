#! /usr/bin/perl -w

#/***************************************************************************
# *   Copyright (C) 2008 British Broadcasting Corporation                   *
# *   - all rights reserved.                                                *
# *                                                                         *
# *   This program is free software; you can redistribute it and/or modify  *
# *   it under the terms of the GNU General Public License as published by  *
# *   the Free Software Foundation; either version 2 of the License, or     *
# *   (at your option) any later version.                                   *
# *                                                                         *
# *   This program is distributed in the hope that it will be useful,       *
# *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
# *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
# *   GNU General Public License for more details.                          *
# *                                                                         *
# *   You should have received a copy of the GNU General Public License     *
# *   along with this program; if not, write to the                         *
# *   Free Software Foundation, Inc.,                                       *
# *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
# ***************************************************************************/

# This script is intended to be called by Ingex recorders at startup and the start and end of recordings.
# All parameters are passed to the server xferserver.pl, listening on port PORT, and then the script exits.
# This allows copying to occur to a server from several recorders on the same machine, with traffic control ensuring that disk IO is not saturated by copying while recordings are taking place.
# See comments in xferserver.pl for the parameter format.

use strict;
use IO::Socket;

use constant PORT => 2000;

my $sock = new IO::Socket::INET(
	PeerAddr => 'localhost',
 	PeerPort => PORT,
# 	Type => SOCK_STREAM,
 	) or die 'Could not connect to port ' . PORT . ": $!\n";

foreach (@ARGV) {
	print $sock "$_\n";
}
