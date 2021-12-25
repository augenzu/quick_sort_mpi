#include "generate-data.h"
#include "qsort-mpi.h"
#include "test.h"
#include <stdlib.h>

void
test(int sz)
{
    int *data = test_data(sz);

    q_sort(data, sz);

    free(data);
}
