#include "test.h"
#include "generate-data.h"
#include "qsort-mpi.h"

void
time_testing(size_t elm_cnt)
{
    int *data = test_data(elm_cnt);

    q_sort(data, elm_cnt - 1);

    delete[] data;
}