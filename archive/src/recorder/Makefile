TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk


SOURCES = Recorder.cpp \
	RecordingSession.cpp \
	RecordingItems.cpp \
	HTTPRecorder.cpp \
	ConfidenceReplay.cpp \
	PlayerState.cpp \
	Chunking.cpp \
	D3MXFFile.cpp 

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
	
VTR_OBJECTS = $(VTR_DIR)/.objs/SerialPort.o \
	$(VTR_DIR)/.objs/SonyProtocol.o \
	$(VTR_DIR)/.objs/VTRControl.o \
	$(VTR_DIR)/.objs/HTTPVTRControl.o \
	$(VTR_DIR)/.objs/DummyVTRControl.o

CAPTURE_OBJECTS = $(CAPTURE_DIR)/.objs/capture.o
	
BROWSE_ENCODER_OBJECTS = $(BROWSE_ENCODER_DIR)/.objs/BrowseEncoder.o \
	$(BROWSE_ENCODER_DIR)/.c_objs/browse_encoder.o

PSE_OBJECTS = $(PSE_DIR)/.objs/pse_report_template.o \
	$(PSE_DIR)/.objs/pse_simple.o \
	$(PSE_DIR)/.objs/PSEReport.o \

	
MXF_INC = $(LIBMXF_INC) $(LIBMXF_ARCHIVE_INC) $(LIBMXF_ARCHIVE_WRITE_INC) $(LIBMXF_ARCHIVE_INFO_INC) $(LIBMXF_READER_INC)
MXF_LIB = $(LIBMXF_ARCHIVE_WRITE_LIB) $(LIBMXF_ARCHIVE_INFO_LIB) $(LIBMXF_READER_LIB) $(LIBMXF_LIB)

INCLUDES += $(GENERAL_INC) $(BARCODE_SCANNER_INC) $(DATABASE_INC) $(CACHE_INC) $(HTTP_INC) $(VTR_INC) $(BROWSE_ENCODER_INC) $(PSE_INC) $(CAPTURE_INC) $(MXF_INC) $(DVS_INC) $(FPA_INC) $(INGEX_COMMON_INC)


.PHONY: all
all: ingex_recorder

.PHONY: ingex-deps
ingex-deps: ingex-common ingex-libmxf



ingex_recorder: general barcode-scanner database http cache vtr capture browse-encoder $(OBJECTS) .objs/main.o  
	$(COMPILE) .objs/main.o $(GENERAL_OBJECTS) $(BARCODE_SCANNER_OBJECTS) $(DATABASE_OBJECTS) $(CACHE_OBJECTS) $(HTTP_OBJECTS) $(VTR_OBJECTS) $(OBJECTS) $(CAPTURE_OBJECTS) $(BROWSE_ENCODER_OBJECTS) $(PSE_OBJECTS) $(INGEX_COMMON_LIB) $(DVS_LIB) $(MXF_LIB) $(FPA_LIB) $(FFMPEG_LIB) $(SHTTPD_LIB) $(LIBPQXX_LIB) -lcurl -lpthread -lrt -o $@


.PHONY: clean
clean: cmn-clean
	@rm -f ingex_recorder
	
	


include $(TOPLEVEL)/rules.mk


