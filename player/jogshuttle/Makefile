CC=g++
CFLAGS = -Wall -W -Wno-unused-parameter -O2
INCLUDES = -I.
LIBS = -lpthread

COMPILE = $(CC) -g $(CFLAGS)


SOURCES = JogShuttle.cpp test_JogShuttle.cpp
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))


.PHONY: all
all: test_JogShuttle


.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $(INCLUDES) $< -o $@


test_JogShuttle: $(OBJECTS)
	$(COMPILE) $(OBJECTS) $(LIBS) -o $@



.PHONY: clean
clean:
	@rm -f *~ *.a test_JogShuttle
	@rm -Rf .objs
	@rm -fR .deps
	

ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
endif	
	

	
