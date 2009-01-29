INGEX PLAYER
------------

The C code is in the ingex_player directory. The CPP code for the "local"
ingex player (direct playback to screen and/or SDI cards) is in the IngexPlayer
directory, and wraps the C code in a class called IngexPlayer.

The CPP code for the remote ingex player (controlled by HTTP requests
over a network) is in the http_IngexPlayer directory and uses IngexPlayer.
A CPP client class is in the client subdirectory.

Classes to interface to a ShuttlePRO jog/shuttle controller are in the
jogshuttle directory.

Build requirements:

libMXF, libMXFReader, libD3MXFInfo, FFMPEG, DVS SDK, FreeType, SDL



1) libMXF and libMXFReader

Install from YAST:
  libuuid1
  libuuid-devel

Checkout the latest libMXF version from the AP CVS. 

Install libMXF to /usr/local:
> cd ingex/libMXF/lib
> make
> sudo make install

Install libMXFReader to /usr/local:
> cd ingex/libMXF/examples/reader
> make
> sudo make install

Install libD3MXFInfo to /usr/local:
> cd ingex/libMXF/examples/archive/info
> make
> sudo make install



2) FFMPEG

Note: The player will still compile if you don't have FFMPEG. However, you will not be 
able to play DV files.



3) DVS SDK

Ask Philip or Stuart for the latest pre-built copy

Set the DVS_SDK_INSTALL environment variable or install in /usr/local.


Note: The player will still compile if you don't have the DVS SDK. However, you will not 
be able to playout over SDI.



4) Freetype font library

Install via YaST if not already present.



5) PortAudio library

For audio replay, install via YaST if not already present:
  portaudio
  portaudio-devel


6) SDL (Simple DirectMedia Layer Library)

Install SDL-devel vis YaST


Building the CPP library:

> cd ingex/player/IngexPlayer
> make


Testing the CPP library (an example - run without arguments for usage information:

> ingex/player/IngexPlayer/test_IngexPlayer --clapper


Building the C library and players:

> cd ingex/common
> make

> cd ingex/player/ingex_player
> make
> sudo make install


Testing the C library:

> ingex/player/ingex_player/player -h
> ...
or
> make testapps
> ./test_...


Building the HTTP player:

build the cpp library
> cd ingex/player/http_IngexPlayer
> make


Testing the HTTP player server:

> cd ingex/player/http_IngexPlayer
> ./http_player --doc-root .
point your web browser at http://localhost:9008/test.html
press "Test Start"


Building the HTTP player client
> cd ingex/player/http_IngexPlayer/client
> make


Testing the HTTP player client:

> cd ingex/player/http_IngexPlayer
> ./http_player &
> cd client
> ./test_HTTPPlayerClient --clapper
