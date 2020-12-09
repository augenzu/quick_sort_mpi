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

    int comm_sz;
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    int deg = log2(comm_sz);

    int rank;
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
        std::cout << std::endl << "Start sending..." << std::endl;

        // send original array parts to other processes [1..comm_sz)
        for (size_t i = 1; i < comm_sz; ++i) {
            std::cout << "Send part " << i << "; start_index: " << shift + part_sz * (i - 1) << std::endl;
            MPI_Send(data + shift + part_sz * i, part_sz, MPI_INT, i, TAG, MPI_COMM_WORLD);
        }
    } else {
        MPI_Status status;
        // need to know incoming array size (i. e. part_sz)
        MPI_Probe(0, TAG, MPI_COMM_WORLD, &status);
        int part_sz;
        MPI_Get_count(&status, MPI_INT, &part_sz);

        // allocate memory for incoming array
        int *part_data = (int *) calloc(part_sz, sizeof(int));

        // recieve array
        MPI_Recv(part_data, part_sz, MPI_INT, 0, TAG, MPI_COMM_WORLD, &status);

        std::cout << "Array part #" << rank << ": " << part_data[0] << " .. " << part_data[part_sz - 1] << std::endl;
    }

    MPI_Finalize();
}
