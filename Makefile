CC=mpicxx
CFLAGS=-c# -std=c++11
#LDFLAGS=-std=c++11

all: main

main: main.o test.o qsort-mpi.o generate-data.o
	$(CC) $(LDFLAGS) main.o test.o qsort-mpi.o generate-data.o -o main

main.o: main.cpp
	$(CC) $(CFLAGS) main.cpp -o main.o

test.o: test.cpp
	$(CC) $(CFLAGS) test.cpp -o test.o

qsort-mpi.o: qsort-mpi.cpp
	$(CC) $(CFLAGS) qsort-mpi.cpp -o qsort-mpi.o

generate-data.o: generate-data.cpp
	$(CC) $(CFLAGS) generate-data.cpp -o generate-data.o

clean:
	rm -f main test *.o