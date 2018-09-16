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

define add_better_paper2
$(eval $(call add_better_paper,$(1)/$(2),$(1)/$(3)))
endef

$(eval $(call add_paper,0847_deducing_this/d0847r1.html,./md/deducing-this.md))
$(eval $(call add_better_paper2,1061_sb_pack,d1061r1.html,sb-extensions.md))
$(eval $(call add_better_paper,overload_sets.html,./md/overload-sets.md))
$(eval $(call add_better_paper,1065_constexpr_invoke/d1065r0.html,1065_constexpr_invoke/constexpr-invoke.md))
$(eval $(call add_better_paper2,1169_static_call,d1169r0.html,static-call.md))
$(eval $(call add_better_paper2,1170_compare_3way,d1170r0.html,compare-3way.md))
$(eval $(call add_better_paper2,xxxx_3way,xxxx.html,3way.md))
$(eval $(call add_better_paper2,xxxx_3way,xxxy.html,compare-redundant.md))

all : $(PAPERS)
.DEFAULT_GOAL := all

.PHONY: clean
clean:
	rm -f $(PAPERS)
