#include "qsort-mpi.h"
#include <iostream>

void
q_sort(int *data, size_t elm_cnt)
{
    MPI_Init(NULL, NULL);

    int size, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); 
    
    std::cout << "rank " << rank << " of size " << size << std::endl;

    std::cout << "Am I here?" << std::endl;

    MPI_Finalize();
}