CC=/netopt/SUNWspro6/bin/CC

et: netest.o ncbiexcp.o ncbiapp.o
	echo OK
	$(CC) -mt -o ne ncbiexcp.o netest.o ncbiapp.o -lpthread

netest.o: netest.cpp
	$(CC) -c netest.cpp -mt -g \
	-DUNIX  -I. -I../include

ncbiexcp.o: ../src/ncbiexcp.cpp
	$(CC) -c ../src/ncbiexcp.cpp -mt -g \
	-DUNIX  -I. -I../include

ncbiapp.o: ../src/ncbiapp.cpp
	$(CC) -c ../src/ncbiapp.cpp -mt -g \
	-DUNIX -I. -I../include
clean: 
	rm *.o

#-DHAVE_IOSTREAM