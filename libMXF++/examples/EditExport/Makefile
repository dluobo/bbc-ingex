LIBMXF_DIR = ../../../libMXF
LIBMXFPP_DIR = ../..
COMMON_DIR = ../Common
ARCHIVEMXFREADER_DIR = ../ArchiveMXFReader
TRANSCODE_DIR = ../Transcode
AVIDCLIPWRITER_DIR = ../AvidClipWriter

CC = g++
CFLAGS = -Wall -W  -Wno-unused-parameter -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
INCLUDEDIRS = -I. -I$(COMMON_DIR) -I$(LIBMXF_DIR)/lib/include -I$(LIBMXFPP_DIR) -I$(ARCHIVEMXFREADER_DIR) -I$(TRANSCODE_DIR) -I$(AVIDCLIPWRITER_DIR)
OPTS = -O3 -mmmx -msse2

COMPILE = $(CC) $(CFLAGS) $(OPTS) $(INCLUDES)

SOURCES = edit_export.cpp
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))


COMMON_OBJECTS = $(COMMON_DIR)/.objs/DynamicByteArray.o
ARCHIVEMXFREADER_OBJECTS = $(ARCHIVEMXFREADER_DIR)/.objs/ArchiveMXFReader.o \
	$(ARCHIVEMXFREADER_DIR)/.objs/ArchiveMXFContentPackage.o
TRANSCODE_OBJECTS = $(TRANSCODE_DIR)/.objs/EncoderException.o \
	$(TRANSCODE_DIR)/.objs/Encoder.o \
	$(TRANSCODE_DIR)/.objs/DVEncoder.o
AVIDCLIPWRITER_OBJECTS = $(AVIDCLIPWRITER_DIR)/.objs/AvidClipWriter.o


FFMPEG_RPM := $(shell rpm -q ffmpeg-h264-aac > /dev/null && echo YES)
FFMPEG_DNXHD_RPM := $(shell rpm -q ffmpeg-DNxHD-h264-aac > /dev/null && echo YES)
ifeq "$(FFMPEG_RPM)" "YES"
	FFMPEG_LIBS += -lavformat -lavcodec -lavutil -lz -lbz2 -lmp3lame -lx264 -la52 -lfaac -lfaad -lm
else ifeq "$(FFMPEG_DNXHD_RPM)" "YES"
	FFMPEG_LIBS += -lavformat -lavcodec -lavutil -lz -lbz2 -lmp3lame -lx264 -lfaac -lfaad -lm
else
	FFMPEG_LIBS += -lavformat -lavcodec -lavutil -lm
endif

# Check for old-style FFMPEG header locations and if needed add -DFFMPEG_OLD_INCLUDE_PATHS
FFMPEG_OLDSTYLE_INCLUDE := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg/avcodec.h && echo $$f && break; done)
ifneq "$(FFMPEG_OLDSTYLE_INCLUDE)" ""
	CFLAGS += -DFFMPEG_OLD_INCLUDE_PATHS
endif


.PHONY: all
all: edit_export

.PHONY: Common
Common:
	@cd $(COMMON_DIR) && $(MAKE) all
	
.PHONY: ArchiveMXFReader
ArchiveMXFReader:
	@cd $(ARCHIVEMXFREADER_DIR) && $(MAKE) all
	
.PHONY: Transcode
Transcode:
	@cd $(TRANSCODE_DIR) && $(MAKE) all
	
.PHONY: AvidClipWriter
AvidClipWriter:
	@cd $(AVIDCLIPWRITER_DIR) && $(MAKE) all
	



.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDEDIRS) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $(INCLUDEDIRS) $< -o $@	
	
	

edit_export: Common ArchiveMXFReader Transcode AvidClipWriter .objs/edit_export.o
	$(COMPILE) .objs/edit_export.o -L. -L$(LIBMXFPP_DIR)/libMXF++ -L$(LIBMXF_DIR)/lib $(COMMON_OBJECTS) $(ARCHIVEMXFREADER_OBJECTS) $(TRANSCODE_OBJECTS) $(AVIDCLIPWRITER_OBJECTS) $(FFMPEG_LIBS) -lMXF++ -lMXF -luuid -o $@

	
.PHONY: clean
clean:
	@rm -f *~ *.a edit_export
	@rm -Rf .objs
	@rm -Rf .deps
	
	

ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
endif	
	




	

