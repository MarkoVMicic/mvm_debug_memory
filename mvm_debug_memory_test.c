#include "mvm_debug_memory.h"
// #include <stdlib.h>


int main(int argc, char **argv)
{

    int *array = (int *)malloc((sizeof *array) * 32);

    free(array);



    return(0);   
}