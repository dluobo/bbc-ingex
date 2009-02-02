
FFMPEG_INCLUDE_PATH := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg && echo $$f && break; test -e $$f/libavcodec && echo $$f && break; done)

CC = g++
CFLAGS = -Wall -W  -Wno-unused-parameter -g -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
INCLUDEDIRS = -I. -I$(FFMPEG_INCLUDE_PATH)
OPT = -O3 -mmmx -msse2

# Check for old-style FFMPEG header locations and if needed add -DFFMPEG_OLD_INCLUDE_PATHS
FFMPEG_OLDSTYLE_INCLUDE := $(shell for f in /usr/local/include /usr/include; do test -e $$f/ffmpeg/avcodec.h && echo $$f && break; done)
ifneq "$(FFMPEG_OLDSTYLE_INCLUDE)" ""
	CFLAGS += -DFFMPEG_OLD_INCLUDE_PATHS
endif

COMPILE = $(CC) $(CFLAGS) $(OPT) $(INCLUDES)
AR = ar cr

SOURCES = EncoderException.cpp Encoder.cpp DVEncoder.cpp
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))


.PHONY: all
all: libEncoder.a


libEncoder.a: $(OBJECTS)
	$(AR) $@ $(OBJECTS)
	


.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDEDIRS) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $(INCLUDEDIRS) $< -o $@	
	
	

.PHONY: clean
clean:
	@rm -f *~ *.a
	@rm -Rf .objs
	@rm -Rf .deps
	
	

ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
endif	
	




	

