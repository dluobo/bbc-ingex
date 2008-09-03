# $Id: Makefile,v 1.3 2008/09/03 15:30:29 john_f Exp $
#
LIBMXF_DIR ?= ../../lib

CC = gcc -Wall -W -Werror -Wno-unused-parameter -g -O3 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
INCLUDES = -I. -I.. -I$(LIBMXF_DIR)/include -I../../common
LIBS = -L../../common -lencode -L../../../common/YUVlib -lYUVlib -L../../../common -lcommon -ljpeg -lpthread
AR = ar cr

.PHONY: all
all: yuv422_to_mjpeg yuv422_to_mjpeg_threaded

yuv422_to_mjpeg: yuv422_to_mjpeg.c
	$(CC) yuv422_to_mjpeg.c $(INCLUDES) $(LIBS) -o $@

yuv422_to_mjpeg_threaded: yuv422_to_mjpeg_threaded.c
	$(CC) yuv422_to_mjpeg_threaded.c $(INCLUDES) $(LIBS) -o $@

clean:
	rm -f yuv422_to_mjpeg yuv422_to_mjpeg_threaded
