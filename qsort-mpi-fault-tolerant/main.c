#include "generate-data.h"
#include "qsort-mpi.h"
#include <stdlib.h>

int
main(int argc, char **argv)
{
    int sz = atoi(argv[1]);

    int *data = test_data(sz);
    q_sort(data, sz);
    free(data);
    
    return 0;
}
