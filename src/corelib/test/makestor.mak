CC=/netopt/SUNWspro6/bin/CC

st: main.o xmlstore.o 
	echo OK
	$(CC)  -o st xmlstore.o main.o

main.o: main.cpp
	$(CC) -c main.cpp -g \
	-DUNIX  -I. -I../include

xmlstore.o: ../src/xmlstore.cpp
	$(CC) -c ../src/xmlstore.cpp  -g \
	-DUNIX  -I. -I../include

clean: 
	rm *.o
#