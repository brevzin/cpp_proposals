THIS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

OUTDIR := .
DEFAULTS := $(THIS_DIR)defaults.yaml
include $(THIS_DIR)wg21/Makefile

$(THIS_DIR)defaults.yaml : $(THIS_DIR)defaults.py
	$< > $@

%.html : $(DEPS)
	$(PANDOC) \
    --bibliography $(THIS_DIR)wg21_fmt.yaml \
    --bibliography $(DATADIR)/index.yaml
