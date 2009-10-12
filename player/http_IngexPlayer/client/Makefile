#
# $Id: Makefile,v 1.2 2009/10/12 16:06:27 philipn Exp $
#
# Copyright (C) 2008-2009 British Broadcasting Corporation, All Rights Reserved
# Author: Philip de Nier
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

INGEX_PLAYER_DIR = ../../IngexPlayer
C_INGEX_PLAYER_DIR = ../../ingex_player
SERVER_DIR = ..
JOGSHUTTLE_DIR = ../../jogshuttle


CFLAGS = -Wall -W -Wno-unused-parameter
OPT = -O3 -mmmx -msse2

INCLUDEDIRS = -I. -I$(INGEX_PLAYER_DIR) -I$(C_INGEX_PLAYER_DIR) -I$(JOGSHUTTLE_DIR) -I$(SERVER_DIR)

LIBDIRS =
LIBS = -lpthread -lcurl

CC = g++ -g $(CFLAGS) $(OPT)
AR = ar cr


SOURCES = HTTPPlayerClient.cpp
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))

SERVER_OBJECTS = $(SERVER_DIR)/.objs/JSONObject.o $(SERVER_DIR)/.objs/Utilities.o $(SERVER_DIR)/.objs/HTTPPlayerState.o

LIBDIRS += -L$(JOGSHUTTLE_DIR)
LIBS += -ljogshuttle



.PHONY: all
all: libHTTPPlayerClient.a test_HTTPPlayerClient

.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(CC) $(INCLUDEDIRS) -M $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(CC) -c $(INCLUDEDIRS) $< -o $@


$(SERVER_DIR)/.objs/%.o:
	cd $(SERVER_DIR) && $(MAKE) $(addprefix .objs/,$(notdir $@))


libHTTPPlayerClient.a: $(OBJECTS) $(SERVER_OBJECTS)
	$(AR) libHTTPPlayerClient.a $(OBJECTS) $(SERVER_OBJECTS)



test_HTTPPlayerClient: .objs/test_HTTPPlayerClient.o libHTTPPlayerClient.a
	$(CC) $(LIBDIRS) .objs/test_HTTPPlayerClient.o libHTTPPlayerClient.a $(LIBS) -o $@


.PHONY: clean
clean:
	@rm -f *~ *.a test_HTTPPlayerClient
	@rm -Rf .objs
	@rm -fR .deps


ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
-include .deps/test_HTTPPlayerClient.d
endif
