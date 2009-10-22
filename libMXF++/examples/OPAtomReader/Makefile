TOPLEVEL = ../..
include $(TOPLEVEL)/vars.mk

COMMON_DIR = ../Common
INCLUDES += -I$(COMMON_DIR)
COMMON_OBJECTS = $(COMMON_DIR)/.objs/DynamicByteArray.o

SOURCES = OPAtomContentPackage.cpp \
	FrameOffsetIndexTable.cpp \
	RawEssenceParser.cpp \
	FixedSizeEssenceParser.cpp \
	VariableSizeEssenceParser.cpp \
	PCMEssenceParser.cpp \
	OPAtomClipReader.cpp \
	OPAtomTrackReader.cpp
OBJECTS = $(patsubst %.cpp,.objs/%.o,$(SOURCES))
DEPENDENCIES = $(patsubst %.cpp,.deps/%.d,$(SOURCES))


.PHONY: all
all: libOPAtomReader.a test_opatomreader

.PHONY: common
common:
	$(MAKE) -C $(COMMON_DIR) all


libOPAtomReader.a: $(OBJECTS) common
	$(AR) $@ $(OBJECTS) $(COMMON_OBJECTS)



.deps/%.d : %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) $(INCLUDES) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $(INCLUDES) $< -o $@



test_opatomreader: .objs/test_opatomreader.o libOPAtomReader.a
	$(COMPILE) .objs/test_opatomreader.o -L. -L$(LIBMXFPP_DIR)/libMXF++ -L$(LIBMXF_DIR)/lib -lOPAtomReader -lMXF++ -lMXF $(UUIDLIB) -o $@


.PHONY: clean
clean:
	@rm -f *~ *.a test_opatomreader
	@rm -Rf .objs
	@rm -Rf .deps


ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
-include .deps/test_opatomreader.d
endif
