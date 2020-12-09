#include "qsort-mpi.h"
#include <iostream>

void
q_sort(int *data, size_t elm_cnt)
{
    MPI_Init(NULL, NULL);

    std::cout << "Am I here?" << std::endl;

    MPI_Finalize();
}