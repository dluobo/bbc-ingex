#
# $Id: Makefile,v 1.3 2009/12/17 15:51:22 john_f Exp $
#
# Makefile for building useful command-line tools
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

FLAGS=-Wall -W -Wno-unused-parameter -g -DDEBUG -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
OPT=-O3 -mmmx -msse2
CFLAGS=$(FLAGS) $(OPT)

MXF_INC=-I../../../libMXF/lib/include
MXF_LIB=-L../../libMXF/examples/archive/write -L../../libMXF/lib -lwritearchivemxf -lMXF -luuid

INCLUDE=-I.. -I../YUVlib $(MXF_INC)
LIB_COMMON=../libcommon.a

PROGS=convert_audio convert_10bit_video create_video_test_signal compare_archive_mxf clapperboard_avsync disk_rw_benchmark send_video receive_video

.PHONY: all
all: $(PROGS) dvs_hardware

.PHONY: dvs_hardware
dvs_hardware:
	cd dvs_hardware && $(MAKE)

convert_audio: convert_audio.c $(LIB_COMMON)
	gcc $(CFLAGS) $(INCLUDE) $< $(LIB_COMMON) -o $@ -lm

convert_10bit_video: convert_10bit_video.c $(LIB_COMMON)
	gcc $(CFLAGS) $(INCLUDE) $< $(LIB_COMMON) -o $@

create_video_test_signal: create_video_test_signal.c $(LIB_COMMON)
	gcc $(CFLAGS) $(INCLUDE) $< $(LIB_COMMON) -o $@

compare_archive_mxf: compare_archive_mxf.c $(LIB_COMMON)
	gcc $(CFLAGS) $(INCLUDE) $< $(LIB_COMMON) -o $@

clapperboard_avsync: clapperboard_avsync.c $(LIB_COMMON)
	gcc $(CFLAGS) -I. $(INCLUDE) $< $(LIB_COMMON) $(MXF_LIB) -o $@ -lm

send_video: send_video.c $(LIB_COMMON)
	gcc $(CFLAGS) $(INCLUDE) $< $(LIB_COMMON) -o $@ -L../YUVlib -lYUVlib -lpthread

receive_video: receive_video.c $(LIB_COMMON)
	gcc $(CFLAGS) $(INCLUDE) $< $(LIB_COMMON) -o $@ -L../YUVlib -lYUVlib -lpthread -lm

clean:
	cd dvs_hardware && $(MAKE) $@
	rm -f $(PROGS)
