TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk


SOURCES = D3MXFFile.cpp

BROWSE_ENCODER_OBJECTS = $(BROWSE_ENCODER_DIR)/.c_objs/browse_encoder.o

GENERAL_OBJECTS = $(GENERAL_DIR)/.objs/Logging.o \
	$(GENERAL_DIR)/.objs/RecorderException.o
	
MXF_INC = $(LIBMXF_INC) $(LIBMXF_ARCHIVE_INC) $(LIBMXF_ARCHIVE_WRITE_INC) $(LIBMXF_ARCHIVE_INFO_INC) $(LIBMXF_READER_INC)
MXF_LIB = $(LIBMXF_ARCHIVE_WRITE_LIB) $(LIBMXF_ARCHIVE_INFO_LIB) $(LIBMXF_READER_LIB) $(LIBMXF_LIB)

INCLUDES += $(INGEX_COMMON_INC) $(MXF_INC) $(GENERAL_INC) $(BROWSE_ENCODER_INC) $(VIDEO_CONV_INC)



.PHONY: all
all: generate_browse

.PHONY: ingex-deps
ingex-deps: ingex-common ingex-libmxf

	
generate_browse: general browse-encoder $(OBJECTS) .objs/main.o  
	$(COMPILE)  .objs/main.o $(OBJECTS) $(GENERAL_OBJECTS) $(BROWSE_ENCODER_OBJECTS) $(INGEX_COMMON_LIB) $(FFMPEG_LIB) $(MXF_LIB) -o $@
	

.PHONY: clean
clean: cmn-clean
	@rm -f generate_browse



include $(TOPLEVEL)/rules.mk



