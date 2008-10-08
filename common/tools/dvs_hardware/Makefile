#
# $Id: Makefile,v 1.3 2008/10/08 10:17:39 john_f Exp $
#
# Makefile for building DVS hardware capture & playout tools
#
# Copyright (C) 2005 BBC, author Stuart Cunningham <stuart_hc@users.sourceforge.net>
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

CFLAGS=-Wall -g -DDEBUG -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
OPT=-O3 -mmmx -msse2
FLAGS=$(CFLAGS) $(OPT)


SDI_PROGS=recplay playout record record_mxf playout_non_mt check_inputs
PROGS=$(SDI_PROGS)

# dvs_sdk_paths.mk searches for sdk and sets HARDWARE_INCLUDE and HARDWARE_LIB
# search is overridden by setting DVSSDK
include ../../dvs_sdk_paths.mk

ifndef HARDWARE_INCLUDE
  $(info SDI tools will not be built)
  PROGS=
endif

INCLUDE=-I../.. -I.. $(HARDWARE_INCLUDE)


.PHONY: all
all: $(PROGS) libdvsoem_dummy.a

libdvsoem_dummy.a: dvsoem_dummy.c
	gcc $(FLAGS) $(INCLUDE) -c dvsoem_dummy.c
	ar crv libdvsoem_dummy.a dvsoem_dummy.o
	
playout: playout.c
	g++ $(FLAGS) $(INCLUDE) playout.c $(HARDWARE_LIB) -lpthread -o playout

record: record.c ../../libcommon.a
	g++ $(FLAGS) -I. $(INCLUDE) record.c ../../libcommon.a $(HARDWARE_LIB) -lpthread -o record

record_mxf: record_mxf.c ../../libcommon.a
	g++ $(FLAGS) -I. $(INCLUDE) -I../../../libMXF/lib/include -I../../../libMXF/examples/archive -I../../../libMXF/examples/archive/write record_mxf.c ../../libcommon.a $(HARDWARE_LIB) -lpthread -L../../../libMXF/examples/archive/write -L../../../libMXF/lib -lwritearchivemxf -lMXF -luuid -o record_mxf

recplay: recplay.c ../../libcommon.a
	g++ $(FLAGS) -I. $(INCLUDE) recplay.c ../../libcommon.a $(HARDWARE_LIB) -lpthread -lncurses -o recplay

playout_non_mt: playout_non_mt.c
	g++ $(FLAGS) $(INCLUDE) playout_non_mt.c $(HARDWARE_LIB) -lpthread -o playout_non_mt

check_inputs: check_inputs.c
	g++ $(FLAGS) $(INCLUDE) check_inputs.c $(HARDWARE_LIB) -o check_inputs

clean:
	rm -f $(SDI_PROGS) *.o libdvsoem_dummy.a
