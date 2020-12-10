#include "test.h"
#include "generate-data.h"
#include "qsort-mpi.h"
#include <iostream>

void
time_testing(int sz)
{
    int *data = test_data(sz);

    double start = MPI_Wtime();
    q_sort(data, sz);
    double end = MPI_Wtime();
    double elapsed = end - start;
    std::cout << "elapsed, before & after q_sort: " << elapsed << std::endl;

    // and show resultant sorted array, just for my paranoia
    std::cout << std::endl << "Sorted data:" << std::endl;
    for (int i = 0; i < sz; ++i) {
        std::cout << data[i] << " ";
    }
    std::cout << std::endl;

    free(data);
}
