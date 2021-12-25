#include "test.h"
#include <stdlib.h>

int
main(int argc, char **argv)
{
    int elm_cnt = atoi(argv[1]);

    test(elm_cnt);
    
    return 0;
}
