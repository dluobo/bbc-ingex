# dependency and object file targets. $(COMPILE) must be defined

.deps/%.d: %.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.objs/%.o: %.cpp
	@mkdir -p .objs
	$(COMPILE) -c $< -o $@	
	
.deps/main.d: main.cpp
	@echo Generating dependency file for $<; \
	mkdir -p .deps; \
	$(COMPILE) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.c_deps/%.d: %.c
	@echo Generating dependency file for $<; \
	mkdir -p .c_deps; \
	$(C_COMPILE) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\.c_objs/\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.c_objs/%.o: %.c
	@mkdir -p .c_objs
	$(C_COMPILE) -c $< -o $@	
	
	


# targets for directories / libraries

.PHONY: barcode-scanner
barcode-scanner:
	@cd $(BARCODE_SCANNER_DIR) && $(MAKE) all

.PHONY: browse-encoder
browse-encoder:
	@cd $(BROWSE_ENCODER_DIR) && $(MAKE) all

.PHONY: cache
cache:
	@cd $(CACHE_DIR) && $(MAKE) all

.PHONY: capture
capture:
	@cd $(CAPTURE_DIR) && $(MAKE) all

.PHONY: database
database:
	@cd $(DATABASE_DIR) && $(MAKE) all

.PHONY: general
general:
	@cd $(GENERAL_DIR) && $(MAKE) all

.PHONY: http
http:
	@cd $(HTTP_DIR) && $(MAKE) all

.PHONY: pse
pse:
	@cd $(PSE_DIR) && $(MAKE) all

.PHONY: tape-io
tape-io:
	@cd $(TAPE_IO_DIR) && $(MAKE) all

.PHONY: vtr
vtr:
	@cd $(VTR_DIR) && $(MAKE) all


.PHONY: pse-fpa
pse-fpa:
ifdef BBC_ARCHIVE_DIR
	@cd $(BBC_ARCHIVE_DIR)/pse && $(MAKE) pse_fpa.o
else
	$(warn "WARNING: BBC_ARCHIVE_DIR environment variable not set - using dummy PSE module")
endif

.PHONY: ingex-common
ingex-common:
	@cd $(INGEX_COMMON_DIR) && $(MAKE) libcommon.a

.PHONY: ingex-libmxf
ingex-libmxf:
	@cd $(LIBMXF_DIR) && $(MAKE) all

.PHONY: ingex-player
ingex-player:
	@cd $(INGEX_PLAYER_DIR) && $(MAKE) all



# common clean target

.PHONY: cmn-clean
cmn-clean:
	@rm -f *~ *.a
	@rm -Rf .objs .deps
	@rm -Rf .c_objs .c_deps
	

	
# include the dependency targets 

ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPENDENCIES)
-include $(C_DEPENDENCIES)
-include .deps/main.d
endif


