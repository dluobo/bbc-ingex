LIBMXF_DIR ?= ../../libMXF
LIBMXFPP_DIR = ../libMXF++


# we turn of checking for unused parameters (-Wno-unused-parameter) because we have
# callbacks where the parameters are not always used
CC = g++
CFLAGS = -Wall -W  -Wno-unused-parameter -Werror -g -O2 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
INCLUDES = -I$(LIBMXF_DIR)/lib/include -I../

COMPILE = $(CC) $(CFLAGS) $(INCLUDES)
AR = ar cr

SOURCES = $(wildcard *.cpp)
OBJECTS = $(subst .cpp,.o,$(SOURCES))
DEPENDENCIES = $(subst .cpp,.d,$(SOURCES))


.PHONY: all
all: test


.PHONY: libMXF.a
libMXF.a:
	@cd $(LIBMXF_DIR)/lib && $(MAKE) $@

.PHONY: libMXF++.a
libMXF++.a:
	@cd $(LIBMXFPP_DIR) && $(MAKE) $@

	
test: libMXF.a libMXF++.a test.o
	$(COMPILE) test.o -L$(LIBMXF_DIR)/lib -L$(LIBMXFPP_DIR) -lMXF++ -lMXF -luuid -o $@


%.d : %.cpp
	@echo Generating dependency file for $<; \
	$(CC) $(CFLAGS) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.o: %.cpp
	$(COMPILE) -c $< -o $@	
	

.PHONY: clean
clean:
	@rm -f *.d *.o *.a *~ test
	
	
ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
endif	
