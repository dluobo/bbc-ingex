TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk


SOURCES = tapeops.cpp \
	indexfile.cpp \
	tape.cpp \
	test_tape.cpp

MXF_INC = $(LIBMXF_INC) $(LIBMXF_ARCHIVE_INC) $(LIBMXF_ARCHIVE_WRITE_INC) 
MXF_LIB = $(LIBMXF_ARCHIVE_WRITE_LIB) $(LIBMXF_LIB)

INCLUDES += $(INGEX_COMMON_INC) $(MXF_INC)


.PHONY: all
all: $(OBJECTS) test_tape

.PHONY: ingex-deps
ingex-deps: ingex-common ingex-libmxf


test_tape: $(OBJECTS)
	$(COMPILE) -o test_tape $(OBJECTS) $(INGEX_COMMON_LIB) $(MXF_LIB) -lncurses -lpthread -lrt


.PHONY: clean
clean: cmn-clean
	@rm -f test_tape
	
	

include $(TOPLEVEL)/rules.mk



