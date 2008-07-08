# NOTES:
#
# Set the (optional) BBC_ARCHIVE_DIR environment variable to the directory containing the BBC stuff, including the PSE module
# Set the (optional) USE_SV_DUMMY environment variable to use the dummy capture when testing without a DVS card
# Set the (optional) DVS_DIR environment variable if the DVS SDK is not in $(HOME)/sdk2.7p57
# Edit the FFMPEG_LIB variable to match the library dependencies of the installed FFMPEG library



# TOPLEVEL must be set by any Makefile which includes this file
ifndef TOPLEVEL
  $(error TOPLEVEL variable not defined)
endif



# directories. 

INGEX_DIR = $(TOPLEVEL)/../..

INGEX_COMMON_DIR = $(INGEX_DIR)/common

INGEX_PLAYER_DIR = $(INGEX_DIR)/player/ingex_player

LIBMXF_DIR = $(INGEX_DIR)/libMXF
LIBMXF_LIB_DIR = $(LIBMXF_DIR)/lib
LIBMXF_ARCHIVE_DIR = $(LIBMXF_DIR)/examples/archive
LIBMXF_READER_DIR = $(LIBMXF_DIR)/examples/reader

ARCHIVE_DIR = $(TOPLEVEL)
GENERAL_DIR = $(ARCHIVE_DIR)/general
DATABASE_DIR = $(ARCHIVE_DIR)/database
CACHE_DIR = $(ARCHIVE_DIR)/cache
HTTP_DIR = $(ARCHIVE_DIR)/http
BARCODE_SCANNER_DIR = $(ARCHIVE_DIR)/barcode-scanner
BROWSE_ENCODER_DIR = $(ARCHIVE_DIR)/browse-encoder
CAPTURE_DIR = $(ARCHIVE_DIR)/capture
TAPE_IO_DIR = $(ARCHIVE_DIR)/tape-io
VTR_DIR = $(ARCHIVE_DIR)/vtr
PSE_DIR = $(ARCHIVE_DIR)/pse

DVS_DIR ?= $(HOME)/sdk2.7p57

BBC_ARCHIVE_DIR ?=



# Variables for compilation

CC = g++
C_CC = gcc
INCLUDES = -I.
CFLAGS = -Wall -W -Wno-unused-parameter -Werror -g -O2 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE $(INCLUDES)
AR = ar cr

COMPILE = $(CC) $(CFLAGS)
C_COMPILE = $(C_CC) $(CFLAGS)


# Check for old-style FFMPEG header locations and if needed add -DFFMPEG_OLD_INCLUDE_PATHS
FFMPEG_OLDSTYLE_INCLUDE := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg/avcodec.h && echo $$f && break; done)
ifneq "$(FFMPEG_OLDSTYLE_INCLUDE)" ""
	CFLAGS += -DFFMPEG_OLD_INCLUDE_PATHS
endif



# Objects and dependency files associated with $SOURCES and $C_SOURCES

OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))

C_OBJECTS = $(patsubst %.c,.c_objs/%.o,$(C_SOURCES))
C_DEPENDENCIES = $(patsubst %.c,.c_deps/%.d,$(C_SOURCES))



# includes

INGEX_COMMON_INC = -I$(INGEX_COMMON_DIR)

LIBMXF_INC = -I$(LIBMXF_LIB_DIR)/include
LIBMXF_ARCHIVE_INC = -I$(LIBMXF_ARCHIVE_DIR)
LIBMXF_ARCHIVE_WRITE_INC = -I$(LIBMXF_ARCHIVE_DIR)/write
LIBMXF_ARCHIVE_INFO_INC = -I$(LIBMXF_ARCHIVE_DIR)/info
LIBMXF_READER_INC = -I$(LIBMXF_READER_DIR)

GENERAL_INC=-I$(GENERAL_DIR)
CACHE_INC=-I$(CACHE_DIR)
DATABASE_INC=-I$(DATABASE_DIR)
HTTP_INC=-I$(HTTP_DIR)
BARCODE_SCANNER_INC=-I$(BARCODE_SCANNER_DIR)
BROWSE_ENCODER_INC=-I$(BROWSE_ENCODER_DIR)
TAPE_IO_INC=-I$(TAPE_IO_DIR)
CAPTURE_INC=-I$(CAPTURE_DIR)
VTR_INC=-I$(VTR_DIR)
PSE_INC=-I$(PSE_DIR)

DVS_INC = -I$(DVS_DIR)/development/header

ifdef BBC_ARCHIVE_DIR
	FPA_INC = -I$(BBC_ARCHIVE_DIR)/pse
else
	FPA_INC =
endif



# libraries 

INGEX_COMMON_LIB = -L$(INGEX_COMMON_DIR) -lcommon

LIBMXF_LIB = -L$(LIBMXF_LIB_DIR) -lMXF -luuid
LIBMXF_ARCHIVE_WRITE_LIB = -L$(LIBMXF_ARCHIVE_DIR)/write -lwritearchivemxf
LIBMXF_ARCHIVE_INFO_LIB = -L$(LIBMXF_ARCHIVE_DIR)/info -ld3mxfinfo
LIBMXF_READER_LIB = -L$(LIBMXF_READER_DIR) -lMXFReader

ifdef USE_SV_DUMMY
	DVS_LIB = $(CAPTURE_DIR)/.c_objs/sv_dummy.o
else
	DVS_LIB = -L$(DVS_DIR)/linux/lib -ldvsoem
endif

ifdef BBC_ARCHIVE_DIR
	FPA_LIB = -L$(BBC_ARCHIVE_DIR)/pse -lfpacore2
else
	FPA_LIB =
endif

# using the static PQXX library file because the channel servers do not have the 
# dynamic library installed
LIBPQXX_LIB = /usr/lib/libpqxx.a -lpq

SHTTPD_LIB = -lshttpd

# modify this line to match the FFMPEG installation
FFMPEG_LIB = -L/usr/local/lib -lavformat -lavcodec -lavutil -la52 -lmp3lame -lx264 -lfaac -lfaad -lpthread -lz -lm




