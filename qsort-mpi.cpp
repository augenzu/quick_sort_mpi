#include "qsort-mpi.h"
#include <iostream>

// integer log2(num); works only if num == 2**deg && deg in [0, 30]
// (yes, this is a crutch but this is enough for our goals)
int
log2(int num)
{
    enum { MAX_DEG = 30 };

    int deg = MAX_DEG;
    int powered = (1 << deg);

    while (powered && (powered & num) != powered) {
        powered >>= 1;
        --deg;    
    }

    return deg;
}

void
q_sort(int *data, size_t sz)
{
    enum { TAG = 0 };
    
    MPI_Init(NULL, NULL);

    int comm_sz, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); 
    
    if (rank == 0) {
        int part_sz = sz / comm_sz;
        int shift = sz - (part_sz * comm_sz);
        std::cout << "sz: " << sz << "; comm_sz: " << comm_sz << "; part_sz: " 
                << part_sz << "; shift: " << shift << std::endl;
        std::cout << "Original array:" << std::endl;
        for (size_t i = 0; i < sz; ++i) {
            std::cout << data[i] << " ";
        }
        std::cout << std::endl << "Start initial array parts sending..." << std::endl;

        // send original array parts to other processes [1..comm_sz)
        for (size_t i = 1; i < comm_sz; ++i) {
            std::cout << "Send part " << i << "; start_index: " << shift + part_sz * i << std::endl;
            MPI_Send(data + shift + part_sz * i, part_sz, MPI_INT, i, TAG, MPI_COMM_WORLD);
        }

        // need this to work with 0's array part the same as we do this 
        // with other processes' parts
        // and to avoid memory leak
        sz = part_sz + shift;
        data = (int *) realloc(data, sz * sizeof(int));

        std::cout << "Array part #" << rank << ": " << data[0] << " .. " << data[sz - 1] << std::endl;
    } else {
        MPI_Status status;
        // need to know incoming array size (i. e. part_sz from process 0)
        MPI_Probe(0, TAG, MPI_COMM_WORLD, &status);
        int sz;
        MPI_Get_count(&status, MPI_INT, &sz);

        // allocate memory for incoming array
        int *data = (int *) calloc(sz, sizeof(int));

        // recieve array
        MPI_Recv(data, sz, MPI_INT, 0, TAG, MPI_COMM_WORLD, &status);

        std::cout << "Array part #" << rank << ": " << data[0] << " .. " << data[sz - 1] << std::endl;
    }

    int deg = log2(comm_sz);

    // main sorting cycle
    for (int i = deg; i > 0; --i) {
        int step = (1 << i);
        int mask = ~(step - 1);

        if ((rank & mask) == rank) {  // 'main' process in a 'group'
            // middle value to slit 'group' array elements by
            int middle_i = (data[0] + data[sz - 1]) / 2;
            // send middle value to other 'group' members
            for (size_t dst = rank + 1; dst < rank + step; ++dst) {
                std::cout << "LE bit: #" << i << "; I'm the main proc in group; rank: " 
                        << rank << "; orig middle_i: " << middle_i << std::endl
                        << "Start sending to other in group..." << std::endl;
                int tag = i;
                MPI_Send(&middle_i, 1, MPI_INT, dst, tag, MPI_COMM_WORLD);
            }
        } else {  // all the processes in a 'group' except of 'main'
            int middle_i = 0;
            // recieve middle value to slit array elements by
            int src = (rank & mask);
            int tag = i;
            MPI_Status status;
            MPI_Recv(&middle_i, 1, MPI_INT, src, tag, MPI_COMM_WORLD, &status);
            std::cout << "LE bit: #" << i << "; I'm not a main proc; main's rank: " 
                        << src << "; rcvd middle_i: " << middle_i << "; my rank: " << rank << std::endl;
        }
    }

    MPI_Finalize();
}
