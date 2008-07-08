TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk


SOURCES = capture.cpp \
	test_capture.cpp

C_SOURCES = sv_dummy.c


BROWSE_ENCODER_OBJECTS = $(BROWSE_ENCODER_DIR)/.c_objs/browse_encoder.o \
	$(BROWSE_ENCODER_DIR)/.objs/BrowseEncoder.o

PSE_OBJECTS = $(PSE_DIR)/.objs/pse_report_template.o \
	$(PSE_DIR)/.objs/pse_simple.o \
	$(PSE_DIR)/.objs/PSEReport.o \


MXF_INC = $(LIBMXF_INC) $(LIBMXF_ARCHIVE_INC) $(LIBMXF_ARCHIVE_WRITE_INC) 
MXF_LIB = $(LIBMXF_ARCHIVE_WRITE_LIB) $(LIBMXF_LIB)

INCLUDES += $(BROWSE_ENCODER_INC) $(PSE_INC) $(INGEX_COMMON_INC) $(MXF_INC) $(DVS_INC) $(FPA_INC)

LIBS = $(INGEX_COMMON_LIB) $(MXF_LIB) $(FFMPEG_LIB)



.PHONY: all
all: $(OBJECTS) $(C_OBJECTS) test_capture dummy_capture

.PHONY: ingex-deps
ingex-deps: ingex-common ingex-libmxf


test_capture: pse-fpa pse browse-encoder $(OBJECTS) $(C_OBJECTS)
	$(COMPILE) -o test_capture $(OBJECTS) $(BROWSE_ENCODER_OBJECTS) $(PSE_OBJECTS) $(LIBS) -lncurses -lpthread -lrt $(FPA_LIB) $(DVS_LIB)

dummy_capture: pse-fpa pse browse-encoder $(OBJECTS) $(C_OBJECTS)
	$(COMPILE) -o dummy_capture $(OBJECTS) $(C_OBJECTS) $(BROWSE_ENCODER_OBJECTS) $(PSE_OBJECTS) $(LIBS) -lncurses -lpthread -lrt $(FPA_LIB)


.PHONY: clean
clean: cmn-clean
	@rm -f test_capture dummy_capture
	

include $(TOPLEVEL)/rules.mk


