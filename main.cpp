#include "test.h"
#include <iostream>
#include <cstdlib>

// TODO: cast & gather instead of sending for loops

int
main(int argc, char **argv)
{
    int elm_cnt = std::atoi(argv[1]);

    time_testing(elm_cnt);
    
    return 0;
}
