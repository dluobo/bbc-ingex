#
# $Id: Makefile,v 1.12 2009/03/19 18:03:58 john_f Exp $
#
# Makefile for building ingex server and utilities.
#
# Copyright (C) 2005  Stuart Cunningham <stuart_hc@users.sourceforge.net>
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

CFLAGS=-Wall -g -O3 -DDEBUG
INCLUDES=-I../../common
LIBS=-L../../common -lcommon
OPT=-mmmx -msse2
FLAGS=$(CFLAGS) $(INCLUDES) $(OPT) -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE

# dvs_sdk_paths.mk searches for sdk and sets DVS_HARDWARE_INCLUDE and DVS_HARDWARE_LIB
# search is overridden by setting DVSSDK
include ../../common/dvs_sdk_paths.mk

ifdef DVS_HARDWARE_INCLUDE
  SDI_PROGS=dvs_sdi dvs_dummy testgen
else
  $(warning dvs_sdi will not be built)
  SDI_PROGS=dvs_dummy testgen
endif

FFMPEG_LIBS=-lavformat -lavcodec -lswscale -lavutil -lz -lbz2 -lmp3lame -lx264 -lfaac -lfaad -lm -lpthread

PROGS=$(SDI_PROGS) nexus_save nexus_xv nexus_stats nexus_web system_info_web nexus_multicast nexus_set_source 

.PHONY: all
all: $(PROGS)

dvs_sdi: dvs_sdi.c
	g++ $(FLAGS) $(DVS_HARDWARE_INCLUDE) dvs_sdi.c $(DVS_HARDWARE_LIB) $(LIBS) -lpthread -o dvs_sdi $(FFMPEG_LIBS)

dvs_dummy: dvs_sdi.c
	g++ $(FLAGS) $(DUMMY_HARDWARE_INCLUDE) dvs_sdi.c $(DUMMY_HARDWARE_LIB) $(LIBS) -lpthread -o dvs_dummy $(FFMPEG_LIBS)

testgen: testgen.c
	g++ $(FLAGS) testgen.c $(LIBS) -lpthread -o testgen

nexus_stats: nexus_stats.c
	g++ $(FLAGS) nexus_stats.c $(LIBS) -lpthread -o nexus_stats

nexus_web: nexus_web.c
	@if [ -r /usr/include/shttpd.h -o -r /usr/local/include/shttpd.h ] ; then set -x ; g++ $(FLAGS) nexus_web.c $(LIBS) -lpthread -o nexus_web -lshttpd -ldl ; else echo "shttpd.h not found - nexus_web will not be built"; echo ; fi

system_info_web: system_info_web.c
	@if [ -r /usr/include/shttpd.h -o -r /usr/local/include/shttpd.h ] ; then set -x ; g++ $(FLAGS) system_info_web.c -o system_info_web -lshttpd -ldl -lpthread ; else echo "shttpd.h not found - system_info_web_web will not be built"; echo ; fi

nexus_set_source: nexus_set_source.c
	g++ $(FLAGS) nexus_set_source.c $(LIBS) -lpthread -o nexus_set_source

nexus_save: nexus_save.c ../common/ffmpeg_encoder.o
	g++ $(FLAGS) -DUSE_FFMPEG nexus_save.c -o nexus_save ../common/ffmpeg_encoder.o $(LIBS) $(FFMPEG_LIBS)

nexus_multicast: nexus_multicast.c
	g++ $(FLAGS) nexus_multicast.c -o nexus_multicast $(LIBS) -L../../common/YUVlib -lYUVlib $(FFMPEG_LIBS)

nexus_xv: nexus_xv.c
	g++ $(FLAGS) nexus_xv.c -o nexus_xv $(LIBS) -L/usr/X11R6/lib -L/usr/X11R6/lib64 -lXv

clean:
	rm -f core *.o dvs_sdi $(PROGS)
