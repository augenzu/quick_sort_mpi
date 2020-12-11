#include "test.h"
#include "generate-data.h"
#include "qsort-mpi.h"
#include <iostream>

void
time_testing(int sz)
{
    int *data = test_data(sz);

    q_sort(data, sz);

    free(data);
}
