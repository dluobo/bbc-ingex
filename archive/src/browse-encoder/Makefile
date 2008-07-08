TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk


SOURCES = BrowseEncoder.cpp

C_SOURCES = browse_encoder.c


INCLUDES += $(INGEX_COMMON_INC) $(LIBMXF_INC) $(LIBMXF_ARCHIVE_INC)



.PHONY: all
all: test_browse_encoder $(OBJECTS) $(C_OBJECTS)

test_browse_encoder: test_browse_encoder.c $(C_OBJECTS)
	$(C_COMPILE) test_browse_encoder.c $(C_OBJECTS) -o test_browse_encoder $(FFMPEG_LIB)


.PHONY: clean
clean: cmn-clean
	@rm -f test_browse_encoder


include $(TOPLEVEL)/rules.mk


