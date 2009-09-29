TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk

SOURCES = $(wildcard *.cpp)
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))


.PHONY: all
all: test


.PHONY: libMXF.a
libMXF.a:
	@cd $(LIBMXF_DIR)/lib && $(MAKE) $@

.PHONY: libMXF++.a
libMXF++.a:
	@cd $(LIBMXFPP_DIR)/libMXF++ && $(MAKE) $@

	
test: libMXF.a libMXF++.a .objs/test.o
	$(COMPILE) .objs/test.o -L$(LIBMXF_DIR)/lib -L$(LIBMXFPP_DIR)/libMXF++ -lMXF++ -lMXF $(UUIDLIB) -o $@


.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $(INCLUDES) $< -o $@

	

.PHONY: clean
clean:
	@rm -f *~ *.a test
	@rm -Rf .objs
	@rm -Rf .deps
	
	
ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
endif	
