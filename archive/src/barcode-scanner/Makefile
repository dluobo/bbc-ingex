TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk


SOURCES = BarcodeScanner.cpp \
	BarcodeScannerFIFO.cpp

GENERAL_OBJECTS=$(GENERAL_DIR)/.objs/Threads.o \
	$(GENERAL_DIR)/.objs/Logging.o \
	$(GENERAL_DIR)/.objs/RecorderException.o \
	$(GENERAL_DIR)/.objs/Utilities.o \
	$(GENERAL_DIR)/.objs/Timing.o


INCLUDES += $(GENERAL_INC)



.PHONY: all
all: $(OBJECTS) barcode_scanner


barcode_scanner: general $(OBJECTS) .objs/main.o  
	$(COMPILE) .objs/main.o $(OBJECTS) $(GENERAL_OBJECTS) -lpthread -lrt -o $@
	
	
.PHONY: clean
clean: cmn-clean
	@rm -f barcode_scanner
	
	
include $(TOPLEVEL)/rules.mk



