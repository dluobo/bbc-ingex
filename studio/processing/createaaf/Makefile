# AAFSDKINSTALL is path to installed AAF SDK includes and libs (i.e. after an SDK make install)
ifneq "$(MAKECMDGOALS)" "clean"
ifneq "$(MAKECMDGOALS)" "install-web"
ifneq "$(MAKECMDGOALS)" "uninstall-web"
	ifndef AAFSDKINSTALL
$(error Please set the AAFSDKINSTALL environment variable to the location of your installed AAFSDK files)
	endif
endif	
endif	
endif	

DATABASELIB_DIR = ../../database/src
COMMONLIB_DIR = ../../../common
STUDIOCOMMONLIB_DIR = ../../common
INSTALL_WEB_DIR = /srv/ingex/createaaf

CC = g++
CFLAGS = -g -Wall -Wundef -W -Wno-unused-parameter -Wno-non-virtual-dtor -Werror
INCLUDES = -I. -I$(DATABASELIB_DIR) -I$(COMMONLIB_DIR) -I$(STUDIOCOMMONLIB_DIR) -I$(AAFSDKINSTALL)/include

COMPILE = $(CC) $(CFLAGS) $(INCLUDES)
LD = $(CC) -ldl -rdynamic

SOURCES = $(wildcard *.cpp)
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))


.PHONY: all
all: create_aaf


$(DATABASELIB_DIR)/libprodautodb.a:
	@cd $(DATABASELIB_DIR) && $(MAKE) libprodautodb.a

$(STUDIOCOMMONLIB_DIR)/libstudiocommon.a:
	@cd $(STUDIOCOMMONLIB_DIR) && $(MAKE) libstudiocommon.a


create_aaf: $(DATABASELIB_DIR)/libprodautodb.a $(STUDIOCOMMONLIB_DIR)/libstudiocommon.a $(OBJECTS)
	$(LD) $(OBJECTS) -Xlinker -rpath -Xlinker . -L$(DATABASELIB_DIR) -L$(COMMONLIB_DIR) -L$(STUDIOCOMMONLIB_DIR) -L$(AAFSDKINSTALL)/lib/debug -lprodautodb -lcommon -lstudiocommon -laaflib -laafiid -lpq -lpqxx -luuid -lpthread -lxerces-c -o $@

	
.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $(INCLUDES) $< -o $@


.PHONY: install-web
install-web: create_aaf
	cp create_aaf $(INSTALL_WEB_DIR)

.PHONY: uninstall-web
uninstall-web:
	@rm -f $(INSTALL_WEB_DIR)/create_aaf

.PHONY: clean
clean:
	@rm -f *.a *~ create_aaf
	@rm -Rf .objs
	@rm -Rf .deps
	
	

ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
endif	
	


