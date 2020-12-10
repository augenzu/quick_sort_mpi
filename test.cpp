#include "test.h"
#include "generate-data.h"
#include "qsort-mpi.h"
#include <iostream>

void
time_testing(int elm_cnt)
{
    int *data = test_data(elm_cnt);

    q_sort(data, elm_cnt);

    free(data);
}
