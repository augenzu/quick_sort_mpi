#include "generate-data.h"
#include <iostream>
#include <cstdlib>

int *
test_data(int elm_cnt)
{
    int *data = (int *) calloc(elm_cnt, sizeof(int));

    std::srand(42);

    for (int i = 0; i < elm_cnt; ++i) {
        data[i] = std::rand() % 1000;  // REMOVE IT!!! IT'S FOR TESTING ONLY!
    }

    return data;
}
