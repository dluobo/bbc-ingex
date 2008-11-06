#
# $Id: Makefile,v 1.2 2008/11/06 11:33:38 john_f Exp $
#
# Makefile for building Ingex media processing tools
#
# Copyright (C) 2008  BBC Research, Stuart Cunningham <stuart_hc@users.sourceforge.net>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

.PHONY: all
all:
	$(MAKE) -C offline
	$(MAKE) -C media_transfer
	$(MAKE) -C createaaf

.PHONY: clean
clean:
	$(MAKE) -C offline $@
	$(MAKE) -C media_transfer $@
	$(MAKE) -C createaaf $@
