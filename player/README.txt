INGEX PLAYER
------------



The C code is in ingex_player directory. The CPP code is in IngexPlayer directory. 
The CPP code wraps the C code in a class called IngexPlayer.

etc. etc.



Build requirements:

libMXF, libMXFReader, libD3MXFInfo, FFMPEG, DVS SDK, FreeType



1) libMXF and libMXFReader

Checkout the latest libMXF version from the AP CVS. 

Install libMXF to /usr/local:
> cd ingex/libMXF/lib
> sudo make install

Install libMXFReader to /usr/local:
> cd ingex/libMXF/examples/reader
> sudo make install

Install libD3MXFInfo to /usr/local:
> cd ingex/libMXF/examples/archive/info
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




Building the CPP library:

> cd ingex/IngexPlayer/IngexPlayer
> make



Testing the CPP library:

> ingex/IngexPlayer/IngexPlayer/test_IngexPlayer -m test_v1.mxf




Building the C library:

> cd ingex/IngexPlayer/ingex_player
> make



Testing the C library:

> ingex/IngexPlayer/ingex_player/player -h
> ...
or 
> make testapps
> ./test_...


