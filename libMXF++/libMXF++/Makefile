#
# $Id: Makefile,v 1.3 2009/12/17 16:38:40 john_f Exp $
#
# Makefile for building libMXF++
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
TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk


SOURCES = $(wildcard *.cpp)
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))

MD_SOURCES = $(wildcard metadata/*.cpp)
MD_OBJECTS = $(patsubst metadata/%.cpp,metadata/.objs/%.o,$(MD_SOURCES))
MD_DEPENDENCIES = $(patsubst metadata/%.cpp,metadata/.deps/%.d,$(MD_SOURCES))

MD_BASE_SOURCES = $(wildcard metadata/base/*.cpp)
MD_BASE_OBJECTS = $(patsubst metadata/base/%.cpp,metadata/base/.objs/%.o,$(MD_BASE_SOURCES))
MD_BASE_DEPENDENCIES = $(patsubst metadata/base/%.cpp,metadata/base/.deps/%.d,$(MD_BASE_SOURCES))

EXT_SOURCES = $(wildcard extensions/*.cpp)
EXT_OBJECTS = $(patsubst extensions/%.cpp,extensions/.objs/%.o,$(EXT_SOURCES))
EXT_DEPENDENCIES = $(patsubst extensions/%.cpp,extensions/.deps/%.d,$(EXT_SOURCES))

ALL_OBJECTS = $(OBJECTS) $(MD_OBJECTS) $(MD_BASE_OBJECTS) $(EXT_OBJECTS)


.PHONY: all
all: libMXF++.a


.PHONY: libMXF.a
libMXF.a:
	@cd $(LIBMXF_DIR)/lib && $(MAKE) $@

libMXF++.a: $(ALL_OBJECTS)
	$(AR) $@ $(ALL_OBJECTS)
	
	
.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $(INCLUDES) $< -o $@	
	
	
metadata/.deps/%.d : metadata/%.cpp
	@echo Generating dependency file for $<; \
	mkdir -p metadata/.deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

metadata/.objs/%.o: metadata/%.cpp
	@mkdir -p metadata/.objs
	$(COMPILE) -c $(INCLUDES) $< -o $@	
	
	
metadata/base/.deps/%.d : metadata/base/%.cpp
	@echo Generating dependency file for $<; \
	mkdir -p metadata/base/.deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

metadata/base/.objs/%.o: metadata/base/%.cpp
	@mkdir -p metadata/base/.objs
	$(COMPILE) -c $(INCLUDES) $< -o $@	


extensions/.deps/%.d : extensions/%.cpp
	@echo Generating dependency file for $<; \
	mkdir -p extensions/.deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

extensions/.objs/%.o: extensions/%.cpp
	@mkdir -p extensions/.objs
	$(COMPILE) -c $(INCLUDES) $< -o $@	
	
	
.PHONY: install
install: libMXF++.a
	mkdir -p $(INSTALL_PREFIX)/lib
	cp libMXF++.a $(INSTALL_PREFIX)/lib/
	mkdir -p $(INSTALL_PREFIX)/include/libMXF++/metadata/base
	cp *.h $(INSTALL_PREFIX)/include/libMXF++/
	cp metadata/*.h $(INSTALL_PREFIX)/include/libMXF++/metadata/
	cp metadata/base/*.h $(INSTALL_PREFIX)/include/libMXF++/metadata/base/
	mkdir -p $(INSTALL_PREFIX)/include/libMXF++/extensions
	cp extensions/*.h $(INSTALL_PREFIX)/include/libMXF++/extensions/

.PHONY: uninstall
uninstall:
	rm -f $(INSTALL_PREFIX)/lib/libMXF++.a
	rm -rf $(INSTALL_PREFIX)/include/libMXF++


.PHONY: clean
clean:
	@rm -f *~ *.a
	@rm -Rf .objs
	@rm -Rf .deps
	@rm -f metadata/*~
	@rm -Rf metadata/.objs
	@rm -Rf metadata/.deps
	@rm -Rf metadata/base/.objs
	@rm -Rf metadata/base/.deps
	@rm -Rf extensions/.objs
	@rm -Rf extensions/.deps


ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
-include $(MD_DEPENDENCIES)
-include $(MD_BASE_DEPENDENCIES)
endif	
