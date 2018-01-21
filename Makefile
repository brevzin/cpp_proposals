all: deducing-this

TO_CLEAN:=

.PHONY: clean
clean:
	rm -f $(TO_CLEAN)

.PHONY: deducing-this
deducing-this: 0847r0_deducing_this.html

TO_CLEAN += ./0847r0_deducing_this.html
0847r0_deducing_this.html: ./md/deducing-this.md 
	python ./md/barry_md.py "$<" "$@" --style md/style.html




