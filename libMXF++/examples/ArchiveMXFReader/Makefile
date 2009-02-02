#
# $Id: Makefile,v 1.1 2009/02/02 05:14:33 stuart_hc Exp $
#
#
#
# Copyright (C) 2008  BBC Research, Philip de Nier <philipn@users.sourceforge.net>
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
TOPLEVEL = ../..
include $(TOPLEVEL)/vars.mk

COMMON_DIR = ../Common

INCLUDES += -I$(COMMON_DIR)

SOURCES = ArchiveMXFContentPackage.cpp ArchiveMXFReader.cpp
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))
COMMON_OBJECTS = $(COMMON_DIR)/.objs/DynamicByteArray.o


.PHONY: all
all: libArchiveMXFReader.a test_archivemxfreader

.PHONY: common
common:
	@cd $(COMMON_DIR) && $(MAKE) all


libArchiveMXFReader.a: $(OBJECTS) common
	$(AR) $@ $(OBJECTS) $(COMMON_OBJECTS)



.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $(INCLUDES) $< -o $@



test_archivemxfreader: .objs/test_archivemxfreader.o libArchiveMXFReader.a
	$(COMPILE) .objs/test_archivemxfreader.o -L. -L$(LIBMXFPP_DIR)/libMXF++ -L$(LIBMXF_DIR)/lib -lArchiveMXFReader -lMXF++ -lMXF $(UUIDLIB) -o $@


.PHONY: install
install: all
	mkdir -p $(INSTALL_PREFIX)/lib
	cp libArchiveMXFReader.a $(INSTALL_PREFIX)/lib/
	mkdir -p $(INSTALL_PREFIX)/include
	cp *.h $(INSTALL_PREFIX)/include/

.PHONY: clean
clean:
	@rm -f *~ *.a test_archivemxfreader
	@rm -Rf .objs
	@rm -Rf .deps


ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
-include .deps/test_archivemxfreader.d
endif
