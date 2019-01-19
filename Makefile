define add_paper
PAPERS += $(1)

$(1) : $(2) ./md/barry_md.py ./md/style.html
	python ./md/barry_md.py "$$<" "$$@" --style ./md/style.html			
endef

define add_better_paper
PAPERS += $(1)/$(2)

$(1)/$(2) : $(1)/$(3) ./md/better_md.py ./md/style.html ./md/prism.*
	python ./md/better_md.py -i "$$<" -o "$$@" --references
endef

define bikeshed_paper
PAPERS += $(1)/$(2)

$(1)/$(2) : $(1)/$(3)
	bikeshed spec $$< $$@
endef

#$(eval $(call add_paper,0847_deducing_this/d0847r1.html,./md/deducing-this.md))
$(eval $(call add_better_paper,1061_sb_pack,d1061r1.html,sb-extensions.md))
$(eval $(call add_better_paper,1065_constexpr_invoke,p1065r0.html,constexpr-invoke.md))
$(eval $(call add_better_paper,1169_static_call,p1169r0.html,static-call.md))
$(eval $(call add_better_paper,1170_overload_sets,p1170r0.html,overload-sets.md))
$(eval $(call bikeshed_paper,0847_deducing_this,p0847r1.html,deducing-this.bs))

all : $(PAPERS)
.DEFAULT_GOAL := all

.PHONY: clean
clean:
	rm -f $(PAPERS)
