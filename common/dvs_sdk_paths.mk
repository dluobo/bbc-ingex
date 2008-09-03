# $Id: dvs_sdk_paths.mk,v 1.3 2008/09/03 10:51:22 john_f Exp $
#
# Setup DVS_INCLUDE and DVS_LIB paths
#
# If DVSSDK environment variable is set, uses it to test for valid includes and lib.
#
# Otherwise searches $HOME/sdk2* and $HOME/sdk3* looking for the correct arch's inc and lib.
# The most recent sdk version with valid paths is selected.  sdk2* is handled differently
# to sdk3* since the layout of x86 and x86-64 libraries are different.

ifdef DVSSDK
  DVS_PATHS := $(shell dir=$(DVSSDK) ; test -r $$dir/development/header/dvs_clib.h || exit 1 ; result=$$dir ; if test `uname -m` = x86_64 ; then test -r $$result/linux/lib/libdvsoem.a && lib=$$result/linux/lib ; test -r $$result/linux-x86_64/lib/libdvsoem.a && lib=$$result/linux-x86_64/lib ; else test -r $$result/linux/lib/libdvsoem.a && lib=$$result/linux/lib ; test -r $$result/linux-x86/lib/libdvsoem.a && lib=$$result/linux-x86/lib ; fi ; echo $$result/development/header $$lib )
  ifeq "$(DVS_PATHS)" ""
    $(warning DVSSDK=$(DVSSDK) does not contain usable includes and libs)
  else
    $(info DVSSDK set and contains valid includes and libs, DVS_PATHS=$(DVS_PATHS))
	HARDWARE_INCLUDE=-I$(firstword $(DVS_PATHS))
	HARDWARE_LIB=-L$(lastword $(DVS_PATHS)) -ldvsoem
  endif
else
  DVS_PATHS := $(shell if test `uname -m` = x86_64 ; then listv2=$$HOME/sdk2.*x86_64 ; else listv2=`ls -d $$HOME/sdk2.* | grep -v x86_64` ; fi ; for dir in $$listv2 $$HOME/sdk3.* ; do test -r $$dir/development/header/dvs_clib.h && result=$$dir ; done ; if test `uname -m` = x86_64 ; then test -r $$result/linux/lib/libdvsoem.a && lib=$$result/linux/lib ; test -r $$result/linux-x86_64/lib/libdvsoem.a && lib=$$result/linux-x86_64/lib ; else test -r $$result/linux/lib/libdvsoem.a && lib=$$result/linux/lib ; test -r $$result/linux-x86/lib/libdvsoem.a && lib=$$result/linux-x86/lib ; fi ; test -z $$result && exit ; echo $$result/development/header $$lib )
  ifeq "$(DVS_PATHS)" ""
    $(warning DVSSDK detection failed)
  else
    $(info DVSSDK detection succeeded, DVS_PATHS=$(DVS_PATHS))
	HARDWARE_INCLUDE=-I$(firstword $(DVS_PATHS))
	HARDWARE_LIB=-L$(lastword $(DVS_PATHS)) -ldvsoem
  endif
endif

# If the dummy dvs library is requested use it instead of libdvsoem.a
ifdef DVSDUMMY
	TMP_INGEX_ROOT=$(shell echo $(CURDIR) | sed -e 's,/studio/.*,,' -e 's,/common/.*,,')
	HARDWARE_LIB=-L$(TMP_INGEX_ROOT)/common/tools/dvs_hardware -ldvsoem_dummy
    $(info CURDIR=$(CURDIR) TMP_ROOT=$(TMP_ROOT) HARDWARE_LIB=$(HARDWARE_LIB))
endif
