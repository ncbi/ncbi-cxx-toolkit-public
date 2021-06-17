%.1: %.tex
	latex2man -M $< $@
%.html: %.tex
	latex2man -H $< $@
%.txt: %.html
	lynx -dump $< > $@
%.dvi: %.tex
	latex $<
%.pdf: %.dvi
	-rm $@
	dvipdf $<

all: oligofar.1 oligofar.html oligofar.txt oligofar.pdf




