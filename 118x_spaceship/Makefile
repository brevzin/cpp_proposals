all : p1186r3.html p1614r2.html p1630r1.html

include ../md/mpark-wg21.mk

p1188r0.html : spaceship-lib.md
	python ../md/better_md.py -i $< -o $@ --references

p1185r2.html : spaceship-no-eq.md
	python ../md/better_md.py -i $< -o $@ --references

p1186r3.html : spaceship-is-compare.md
	$(PANDOC)

p1189r0.html : add-spaceship-to-lib.md
	python ../md/better_md.py -i $< -o $@ --references

p1614r2.html : mothership.md
	$(PANDOC)

p1630r1.html : spaceship-tuneup.md
	$(PANDOC)
