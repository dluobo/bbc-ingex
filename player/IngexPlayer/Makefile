C_INGEX_PLAYER_DIR = ../ingex_player
COMMON_DIR = ../../common
LIBMXF_DIR = ../../libMXF
LIBMXF_ARCHIVE_DIR = $(LIBMXF_DIR)/examples/archive
LIBMXF_READER_DIR = $(LIBMXF_DIR)/examples/reader


ifneq "$(MAKECMDGOALS)" "clean"

DVS_PATH := $(shell for f in /usr/local /usr; do test -e $$f/include/dvs_fifo.h && echo $$f && break; done)
ifneq "$(DVS_INCLUDE_PATH)" ""
	ifndef DVS_SDK_INSTALL
$(warning no DVS available - set the DVS_SDK_INSTALL environment variable to the location of your installed DVS SDK files)
	endif
endif

ifndef DISABLE_FFMPEG
	FFMPEG_INCLUDE_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg && echo $$f && break; test -e $$f/libavcodec && echo $$f && break; done)
	ifeq "$(FFMPEG_INCLUDE_PATH)" ""
$(warning no FFMPEG library found - DV stream connector not available)
	endif
endif	

PORTAUDIO_INCLUDE_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/portaudio.h && echo $$f && break; done)
ifneq "$(PORTAUDIO_INCLUDE_PATH)" ""
HAVE_PORTAUDIO = 1
else
$(warning: no PortAudio library found - No PC audio playback available)
endif


endif	


CFLAGS = -Wall -W -Wno-unused-parameter -D_THREAD_SAFE
OPT = -O3 -mmmx -msse2

INCLUDEDIRS = -I. -I$(LIBMXF_DIR)/lib/include -I$(LIBMXF_ARCHIVE_DIR) -I$(LIBMXF_ARCHIVE_DIR)/info -I$(LIBMXF_READER_DIR) -I$(C_INGEX_PLAYER_DIR) 

LIBDIRS = -L$(C_INGEX_PLAYER_DIR) -L$(LIBMXF_DIR)/lib -L$(LIBMXF_ARCHIVE_DIR)/info -L$(LIBMXF_READER_DIR) -L$(COMMON_DIR) -L$(COMMON_DIR)/YUVlib -L/usr/X11R6/lib -L/usr/local/lib
LIBS = -lX11 -lXv -lpthread -lfreetype -lfontconfig -luuid -lcommon -lYUVlib -ld3mxfinfo -lMXFReader -lMXF
 

ifdef DVS_PATH
	INCLUDEDIRS += -I$(DVS_PATH)/include
	LIBDIRS += -L$(DVS_PATH)/lib -L$(DVS_PATH)/lib64
	LIBS += -ldvsoem
	CFLAGS += -DHAVE_DVS
else 
	ifdef DVSSDK
		# DVS SDK version 3		
		INCLUDEDIRS += -I$(DVSSDK)/development/header
		LIBDIRS += -L$(DVSSDK)/linux-x86/lib -L$(DVSSDK)/linux-x86_64/lib
		LIBS += -ldvsoem
		CFLAGS += -DHAVE_DVS
	else
		ifdef DVS_SDK_INSTALL
			INCLUDEDIRS += -I$(DVS_SDK_INSTALL)/development/header
			LIBDIRS += -L$(DVS_SDK_INSTALL)/linux/lib
			LIBS += -ldvsoem
			CFLAGS += -DHAVE_DVS
		endif
	endif
endif


ifdef FFMPEG_INCLUDE_PATH

	CFLAGS += -DHAVE_FFMPEG

	# Check for old-style FFMPEG header locations and if needed add -DFFMPEG_OLD_INCLUDE_PATHS
	FFMPEG_OLDSTYLE_INCLUDE := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg/avcodec.h && echo $$f && break; done)
	ifneq "$(FFMPEG_OLDSTYLE_INCLUDE)" ""
		CFLAGS += -DFFMPEG_OLD_INCLUDE_PATHS
	endif

	# Check for swscale
	FFMPEG_SWSCALE_INCLUDE_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg/swscale.h && echo YES && break; test -e $$f/libswscale/swscale.h && echo YES && break; done)
	ifeq "$(FFMPEG_SWSCALE_INCLUDE_PATH)" "YES"
		CFLAGS += -DHAVE_FFMPEG_SWSCALE
	endif

	# Use the following alternative link line for ffmpeg rpm installed ffmpeg
	FFMPEG_RPM := $(shell rpm -q ffmpeg-h264-aac > /dev/null && echo YES)
	FFMPEG_DNXHD_RPM := $(shell rpm -q ffmpeg-DNxHD-h264-aac > /dev/null && echo YES)
	ifeq "$(FFMPEG_RPM)" "YES"
		LIBS += -lavformat -lavcodec -lavutil -lz -lbz2 -lmp3lame -lx264 -la52 -lfaac -lfaad -lm
	else
		ifeq "$(FFMPEG_DNXHD_RPM)" "YES"
			LIBS += -lavformat -lavcodec -lavutil -lz -lbz2 -lmp3lame -lx264 -lfaac -lfaad -lm
			CFLAGS += -DHAVE_DNXHD
		else
			LIBS += -lavformat -lavcodec -lavutil -lm
		endif
	endif
else
	CFLAGS += -Werror
endif

ifdef HAVE_PORTAUDIO
	CFLAGS += -DHAVE_PORTAUDIO -I$(PORTAUDIO_INCLUDE_PATH)
	LIBS += -lportaudio 
endif


CC = g++ -g $(CFLAGS) $(OPT)
AR_APPEND = ar r


SOURCES = IngexPlayerException.cpp LocalIngexPlayer.cpp
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))


.PHONY: all
all: libIngexPlayer.a test_IngexPlayer



$(C_INGEX_PLAYER_DIR)/libingexplayer.a:
	@cd $(C_INGEX_PLAYER_DIR) && $(MAKE) libingexplayer.a

	
libIngexPlayer.a: $(OBJECTS) $(C_INGEX_PLAYER_DIR)/libingexplayer.a
	cp $(C_INGEX_PLAYER_DIR)/libingexplayer.a libIngexPlayer.a
	$(AR_APPEND) libIngexPlayer.a $(OBJECTS) 

	
	
.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(CC) $(INCLUDEDIRS) -M $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(CC) -c $(INCLUDEDIRS) $< -o $@


.deps/test_IngexPlayer.d : test_IngexPlayer.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(CC) $(INCLUDEDIRS) -M $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

test_IngexPlayer: .objs/test_IngexPlayer.o libIngexPlayer.a
	$(CC) $(LIBDIRS) .objs/test_IngexPlayer.o libIngexPlayer.a $(LIBS) -o $@


.PHONY: clean
clean:
	@rm -f *~ *.a test_IngexPlayer
	@rm -Rf .objs
	@rm -fR .deps
	

ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
-include .deps/test_IngexPlayer.d
endif	
	

	
