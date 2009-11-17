INSTALL_DIR ?= /usr/local

INGEX_DIR = ../../..

LIBMXF_DIR = $(INGEX_DIR)/libMXF
LIBMXFPP_DIR = $(INGEX_DIR)/libMXF++
DATABASE_DIR = $(INGEX_DIR)/studio/database/src
INGEXMXFINFO_DIR = $(INGEX_DIR)/studio/mxfreader
MXFWRITER_DIR = $(INGEX_DIR)/studio/mxfwriter
OPATOMREADER_DIR = $(LIBMXFPP_DIR)/examples/OPAtomReader
LIBMXF++_COMMON_DIR = $(LIBMXFPP_DIR)/examples/Common


CC = g++
CFLAGS = -g -Wall -Wundef -W -Werror  -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
COMPILE = $(CC) $(CFLAGS)

INCLUDES = -I. \
		   -I$(LIBMXFPP_DIR) \
		   -I$(LIBMXF_DIR)/lib/include \
		   -I$(DATABASE_DIR) \
		   -I$(INGEXMXFINFO_DIR) \
		   -I$(MXFWRITER_DIR) \
		   -I$(OPATOMREADER_DIR) \
		   -I$(LIBMXF++_COMMON_DIR)
LIB_DIRS = -L$(LIBMXFPP_DIR)/libMXF++ \
		   -L$(LIBMXF_DIR)/lib \
		   -L$(LIBMXF_DIR)/examples/writeavidmxf \
		   -L$(DATABASE_DIR) \
		   -L$(INGEXMXFINFO_DIR) \
		   -L$(MXFWRITER_DIR) \
		   -L$(OPATOMREADER_DIR) 
LIBS = -lOPAtomReader -lingexmxfinfo -lmxfwriter -lprodautodb -lpq -lpqxx -lMXF++ -lwriteavidmxf -lMXF -luuid -lpthread


SOURCES = $(wildcard *.cpp)
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))


.PHONY: all
all: recover_mxf


.PHONY: libMXF.a
libMXF.a:
	@cd $(LIBMXF_DIR)/lib && $(MAKE) $@

.PHONY: libMXF++.a
libMXF++.a:
	@cd $(LIBMXFPP_DIR)/libMXF++ && $(MAKE) $@

.PHONY: libprodautodb.a
libprodautodb.a:
	@cd $(DATABASE_DIR) && $(MAKE) $@

.PHONY: libingexmxfinfo.a
libingexmxfinfo.a:
	@cd $(INGEXMXFINFO_DIR) && $(MAKE) $@

.PHONY: libmxfwriter.a
libmxfwriter.a:
	@cd $(MXFWRITER_DIR) && $(MAKE) $@

.PHONY: libOPAtomReader.a
libOPAtomReader.a:
	@cd $(OPATOMREADER_DIR) && $(MAKE) $@


.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $(INCLUDES) $< -o $@


recover_mxf: libMXF.a libMXF++.a libprodautodb.a libingexmxfinfo.a libmxfwriter.a libOPAtomReader.a $(OBJECTS)
	$(COMPILE) $(OBJECTS) $(LIB_DIRS) $(LIBS) -o $@



.PHONY: install
install: recover_mxf
	cp recover_mxf $(INSTALL_DIR)/bin

.PHONY: uninstall
uninstall:
	@rm -f $(INSTALL_DIR)/bin/recover_mxf


.PHONY: clean
clean:
	@rm -f *.a *~ recover_mxf
	@rm -Rf .objs
	@rm -Rf .deps


ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
endif	
	

