INSTALL_DIR ?= /usr/local

DATABASELIB_DIR = ../../database/src
STUDIOCOMMONLIB_DIR = ../../common

CC = g++
CFLAGS = -g -Wall -Wundef -W -Wno-unused-parameter -Wno-non-virtual-dtor -Werror
COMPILE = $(CC) $(CFLAGS)

INCLUDES = -I. -I$(DATABASELIB_DIR) -I$(STUDIOCOMMONLIB_DIR) -I$(AAFSDKINSTALL)/include
LIB_DIRS = -L$(DATABASELIB_DIR) -L$(STUDIOCOMMONLIB_DIR)
LIBS = -lprodautodb -lstudiocommon -lpq -lpqxx -luuid -lpthread -lxerces-c


SOURCES = $(wildcard *.cpp)
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))


.PHONY: all
all: import_cuts


$(DATABASELIB_DIR)/libprodautodb.a:
	@cd $(DATABASELIB_DIR) && $(MAKE) libprodautodb.a

$(STUDIOCOMMONLIB_DIR)/libstudiocommon.a:
	@cd $(STUDIOCOMMONLIB_DIR) && $(MAKE) libstudiocommon.a


.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $(INCLUDES) $< -o $@


import_cuts: $(DATABASELIB_DIR)/libprodautodb.a $(STUDIOCOMMONLIB_DIR)/libstudiocommon.a $(OBJECTS)
	$(COMPILE) $(OBJECTS)  $(LIB_DIRS) $(LIBS) -o $@



.PHONY: install
install: import_cuts
	cp import_cuts $(INSTALL_DIR)/bin

.PHONY: uninstall
uninstall:
	@rm -f $(INSTALL_DIR)/bin/import_cuts


.PHONY: clean
clean:
	@rm -f *.a *~ import_cuts
	@rm -Rf .objs
	@rm -Rf .deps


ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
endif	
	

