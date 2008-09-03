#
# $Id: Makefile,v 1.6 2008/09/03 14:13:26 john_f Exp $
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

CFLAGS=-Wall -g -DDEBUG -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
INCLUDES=-I../../common
LIBS=-L../../common -lcommon
OPT=-O3 -mmmx -msse2
FLAGS=$(CFLAGS) $(INCLUDES) $(OPT)

# dvs_sdk_paths.mk searches for sdk and sets HARDWARE_INCLUDE and HARDWARE_LIB
# search is overridden by setting DVSSDK
include ../../common/dvs_sdk_paths.mk

ifndef HARDWARE_INCLUDE
  $(warning DVSSDK required to build dvs_sdi)
endif

SDI_PROGS=dvs_sdi testgen
PROGS=$(SDI_PROGS) nexus_save nexus_xv nexus_stats nexus_web system_info_web nexus_multicast nexus_set_source 

.PHONY: all
all: $(PROGS)

nexus_control.o: nexus_control.h nexus_control.c
	g++ $(FLAGS) -c nexus_control.c

dvs_sdi: dvs_sdi.c nexus_control.h
	g++ $(FLAGS) $(HARDWARE_INCLUDE) dvs_sdi.c $(HARDWARE_LIB) $(LIBS) -lpthread -o dvs_sdi

testgen: testgen.c nexus_control.h
	g++ $(FLAGS) testgen.c $(LIBS) -lpthread -o testgen

nexus_stats: nexus_stats.c nexus_control.h nexus_control.o
	g++ $(FLAGS) nexus_stats.c nexus_control.o $(LIBS) -lpthread -o nexus_stats

nexus_web: nexus_web.c nexus_control.h nexus_control.o
	@if [ -r /usr/include/shttpd.h -o -r /usr/local/include/shttpd.h ] ; then set -x ; g++ $(FLAGS) nexus_web.c nexus_control.o $(LIBS) -lpthread -o nexus_web -lshttpd -ldl ; else echo "shttpd.h not found - nexus_web will not be built"; echo ; fi

system_info_web: system_info_web.c
	@if [ -r /usr/include/shttpd.h -o -r /usr/local/include/shttpd.h ] ; then set -x ; g++ $(FLAGS) system_info_web.c -o system_info_web -lshttpd -ldl -lpthread ; else echo "shttpd.h not found - system_info_web_web will not be built"; echo ; fi

nexus_set_source: nexus_set_source.c nexus_control.h
	g++ $(FLAGS) nexus_set_source.c $(LIBS) -lpthread -o nexus_set_source

ifeq ($(USE_FFMPEG), 1)
nexus_save: nexus_save.c nexus_control.h nexus_control.o ../common/ffmpeg_encoder.o
	g++ $(FLAGS) -DUSE_FFMPEG nexus_save.c -o nexus_save nexus_control.o ../common/ffmpeg_encoder.o -lavformat -lavcodec -lavutil -lz -lbz2 -lmp3lame -lx264 -la52 -lfaac -lfaad -lm -lpthread
else
nexus_save: nexus_save.c nexus_control.h nexus_control.o
	g++ $(FLAGS) nexus_save.c -o nexus_save nexus_control.o
endif

nexus_multicast: nexus_multicast.c nexus_control.h nexus_control.o
	g++ -I../../common $(FLAGS) nexus_multicast.c -o nexus_multicast nexus_control.o -L../../common -lcommon -L../../common/YUVlib -lYUVlib -lpthread

nexus_xv: nexus_xv.c nexus_control.h nexus_control.o
	g++ $(FLAGS) nexus_xv.c -o nexus_xv nexus_control.o -L/usr/X11R6/lib -L/usr/X11R6/lib64 -lXv

clean:
	rm -f core $(PROGS)
