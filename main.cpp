#include "test.h"
#include <iostream>
#include <cstdlib>

int
main(int argc, char **argv)
{
    size_t elm_cnt = std::atol(argv[1]);

    time_testing(elm_cnt);
    
    return 0;
}
