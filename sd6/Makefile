include ../md/mpark-wg21.mk

sd6.html : sd6.md
	$(PANDOC) \
    --bibliography $(THIS_DIR)wg21_fmt.yaml \
    --bibliography $(DATADIR)/index.yaml \
    --bibliography missing.yaml

sd6.md : sd6.py sd6.tmpl macros.yaml reduced.json
	python $< > $@

missing.yaml : not_yet_published.py missing_papers
	python $< > $@

reduced.json : reduce_refs.py ../md/wg21_fmt.yaml ../md/wg21/data/index.yaml reduce_refs.py missing.yaml
	python $< > $@

