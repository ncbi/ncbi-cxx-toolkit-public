.PHONY: all install clean blast2seq.pure

all:
	cd ../../lib && make
	cd .. && make
	make -f Makefile.blast2seq_app

# Solaris only!
CXX = purify /netopt/forte6u2/bin/CC
blast2seq.pure:
	cd .. && make -f Makefile.blastxx_lib
	make APP=blast2seq.pure CXX="$(CXX)" -f Makefile.blast2seq_app

clean:
	make -f Makefile.blast2seq_app clean
	rm -fr blast2seq.pure *.out

install: blast2seq
	test -d ../../bin/ || mkdir ../../bin
	install blast2seq ../../bin
