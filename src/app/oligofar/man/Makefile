%.1: %.tex
	latex2man -M $< $@
%.html: %.tex
	latex2man -H $< $@
%.txt: %.html
	lynx -dump $< > $@

all: oligofar.1 oligofar.html oligofar.txt




