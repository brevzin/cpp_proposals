THIS_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
MPARK_DATA := $(THIS_DIR)wg21/data

PANDOC = pandoc $< $(MPARK_DATA)/references.md \
   --number-sections \
   --self-contained \
   --table-of-contents \
   --bibliography $(THIS_DIR)wg21_fmt.yaml \
   --bibliography $(MPARK_DATA)/index.yaml \
   --csl $(MPARK_DATA)/cpp.csl \
   --css $(MPARK_DATA)/template/14882.css \
   --css $(THIS_DIR)/pandoc.css \
   --filter pandoc-citeproc \
   --filter $(THIS_DIR)/pandoc.py \
   --filter $(MPARK_DATA)/filter/wg21.py \
   --highlight-style $(MPARK_DATA)/syntax/wg21.theme \
   --metadata datadir:$(MPARK_DATA) \
   --metadata-file $(MPARK_DATA)/metadata.yaml \
   --syntax-definition $(MPARK_DATA)/syntax/isocpp.xml \
   --template $(MPARK_DATA)/template/wg21 \
   --output $@
