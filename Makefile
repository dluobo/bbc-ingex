#
# $Id: Makefile,v 1.1 2008/10/29 17:27:13 john_f Exp $
#
# Makefile for building the Ingex suite of applications
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

# archive is not included in 'all' until further testing is completed
.PHONY: all
all:
	$(MAKE) -C libMXF
	$(MAKE) -C common
	$(MAKE) -C player
	$(MAKE) -C studio

.PHONY: studio
studio:
	$(MAKE) -C libMXF
	$(MAKE) -C common
	$(MAKE) -C player
	$(MAKE) -C studio

.PHONY: archive
archive:
	$(MAKE) -C libMXF
	$(MAKE) -C common
	$(MAKE) -C player
	$(MAKE) -C archive

.PHONY: clean
clean:
	$(MAKE) -C libMXF $@
	$(MAKE) -C common $@
	$(MAKE) -C player $@
	$(MAKE) -C studio $@
	$(MAKE) -C archive $@

# So far, only libMXF and common have make check targets
.PHONY: check
check: all
	$(MAKE) -C libMXF $@
	$(MAKE) -C common $@

.PHONY: valgrind-check
valgrind-check: all
	$(MAKE) -C libMXF $@
	$(MAKE) -C common $@
