all:
	cd .. && make -f Makefile.blastxx_lib
	make -f Makefile.blast2seq_app

# Solaris only!
CXX = purify /netopt/forte6u2/bin/CC
pure:
	cd .. && make -f Makefile.blastxx_lib
	make APP=blast2seq.pure CXX="$(CXX)" -f Makefile.blast2seq_app

clean:
	make -f Makefile.blast2seq_app clean
	rm -fr blast2seq.pure *.out
