#include "generate-data.h"
#include <iostream>
#include <cstdlib>

int *
test_data(size_t elm_cnt)
{
    int *data = new int[elm_cnt];

    std::srand(42);

    for (size_t i = 0; i < elm_cnt; ++i) {
        data[i] = std::rand();
    }

    return data;
}