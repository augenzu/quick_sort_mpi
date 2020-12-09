#include "test.h"
#include <iostream>
#include <string>

int
main(int argc, char **argv)
{
    size_t elm_cnt = std::stoull(argv[1]);

    time_testing(elm_cnt);
    
    return 0;
}
