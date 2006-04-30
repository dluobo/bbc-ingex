#
# $Id: Makefile,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
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
OPT=-O3 -mmmx -msse2
FLAGS=$(CFLAGS) $(OPT)
HARDWARE_INCLUDE=-I /home/stuartc/sdk2.7p33/development/header
HARDWARE_LIB=-L /home/stuartc/sdk2.7p33/linux/lib

PROGS=dvs_sdi nexus_save nexus_xv testgen audio_mixdown

all: $(PROGS)

dvs_sdi: hardware/dvs_sdi.c nexus_control.h utils.o
	g++ $(FLAGS) $(HARDWARE_INCLUDE) hardware/dvs_sdi.c utils.o $(HARDWARE_LIB) -ldvsoem -ldvsfile -lpthread -o dvs_sdi

testgen: testgen.c nexus_control.h
	g++ $(FLAGS) testgen.c -lpthread -o testgen

librecord.a: nexus_save.c nexus_control.h dvd_encoder.h AudioMixer.h audio_utils.h recorder.o dvd_encoder.o AudioMixer.o audio_utils.o quad_split_I420.o utils.o
	ar rv librecord.a recorder.o dvd_encoder.o AudioMixer.o audio_utils.o quad_split_I420.o utils.o

nexus_save: nexus_save.c nexus_control.h dvd_encoder.h AudioMixer.h audio_utils.h librecord.a
	g++ $(FLAGS) nexus_save.c -o nexus_save -L. -lrecord -lavformat -lavutil -lavcodec -lmp3lame -lz -lm -lpthread

recorder.o: recorder.c recorder.h
	g++ $(FLAGS) recorder.c -c

dvd_encoder.o: dvd_encoder.c dvd_encoder.h
	gcc $(FLAGS) -I/usr/local/include/ffmpeg dvd_encoder.c -c

nexus_xv: nexus_xv.c nexus_control.h
	g++ $(FLAGS) nexus_xv.c -o nexus_xv -L/usr/X11R6/lib -lXv

audio_mixdown: AudioMixer.o audio_utils.o audio_mixdown.c
	g++ $(FLAGS) audio_mixdown.c -o audio_mixdown AudioMixer.o audio_utils.o

fixwav: audio_utils.o fixwav.c
	g++ $(FLAGS) fixwav.c -o fixwav audio_utils.o

AudioMixer.o: AudioMixer.cpp AudioMixer.h
	g++ $(FLAGS) -c AudioMixer.cpp

audio_utils.o: audio_utils.c audio_utils.h
	g++ $(FLAGS) -c audio_utils.c

quad_split_I420.o: quad_split_I420.c quad_split_I420.h
	g++ $(FLAGS) -c quad_split_I420.c

utils.o: utils.c utils.h
	g++ $(FLAGS) -c utils.c

clean:
	rm -f $(PROGS) dvd_encoder.o AudioMixer.o audio_utils.o quad_split_I420.o utils.o librecord.a
