#
# $Id: Makefile,v 1.1 2008/10/08 10:16:06 john_f Exp $
#
# Makefile for building studio common functions
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

CC=gcc
OPT=-DNDEBUG -O3 -mmmx -msse2
INCLUDES=-I../../common
CFLAGS=-Wall -W -Wno-unused-parameter -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE $(OPT) $(INCLUDES)

FFMPEG_OLDSTYLE_INCLUDE := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg/avcodec.h && echo $$f && break; done)
ifneq "$(FFMPEG_OLDSTYLE_INCLUDE)" ""
	CFLAGS+=-DFFMPEG_OLD_INCLUDE_PATHS
endif

HEADERS=$(wildcard *.h)
CFILES=$(wildcard *.c)
CPPFILES=$(wildcard *.cpp)

OBJECTS=$(CFILES:%.c=%.o) $(CPPFILES:%.cpp=%.o)

LIBSTUDIOCOMMON=libstudiocommon.a

.PHONY: all
all: $(LIBSTUDIOCOMMON)

$(LIBSTUDIOCOMMON): $(OBJECTS)
	ar rcv $(LIBSTUDIOCOMMON) $(OBJECTS)

.PHONY: clean
clean:
	rm -f $(LIBSTUDIOCOMMON) $(OBJECTS)


