DATABASELIB_DIR = ../src

CC = g++
CFLAGS = -g -Wall -Wundef -W -Wno-unused-parameter -Werror
INCLUDES = -I. -I$(DATABASELIB_DIR)

COMPILE = $(CC) $(CFLAGS) $(INCLUDES)

SOURCES = $(wildcard *.cpp)
OBJECTS = $(subst .cpp,.o,$(SOURCES))
DEPENDENCIES = $(subst .cpp,.d,$(SOURCES))


.PHONY: all
all: test


$(DATABASELIB_DIR)/libprodautodb.a:
	@cd $(DATABASELIB_DIR) && $(MAKE) $@


test: $(DATABASELIB_DIR)/libprodautodb.a $(OBJECTS)
	$(CC) $(OBJECTS) -L$(DATABASELIB_DIR) -lprodautodb -lodbc++ -luuid -lpthread -o $@

	
%.d : %.cpp
	@echo Generating dependency file for $<; \
	$(CC) $(CFLAGS) $(INCLUDES) -M $< > $@.$$$$; \
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
	











