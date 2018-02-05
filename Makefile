define add_paper
PAPERS += $(1)

$(1) : $(2) ./md/barry_md.py ./md/style.html
	python ./md/barry_md.py "$$<" "$$@" --style ./md/style.html			
endef

$(eval $(call add_paper,0847r0_deducing_this.html,./md/deducing-this.md))

all : $(PAPERS)
.DEFAULT_GOAL := all

.PHONY: clean
clean:
	rm -f $(PAPERS)
