CC=/netopt/SUNWspro6/bin/CC

et: netest.o ncbiexcp.o
	echo OK
	$(CC) -mt -o ne ncbiexcp.o netest.o -lpthread

netest.o: netest.cpp
	$(CC) -c netest.cpp -mt -g \
	-DUNIX  -I.

ncbiexcp.o: ncbiexcp.cpp
	$(CC) -c ncbiexcp.cpp -mt -g \
	-DUNIX -I.
clean: 
	rm *.o

