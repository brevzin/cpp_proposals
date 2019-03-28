all : p1185r2.html d1186r2.html p1188r0.html p1189r0.html d1614r1.html

p1188r0.html : spaceship-lib.md
	python ../md/better_md.py -i $< -o $@ --references

p1185r2.html : spaceship-no-eq.md
	python ../md/better_md.py -i $< -o $@ --references

d1186r2.html : spaceship-is-compare.md
	python ../md/better_md.py -i $< -o $@ --references

p1189r0.html : add-spaceship-to-lib.md
	python ../md/better_md.py -i $< -o $@ --references

d1614r1.html : mothership.md
	python ../md/better_md.py -i $< -o $@ --references

d1630r0.html : annex-c.md
	python ../md/better_md.py -i $< -o $@ --references
