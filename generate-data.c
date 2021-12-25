#include "generate-data.h"
#include <stdlib.h>

int *
test_data(int sz)
{
    int *data = (int *) calloc(sz, sizeof(int));

    std::srand(42);

    for (int i = 0; i < sz; ++i) {
        data[i] = std::rand();
    }

    return data;
}
