INGEX PLAYER
------------



The C code is in ingex_player directory. The CPP code is in IngexPlayer directory. 
The CPP code wraps the C code in a class called IngexPlayer.

etc. etc.



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



Testing the CPP library:

> ingex/player/IngexPlayer/test_IngexPlayer -m test_v1.mxf




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



