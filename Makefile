CC=mpicxx
CFLAGS=-c

all: main

main: main.o test.o qsort-mpi.o generate-data.o
	$(CC) $(LDFLAGS) main.o test.o qsort-mpi.o generate-data.o -o main

main.o: main.c
	$(CC) $(CFLAGS) main.c -o main.o

test.o: test.c
	$(CC) $(CFLAGS) test.c -o test.o

qsort-mpi.o: qsort-mpi.c
	$(CC) $(CFLAGS) qsort-mpi.c -o qsort-mpi.o

generate-data.o: generate-data.c
	$(CC) $(CFLAGS) generate-data.c -o generate-data.o

clean:
	rm -f main *.o