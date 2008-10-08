PLAYER_INSTALL_PREFIX ?= /usr/local

COMMON_DIR = ../../common
LIBMXF_DIR = ../../libMXF
LIBMXF_ARCHIVE_DIR = $(LIBMXF_DIR)/examples/archive
LIBMXF_READER_DIR = $(LIBMXF_DIR)/examples/reader


ifneq "$(MAKECMDGOALS)" "clean"

DVS_PATH := $(shell for f in /usr/local /usr; do test -e $$f/include/dvs_fifo.h && echo $$f && break; done)
ifeq "$(DVS_PATH)" ""
	ifndef DVSSDK
		ifndef DVS_SDK_INSTALL
$(warning no DVS available - set the DVSSDK or DVS_SDK_INSTALL environment variable to the location of your installed DVS SDK version 3 or version 2 respectively)
		endif
	endif
endif

ifndef DISABLE_FFMPEG
	FFMPEG_INCLUDE_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg && echo $$f && break; test -e $$f/libavcodec && echo $$f && break; done)
	ifeq "$(FFMPEG_INCLUDE_PATH)" ""
$(warning no FFMPEG library found - DV stream connector not available)
	endif
endif	
	
ifndef DISABLE_HTTP_ACCESS
	SHTTPD_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/shttpd.h && echo $$f && break; done)
	ifeq "$(SHTTPD_PATH)" ""
$(warning no shttpd library found - http access not available)
	endif
endif	
	
endif	

ifndef DISABLE_SHARED_MEM_SOURCE
	TEST_SHM_PATH := $(shell for f in ../../studio/capture/nexus_control.h; do test -e $$f && echo $$f && break; done)
	ifeq "$(TEST_SHM_PATH)" ""
$(warning shared memory source disabled)
	endif
endif

ifndef DISABLE_UDP_SOURCE
	TEST_UDP_PATH := $(shell for f in ../../common/multicast_video.h; do test -e $$f && echo $$f && break; done)
	ifeq "$(TEST_UDP_PATH)" ""
$(warning udp source disabled)
	endif
endif

PORTAUDIO_INCLUDE_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/portaudio.h && echo $$f && break; done)
ifneq "$(PORTAUDIO_INCLUDE_PATH)" ""
HAVE_PORTAUDIO = 1
else
$(warning no PortAudio library found - No PC audio playback available)
endif

SDL_INCLUDE_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/SDL/SDL.h && echo $$f && break; done)
ifneq "$(SDL_INCLUDE_PATH)" ""
HAVE_SDL = 1
else
$(warning no SDL library found - SDL sink not available)
endif



# -Werror removed since current ffmpeg header give "AVFrac is deprecated"
# warnings when compiling dv_stream_connect.c
CFLAGS = -Wall -W -Wno-unused-parameter -D_THREAD_SAFE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
OPT = -O3 -mmmx -msse2

INCLUDEDIRS = -I. -I$(LIBMXF_DIR)/lib/include -I$(LIBMXF_ARCHIVE_DIR) -I$(LIBMXF_ARCHIVE_DIR)/info -I$(LIBMXF_READER_DIR) -I/usr/local/include -I/usr/include/freetype2 -I/usr/include/fontconfig 

LIBDIRS = -L. -L$(LIBMXF_DIR)/lib -L$(LIBMXF_ARCHIVE_DIR)/info -L$(LIBMXF_READER_DIR) -L/usr/X11R6/lib -L/usr/local/lib -L/usr/X11R6/lib64 -L/usr/local/lib64
LIBS = -lX11 -lXv -lpthread -lfreetype -lfontconfig -luuid -ld3mxfinfo -lMXFReader -lMXF
LIB_DEPS = $(LIBMXF_DIR)/lib/libMXF.a $(LIBMXF_ARCHIVE_DIR)/info/libd3mxfinfo.a $(LIBMXF_READER_DIR)/libMXFReader.a

ifdef DISABLE_QC_LTO_ACCESS
	CFLAGS += -DDISABLE_QC_LTO_ACCESS
