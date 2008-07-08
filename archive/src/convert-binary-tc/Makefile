TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk


C_SOURCES = convert_binary_tc.c


.PHONY: all
all: $(C_OBJECTS) convert_binary_tc


convert_binary_tc: $(C_OBJECTS)
	$(C_COMPILE) $(C_OBJECTS) -o $@


.PHONY: clean
clean: cmn-clean
	@rm -f convert_binary_tc
	

include $(TOPLEVEL)/rules.mk


