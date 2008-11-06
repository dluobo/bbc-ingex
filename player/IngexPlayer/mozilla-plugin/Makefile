CC=gcc
# _GNU_SOURCE turns on certain PTHREAD features like pthread_mutexattr_settype()
# MOZ_X11 turns on X11 specifics in npapi.h
# XP_UNIX turns on unix specifics in npapi.h
CFLAGS=-I. -Iinclude -I.. -I../../ingex_player -D_GNU_SOURCE -g -O2 -DXP_UNIX -DMOZ_X11 -Wall

.PHONY: all
all: ingex_plugin.so

plugin.o: plugin.c
	g++ $(CFLAGS) -c plugin.c

debug_x11.o: debug_x11.c
	g++ $(CFLAGS) -c debug_x11.c

npentry.o: npentry.c
	$(CC) $(CFLAGS) -c npentry.c

ingex_plugin.so: plugin.o npentry.o debug_x11.o
	g++ -shared plugin.o npentry.o debug_x11.o ../libIngexPlayer.a ../../../common/libcommon.a ../../../common/YUVlib/libYUVlib.a -lpthread -L/usr/lib -lresolv -lnsl -lrt /usr/lib/libX11.so -pthread -pthread -Wl,-soname -Wl,ingex_plugin.so -lX11 -lXv -lpthread -lfreetype -lfontconfig -luuid -ld3mxfinfo -lMXFReader -lMXF -lavformat -lavcodec -lavutil -lm -lportaudio -o ingex_plugin.so

.PHONY: clean
clean:
	rm -f ingex_plugin.so npentry.o plugin.o
