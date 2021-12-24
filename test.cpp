#include "test.h"
#include "generate-data.h"
#include "qsort-mpi.h"
#include <cstdlib>

void
test(int sz)
{
    int *data = test_data(sz);

    q_sort(data, sz);

    free(data);
}