endif

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
			# DVS SDK version 2		
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

	# Use the following alternative link line for ffmpeg rpm installed ffmpeg
	FFMPEG_RPM := $(shell rpm -q ffmpeg-h264-aac > /dev/null && echo YES)
	FFMPEG_DNXHD_RPM := $(shell rpm -q ffmpeg-DNxHD-h264-aac > /dev/null && echo YES)
	ifeq "$(FFMPEG_RPM)" "YES"
		LIBS += -lavformat -lavcodec -lavutil -lz -lbz2 -lmp3lame -lx264 -la52 -lfaac -lfaad -lm
	else
		ifeq "$(FFMPEG_DNXHD_RPM)" "YES"
		LIBS += -lavformat -lavcodec -lavutil -lz -lbz2 -lmp3lame -lx264 -la52 -lfaac -lfaad -lm
		CFLAGS += -DHAVE_DNXHD
		else
		LIBS += -lavformat -lavcodec -lavutil -lm
		endif
	endif
else
	CFLAGS += -Werror
endif

ifdef SHTTPD_PATH
	INCLUDEDIRS += -I$(SHTTPD_PATH)/include
	LIBDIRS += -L$(SHTTPD_PATH)/lib
	LIBS += -lshttpd
	CFLAGS += -DHAVE_SHTTPD
else 
	ifdef SHTTPD_INSTALL
		INCLUDEDIRS += -I$(SHTTPD_INSTALL)/src
		LIBDIRS += -L$(SHTTPD_INSTALL)/src
		LIBS += -lshttpd
		CFLAGS += -DHAVE_SHTTPD
	endif
endif

ifndef TEST_SHM_PATH
	CFLAGS += -DDISABLE_SHARED_MEM_SOURCE
endif

ifndef TEST_UDP_PATH
	CFLAGS += -DDISABLE_UDP_SOURCE
else
	INCLUDEDIRS += -I../../common
	LIBDIRS += -L$(COMMON_DIR) -L$(COMMON_DIR)/YUVlib
	LIBS += -lcommon -lYUVlib
	LIB_DEPS += $(COMMON_DIR)/libcommon.a $(COMMON_DIR)/YUVlib/libYUVlib.a
endif

ifdef HAVE_PORTAUDIO
	CFLAGS += -DHAVE_PORTAUDIO -I$(PORTAUDIO_INCLUDE_PATH)
	LIBS += -lportaudio 
endif

ifdef HAVE_SDL
	CFLAGS += -DHAVE_SDL
	LIBS += -lSDL
endif


CC = gcc -g $(CFLAGS) $(OPT)
AR = ar cr


SOURCES = multiple_sources.c dvs_sink.c connection_matrix.c stream_connect.c utils.c \
	media_player.c term_keyboard_input.c mxf_source.c on_screen_display.c \
	shuttle_input.c emulate_key.c media_control.c shuttle_input_connect.c media_source.c \
	raw_file_source.c media_sink.c x11_xv_display_sink.c raw_file_sink.c console_monitor.c \
	YUV_frame.c YUV_text_overlay.c YUV_small_pic.c \
	video_switch_sink.c qc_session.c dv_stream_connect.c logging.c buffered_media_sink.c media_sink_frame.c \
	dual_sink.c null_sink.c frame_info.c buffered_media_source.c osd_symbols.c \
	x11_display_sink.c keyboard_input.c keyboard_input_connect.c x11_common.c \
	video_conversion.c system_timecode_source.c half_split_sink.c audio_level_sink.c \
	http_access.c http_access_resources.c qc_lto_access.c bouncing_ball_source.c \
	frame_sequence_sink.c qc_lto_extract.c mpegi_stream_connect.c mjpeg_stream_connect.c shared_mem_source.c \
	udp_source.c audio_sink.c blank_source.c progress_bar_input.c progress_bar_input_connect.c \
	clapper_source.c sdl_sink.c clip_source.c video_switch_database.c source_event.c \
	dnxhd_stream_connect.c qc_http_access.c mouse_input.c mouse_input_connect.c
OBJECTS = $(patsubst %.c,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.c,.deps/%.d,$(SOURCES))


.PHONY: all
all: libingexplayer.a player qc_player


$(LIBMXF_DIR)/lib/libMXF.a:
	cd $(LIBMXF_DIR)/lib && $(MAKE) libMXF.a

$(LIBMXF_ARCHIVE_DIR)/info/libd3mxfinfo.a:
	cd $(LIBMXF_ARCHIVE_DIR)/info && $(MAKE) libd3mxfinfo.a

$(LIBMXF_READER_DIR)/libMXFReader.a:
	cd $(LIBMXF_READER_DIR) && $(MAKE) libMXFReader.a

$(COMMON_DIR)/libcommon.a:
	cd $(COMMON_DIR) && $(MAKE) libcommon.a

