# AAFSDKINSTALL is path to installed AAF SDK includes and libs (i.e. after an SDK make install)
ifneq "$(MAKECMDGOALS)" "clean"
	ifndef AAFSDKINSTALL
$(error Please set the AAFSDKINSTALL environment variable to the location of your installed AAFSDK files)
	endif
endif	

DATABASELIB_DIR = ../../database/src
COMMONLIB_DIR = ../../../common
STUDIOCOMMONLIB_DIR = ../../common

CC = g++
CFLAGS = -g -Wall -Wundef -W -Wno-unused-parameter -Wno-non-virtual-dtor -Werror
INCLUDES = -I. -I$(DATABASELIB_DIR) -I$(COMMONLIB_DIR) -I$(STUDIOCOMMONLIB_DIR) -I$(AAFSDKINSTALL)/include

COMPILE = $(CC) $(CFLAGS) $(INCLUDES)
LD = $(CC) -ldl -rdynamic

SOURCES = $(wildcard *.cpp)
OBJECTS = $(subst .cpp,.o,$(SOURCES))
DEPENDENCIES = $(subst .cpp,.d,$(SOURCES))


.PHONY: all
all: create_aaf


$(DATABASELIB_DIR)/libprodautodb.a:
	@cd $(DATABASELIB_DIR) && $(MAKE) libprodautodb.a

$(STUDIOCOMMONLIB_DIR)/libstudiocommon.a:
	@cd $(STUDIOCOMMONLIB_DIR) && $(MAKE) libstudiocommon.a


create_aaf: $(DATABASELIB_DIR)/libprodautodb.a $(STUDIOCOMMONLIB_DIR)/libstudiocommon.a $(OBJECTS)
	$(LD) $(OBJECTS) -Xlinker -rpath -Xlinker . -L$(DATABASELIB_DIR) -L$(COMMONLIB_DIR) -L$(STUDIOCOMMONLIB_DIR) -L$(AAFSDKINSTALL)/lib/debug -lprodautodb -lcommon -lstudiocommon -laaflib -laafiid -lodbc++ -luuid -lpthread -lxerces-c -o $@

	

%.d : %.cpp
	@echo Generating dependency file for $<; \
	$(CC) $(CFLAGS) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.o: %.cpp
	$(COMPILE) -c $< -o $@	
	

.PHONY: clean
clean:
	@rm -f *.d *.o *.a *~ create_aaf
	
	

ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
endif	
	


