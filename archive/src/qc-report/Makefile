TOPLEVEL = ..
include $(TOPLEVEL)/vars.mk

QC_REPORT_INSTALL_PREFIX = /usr/local


SOURCES = D3MXFFile.cpp \
	QCSessionFile.cpp \
	LTODirectory.cpp \
	QCReport.cpp \
	PSEReportWrapper.cpp \
	qc_report_template.cpp

GENERAL_OBJECTS = $(GENERAL_DIR)/.objs/Logging.o \
	$(GENERAL_DIR)/.objs/RecorderException.o

PSE_OBJECTS = $(PSE_DIR)/.objs/PSEReport.o \
	$(PSE_DIR)/.objs/pse_report_template.o
	
MXF_INC = $(LIBMXF_INC) $(LIBMXF_ARCHIVE_INC) $(LIBMXF_ARCHIVE_INFO_INC) $(LIBMXF_READER_INC)
MXF_LIB = $(LIBMXF_ARCHIVE_INFO_LIB) $(LIBMXF_READER_LIB) $(LIBMXF_LIB)

INCLUDES += $(GENERAL_INC) $(MXF_INC) $(PSE_INC)



.PHONY: all
all: qc_report

.PHONY: ingex-deps
ingex-deps: ingex-libmxf


	
qc_report_template.cpp: qc_report_template.html QCReport.cpp
	$(COMPILE) -DGEN_QC_REPORT_TEMPLATE=1 QCReport.cpp -o gen_qc_report_template
	./gen_qc_report_template qc_report_template.html $@
	

qc_report: general capture $(OBJECTS) .objs/main.o
	$(COMPILE) .objs/main.o $(OBJECTS) $(GENERAL_OBJECTS) $(PSE_OBJECTS) $(MXF_LIB) -o $@
	


.PHONY: install
install: qc_report
	cp qc_report $(QC_REPORT_INSTALL_PREFIX)/bin
	cp qc_delete_cache_dir.sh $(QC_REPORT_INSTALL_PREFIX)/bin
	cp qc_report_script.sh $(QC_REPORT_INSTALL_PREFIX)/bin

.PHONY: uninstall
uninstall:
	rm -f $(QC_REPORT_INSTALL_PREFIX)/bin/qc_report
	rm -f $(QC_REPORT_INSTALL_PREFIX)/bin/qc_delete_cache_dir.sh
	rm -f $(QC_REPORT_INSTALL_PREFIX)/bin/qc_report_script.sh

	
.PHONY: clean
clean: cmn-clean
	@rm -f qc_report gen_qc_report_template qc_report_template.cpp
	



include $(TOPLEVEL)/rules.mk



