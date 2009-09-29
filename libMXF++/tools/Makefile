TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk

.PHONY: all
all: gen_metadata_classes
	
gen_metadata_classes: gen_metadata_classes.c
	$(BIN_PREFIX)gcc -Wall -W  -Wno-unused-parameter -g -O2 -I$(LIBMXF_DIR)/lib/include gen_metadata_classes.c -L$(LIBMXF_DIR)/lib -lMXF $(UUIDLIB) -o gen_metadata_classes
	
.PHONY: generate
generate: gen_metadata_classes
	./gen_metadata_classes generated
	
.PHONY: install
install: generate
	mkdir -p $(LIBMXFPP_DIR)/metadata/base
	cp -R generated/base/* $(LIBMXFPP_DIR)/metadata/base

.PHONY: firstinstall
firstinstall: generate
	mkdir -p $(LIBMXFPP_DIR)/metadata/base
	cp -R generated/* $(LIBMXFPP_DIR)/metadata
	cp Metadata.h $(LIBMXFPP_DIR)/metadata

	
.PHONY: clean
clean:
	@rm -Rf gen_metadata_classes generated *~
	