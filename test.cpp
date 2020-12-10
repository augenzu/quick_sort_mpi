#include "test.h"
#include "generate-data.h"
#include "qsort-mpi.h"
#include <iostream>

// #include <cstdio>
// #include <ctime>

void
time_testing(int sz)
{
    int *data = test_data(sz);

    // struct timespec wall_start;
    // clock_gettime(CLOCK_REALTIME, &wall_start);
    // struct timespec cpu_start;
    // clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start);

    // double start = MPI_Wtime();
    q_sort(data, sz);
    // double end = MPI_Wtime();
    // double elapsed = end - start;
    // std::cout << "elapsed, before & after q_sort: " << elapsed << std::endl;

    // struct timespec wall_end;
    // clock_gettime(CLOCK_REALTIME, &wall_end);
    // struct timespec cpu_end;
    // clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end);

    // long wall_sec = wall_end.tv_sec - wall_start.tv_sec;
    // long wall_nsec = wall_end.tv_nsec - wall_start.tv_sec;
    // double wall_elapsed = wall_sec + wall_nsec * 1e-9;

    // long cpu_sec = cpu_end.tv_sec - cpu_start.tv_sec;
    // long cpu_nsec = cpu_end.tv_nsec - cpu_start.tv_sec;
    // double cpu_elapsed = cpu_sec + cpu_nsec * 1e-9;

    // std::cout << "wall_elapsed, outside q_sort: " << wall_elapsed << std::endl;
    // std::cout << "cpu_elapsed, outside q_sort: " << cpu_elapsed << std::endl;

    // and show resultant sorted array, just for my paranoia
    // std::cout << std::endl << "Sorted data:" << std::endl;
    // for (int i = 0; i < sz; ++i) {
    //     std::cout << data[i] << " ";
    // }
    // std::cout << std::endl;

    free(data);
}
