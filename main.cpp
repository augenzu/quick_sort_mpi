#include "test.h"
#include <iostream>
#include <cstdlib>

int
main(int argc, char **argv)
{
    int elm_cnt = std::atoi(argv[1]);

    test(elm_cnt);
    
    return 0;
}
