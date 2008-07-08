TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk


SOURCES = pse_simple.cpp \
	PSEReport.cpp \
	pse_report_template.cpp


MXF_INC = $(LIBMXF_INC) $(LIBMXF_ARCHIVE_INC) $(LIBMXF_ARCHIVE_WRITE_INC) 

INCLUDES += $(MXF_INC) $(FPA_INC)



.PHONY: all
all: $(OBJECTS)

.PHONY: ingex-deps
ingex-deps: ingex-common ingex-libmxf


pse_report_template.cpp : pse_report_template.html PSEReport.cpp
	g++ -DGEN_PSE_REPORT_TEMPLATE=1 PSEReport.cpp -o gen_pse_report_template
	./gen_pse_report_template pse_report_template.html $@

	
.PHONY: clean
clean: cmn-clean
	@rm -f gen_pse_report_template pse_report_template.cpp
	

include $(TOPLEVEL)/rules.mk


