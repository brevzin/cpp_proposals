define add_paper
PAPERS += $(1)

$(1) : $(2) ./md/barry_md.py ./md/style.html
	python ./md/barry_md.py "$$<" "$$@" --style ./md/style.html			
endef

define add_better_paper
PAPERS += $(1)

$(1) : $(2) ./md/better_md.py ./md/style.html ./md/prism.*
	python ./md/better_md.py -i "$$<" -o "$$@" --references
endef

$(eval $(call add_paper,0847_deducing_this/d0847r1.html,./md/deducing-this.md))
$(eval $(call add_better_paper,1061_sb_pack/p1061r0.html,./md/sb-extensions.md))
$(eval $(call add_better_paper,overload_sets.html,./md/overload-sets.md))

all : $(PAPERS)
.DEFAULT_GOAL := all

.PHONY: clean
clean:
	rm -f $(PAPERS)
