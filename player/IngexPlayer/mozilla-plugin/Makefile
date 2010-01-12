# $Id: Makefile,v 1.3 2010/01/12 16:50:23 john_f Exp $
#

# dvs_sdk_paths.mk searches for sdk and sets DVS_HARDWARE_INCLUDE and DVS_HARDWARE_LIB
# search is overridden by setting DVSSDK
include ../../../common/dvs_sdk_paths.mk

CC=gcc
# _GNU_SOURCE turns on certain PTHREAD features like pthread_mutexattr_settype()
# MOZ_X11 turns on X11 specifics in npapi.h
# XP_UNIX turns on unix specifics in npapi.h
CFLAGS=-I. -Iinclude -I.. -I../../ingex_player -D_GNU_SOURCE -g -O2 -DXP_UNIX -DMOZ_X11 -Wall
LIBS=../libIngexPlayer.a ../../../common/libcommon.a ../../../common/YUVlib/libYUVlib.a -lpthread \
	-L/usr/lib -lresolv -lnsl -lrt /usr/lib/libX11.so -Wl,-soname -Wl,ingex_plugin.so \
	-lX11 -lXv -lfreetype -lfontconfig -luuid \
	-lavformat -lavcodec -lavutil -lm -lportaudio \
	$(DVS_HARDWARE_LIB) \
	-L../../../libMXF/examples/archive/info -larchivemxfinfo \
	-L../../../libMXF/examples/reader -lMXFReader \
	-L../../../libMXF/lib -lMXF \
	-lswscale -lavformat -lavcodec -lavutil -lz -lmp3lame -lx264 -lbz2 -lfaac -lfaad -lm \
	-lshttpd

.PHONY: all
all: ingex_plugin.so

plugin.o: plugin.c
	g++ $(CFLAGS) -c plugin.c

debug_x11.o: debug_x11.c
	g++ $(CFLAGS) -c debug_x11.c

npentry.o: npentry.c
	$(CC) $(CFLAGS) -c npentry.c

ingex_plugin.so: plugin.o npentry.o debug_x11.o
	g++ -shared plugin.o npentry.o debug_x11.o $(LIBS) -o ingex_plugin.so

.PHONY: clean
clean:
	rm -f ingex_plugin.so npentry.o plugin.o
