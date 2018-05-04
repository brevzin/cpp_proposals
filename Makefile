define add_paper
PAPERS += $(1)

$(1) : $(2) ./md/barry_md.py ./md/style.html
	python ./md/barry_md.py "$$<" "$$@" --style ./md/style.html			
endef

$(eval $(call add_paper,0847r1_deducing_this.html,./md/deducing-this.md))
$(eval $(call add_paper,xxxxr0_sb_extensions.html,./md/sb-extensions.md))

all : $(PAPERS)
.DEFAULT_GOAL := all

.PHONY: clean
clean:
	rm -f $(PAPERS)
