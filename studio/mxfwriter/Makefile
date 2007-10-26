DATABASELIB_DIR = ../database/src
LIBMXF_DIR = ../../libMXF
WRITEAVIDMXF_DIR = $(LIBMXF_DIR)/examples/writeavidmxf

CC = g++
CFLAGS = -g -Wall -Wundef -W -Wno-unused-parameter -Werror
INCLUDES = -I. -I$(DATABASELIB_DIR) -I$(LIBMXF_DIR)/lib/include -I$(WRITEAVIDMXF_DIR)

COMPILE = $(CC) $(CFLAGS) $(INCLUDES)
AR = ar cr

SOURCES = $(wildcard *.cpp)
OBJECTS = $(subst .cpp,.o,$(SOURCES))
DEPENDENCIES = $(subst .cpp,.d,$(SOURCES))

LIBOBJECTS = $(OBJECTS)
LIBOBJECTS -= test_mxfwriter.o


.PHONY: all
all: libmxfwriter.a test_mxfwriter


.PHONY: libMXF.a
libMXF.a:
	@cd $(LIBMXF_DIR)/lib && $(MAKE) $@

.PHONY: libwriteavidmxf.a
libwriteavidmxf.a:
	@cd $(WRITEAVIDMXF_DIR) && $(MAKE) $@

.PHONY: libprodautodb.a
libprodautodb.a:
	@cd $(DATABASELIB_DIR) && $(MAKE) $@


	
libmxfwriter.a: $(LIBOBJECTS)
	$(AR) $@ $(LIBOBJECTS)
	
	
test_mxfwriter: libprodautodb.a libwriteavidmxf.a libMXF.a libmxfwriter.a test_mxfwriter.o 
	$(CC) test_mxfwriter.o -L. -L$(DATABASELIB_DIR) -L$(WRITEAVIDMXF_DIR) -L$(LIBMXF_DIR)/lib -lprodautodb -lmxfwriter -lwriteavidmxf -lMXF -lodbc++ -luuid -lpthread -o $@

	
%.d : %.cpp
	@echo Generating dependency file for $<; \
	$(CC) $(CFLAGS) $(INCLUDES) -M $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.o: %.cpp
	$(COMPILE) -c $< -o $@	
	

.PHONY: clean
clean:
	@rm -f *.d *.o *.a *~ test_mxfwriter
	
	

ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
endif	
	

