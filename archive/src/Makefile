TOPLEVEL = .
include $(TOPLEVEL)/vars.mk

.PHONY: all
all:
	cd general && $(MAKE) $@
	cd browse-encoder && $(MAKE) $@
	cd generate-browse && $(MAKE) $@
	cd barcode-scanner && $(MAKE) $@
	cd cache && $(MAKE) $@
	cd database && $(MAKE) $@
	cd http && $(MAKE) $@
	cd vtr && $(MAKE) $@
	cd pse && $(MAKE) $@
	cd capture && $(MAKE) $@
	cd recorder && $(MAKE) $@
	cd tape-io && $(MAKE) $@
	cd tape-export && $(MAKE) $@
	cd qc-report && $(MAKE) $@

.PHONY: clean
clean: cmn-clean
	cd general && $(MAKE) $@
	cd browse-encoder && $(MAKE) $@
	cd generate-browse && $(MAKE) $@
	cd barcode-scanner && $(MAKE) $@
	cd cache && $(MAKE) $@
	cd database && $(MAKE) $@
	cd http && $(MAKE) $@
	cd vtr && $(MAKE) $@
	cd pse && $(MAKE) $@
	cd capture && $(MAKE) $@
	cd recorder && $(MAKE) $@
	cd tape-io && $(MAKE) $@
	cd tape-export && $(MAKE) $@
	cd qc-report && $(MAKE) $@



.PHONY: ingex-deps
ingex-deps: ingex-common ingex-libmxf ingex-player

	
.PHONY: clean-ingex-deps
clean-ingex-deps:
	cd $(LIBMXF_DIR) && $(MAKE) clean
	cd $(INGEX_COMMON_DIR) && $(MAKE) clean
	cd $(INGEX_PLAYER_DIR) && $(MAKE) clean



include $(TOPLEVEL)/rules.mk