$(COMMON_DIR)/YUVlib/libYUVlib.a:
	cd $(COMMON_DIR)/YUVlib && $(MAKE) all

		
		
libingexplayer.a: $(OBJECTS)
	$(AR) libingexplayer.a $(OBJECTS)
	
	
	
.deps/%.d : %.c
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(CC) $(INCLUDEDIRS) -M $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.c
	@mkdir -p .objs
	$(CC) -c $(INCLUDEDIRS) $< -o $@	
	

	
.deps/player.d : player.c
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(CC) $(INCLUDEDIRS) -M $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

player: .objs/player.o libingexplayer.a $(LIB_DEPS)
	$(CC) -c $(INCLUDEDIRS) version.c -o .objs/version.o
	$(CC) $(LIBDIRS) .objs/player.o .objs/version.o libingexplayer.a $(LIBS) -o $@

.deps/qc_player.d : qc_player.c
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(CC) $(INCLUDEDIRS) -M $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

qc_player: .objs/qc_player.o libingexplayer.a $(LIB_DEPS)
	$(CC) -c $(INCLUDEDIRS) version.c -o .objs/version.o
	$(CC) $(LIBDIRS) .objs/qc_player.o .objs/version.o libingexplayer.a $(LIBS) -o $@

	
	
TESTAPPS += test_shuttle_input	
test_shuttle_input: .objs/test_shuttle_input.o .objs/shuttle_input.o .objs/logging.o
	$(CC) $(INCLUDEDIRS) .objs/test_shuttle_input.o .objs/shuttle_input.o .objs/logging.o -o test_shuttle_input

TESTAPPS += test_emulate_key	
test_emulate_key: .objs/test_emulate_key.o .objs/emulate_key.o .objs/logging.o
	$(CC) $(INCLUDEDIRS) -L/usr/X11R6/lib -lX11 -L/usr/X11R6/lib64 .objs/test_emulate_key.o .objs/emulate_key.o .objs/logging.o -o test_emulate_key

TESTAPPS += test_media_control	
test_media_control: .objs/test_media_control.o .objs/media_control.o .objs/shuttle_input_connect.o .objs/shuttle_input.o .objs/x11_keyboard_input.o .objs/term_keyboard_input.o .objs/logging.o
	$(CC) $(INCLUDEDIRS) -L/usr/X11R6/lib -L/usr/X11R6/lib64 -lX11 .objs/test_media_control.o .objs/media_control.o .objs/shuttle_input_connect.o .objs/shuttle_input.o .objs/x11_keyboard_input.o .objs/term_keyboard_input.o .objs/logging.o -o test_media_control

TESTAPPS += ffmpeg_shuttle_emu	
ffmpeg_shuttle_emu: .objs/ffmpeg_shuttle_emu.o .objs/emulate_key.o .objs/shuttle_input.o .objs/logging.o
	$(CC) $(INCLUDEDIRS) -L/usr/X11R6/lib -L/usr/X11R6/lib64 -lX11 .objs/ffmpeg_shuttle_emu.o .objs/emulate_key.o .objs/shuttle_input.o .objs/logging.o -o ffmpeg_shuttle_emu

TESTAPPS += recorder_shuttle_emu	
recorder_shuttle_emu: .objs/recorder_shuttle_emu.o .objs/emulate_key.o .objs/shuttle_input.o .objs/logging.o .objs/utils.o 
	$(CC) $(INCLUDEDIRS) -L/usr/X11R6/lib -L/usr/X11R6/lib64 -lX11 .objs/recorder_shuttle_emu.o .objs/emulate_key.o .objs/shuttle_input.o .objs/logging.o .objs/utils.o -lm -lXtst -lpthread -o recorder_shuttle_emu

	

.PHONY: install
install: player qc_player
	cp player $(PLAYER_INSTALL_PREFIX)/bin
	cp qc_player $(PLAYER_INSTALL_PREFIX)/bin

.PHONY: uninstall
uninstall:
	@rm -f $(PLAYER_INSTALL_PREFIX)/bin/player
	@rm -f $(PLAYER_INSTALL_PREFIX)/bin/qc_player

	
	
.PHONY: clean
clean:
	@rm -f *~ *.a player qc_player $(TESTAPPS)
	@rm -Rf .objs
	@rm -Rf .deps
	

ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
-include .deps/player.d
-include .deps/qc_player.d
endif	
	


	
