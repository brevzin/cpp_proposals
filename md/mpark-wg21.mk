THIS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

OUTDIR := .
DEFAULTS := $(THIS_DIR)defaults.yaml
include $(THIS_DIR)wg21/Makefile

ifdef NO_BIBLIO
full_index :=
else
full_index := --bibliography $(DATADIR)/index.yaml
endif

#%.html : $(DEPS)
#	$(PANDOC) \
#    --bibliography $(THIS_DIR)wg21_fmt.yaml \
#	$(full_index)

$(OUTDIR)/p%.html $(OUTDIR)/d%.html : $(DEPS)
	$(PANDOC) --bibliography $(THIS_DIR)wg21_fmt.yaml -f markdown-tex_math_dollars
