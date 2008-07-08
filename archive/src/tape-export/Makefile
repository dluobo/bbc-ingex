TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk


SOURCES = TapeExport.cpp \
	HTTPTapeExport.cpp \
	TapeExportSession.cpp

GENERAL_OBJECTS=$(GENERAL_DIR)/.objs/Threads.o \
	$(GENERAL_DIR)/.objs/Logging.o \
	$(GENERAL_DIR)/.objs/RecorderException.o \
	$(GENERAL_DIR)/.objs/Utilities.o \
	$(GENERAL_DIR)/.objs/DirectoryWatch.o \
	$(GENERAL_DIR)/.objs/Config.o \
	$(GENERAL_DIR)/.objs/Timing.o \
	$(GENERAL_DIR)/.objs/FileLock.o

BARCODE_SCANNER_OBJECTS = $(BARCODE_SCANNER_DIR)/.objs/BarcodeScanner.o \
	$(BARCODE_SCANNER_DIR)/.objs/BarcodeScannerFIFO.o
	
DATABASE_OBJECTS = $(DATABASE_DIR)/.objs/PostgresDatabase.o \
	$(DATABASE_DIR)/.objs/DatabaseThings.o \
	$(DATABASE_DIR)/.objs/RecorderDatabase.o
	
CACHE_OBJECTS =	$(CACHE_DIR)/.objs/Cache.o

HTTP_OBJECTS = $(HTTP_DIR)/.objs/JSONObject.o \
	$(HTTP_DIR)/.objs/HTTPServer.o
	
TAPE_IO_OBJECTS = $(TAPE_IO_DIR)/.objs/tape.o \
	$(TAPE_IO_DIR)/.objs/tapeops.o \
	$(TAPE_IO_DIR)/.objs/indexfile.o


MXF_INC = $(LIBMXF_INC) $(LIBMXF_ARCHIVE_INC) $(LIBMXF_ARCHIVE_WRITE_INC)
MXF_LIB = $(LIBMXF_ARCHIVE_WRITE_LIB) $(LIBMXF_LIB)

INCLUDES += $(GENERAL_INC) $(BARCODE_SCANNER_INC) $(DATABASE_INC) $(CACHE_INC) $(HTTP_INC) $(TAPE_IO_INC) $(INGEX_COMMON_INC) $(MXF_INC)



.PHONY: all
all: ingex_tape_export

.PHONY: ingex-deps
ingex-deps: ingex-common ingex-libmxf


ingex_tape_export: general barcode-scanner http cache database tape-io $(OBJECTS) .objs/main.o  
	$(COMPILE) .objs/main.o $(GENERAL_OBJECTS) $(BARCODE_SCANNER_OBJECTS) $(DATABASE_OBJECTS) $(CACHE_OBJECTS) $(HTTP_OBJECTS) $(TAPE_IO_OBJECTS) $(OBJECTS) $(INGEX_COMMON_LIB) $(MXF_LIB) $(SHTTPD_LIB) $(LIBPQXX_LIB) -lpthread -lrt -o $@
	
	

.PHONY: clean
clean: cmn-clean
	@rm -f ingex_tape_export
	
	

include $(TOPLEVEL)/rules.mk




