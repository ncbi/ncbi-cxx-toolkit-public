CC=/netopt/SUNWspro6/bin/CC

et: netest.o ncbiexcp.o ncbiapp.o
	echo OK
	$(CC)  -o ne ncbiexcp.o netest.o ncbiapp.o 

netest.o: netest.cpp
	$(CC) -c netest.cpp -g \
	-DUNIX  -I. -I../include

ncbiexcp.o: ../src/ncbiexcp.cpp
	$(CC) -c ../src/ncbiexcp.cpp  -g \
	-DUNIX  -I. -I../include

ncbiapp.o: ../src/ncbiapp.cpp
	$(CC) -c ../src/ncbiapp.cpp  -g \
	-DUNIX -I. -I../include
clean: 
	rm *.o

#-DHAVE_IOSTREAM
#$(CC) -mt -o ne ncbiexcp.o netest.o ncbiapp.o -lpthread