all : p1185r1.html p1186r1.html p1188r0.html d1189r0.html

p1188r0.html : spaceship-lib.md
	python ../md/better_md.py -i $< -o $@ --references

p1185r1.html : spaceship-no-eq.md
	python ../md/better_md.py -i $< -o $@ --references

p1186r1.html : spaceship-is-compare.md
	python ../md/better_md.py -i $< -o $@ --references

d1189r0.html : add-spaceship-to-lib.md
	python ../md/better_md.py -i $< -o $@ --references
